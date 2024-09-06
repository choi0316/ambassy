/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>
#define MIN(x,y)		((x)<(y)?(x):(y))	
/* To the the UUID (found the the TA's h-file(s)) */
#include <hello_world_ta.h>
#include <time.h>

 *image=NULL;


int readBMP(char* filename, unsigned char **ukey, int* w, int* h){

		int i,k;
		FILE* f = fopen(filename, "rb");
		unsigned char info[54];
		
		fread(info, sizeof(unsigned char), 54, f);

		//extract image height and width from header
		int width = *(int*)&info[18];
		int height = *(int*)&info[22];
		printf("width: %d height: %d\n", width, height);
		int size = (width)*(height)*4;

		//int size = (width)*(height);

		if (f ==NULL){
			printf("image file could not be opened. Ignoring it.\n");
			return -1;
		}

		unsigned char* data;
		
		data= malloc(size);
		
		memset(data, 0,size);

		fread(data, sizeof(unsigned char), width*height*4, f);

		fclose(f);

		size--;
		for(i=width*height*4-1; i>=0; i-=4){
 			data[size--] = data[i-3]; // A
			data[size--] = data[i-2]; // B
			data[size--] = data[i-1]; //G
			data[size--] = data[i]; // R
			}


		*ukey = data;
	
		*w = width;
		*h = height;

		printf("image read success\n");

		return 0;

}

int save_image_file(char *file, unsigned char **ukey, int * ssize){

	FILE *FROMF;
	unsigned char *keyword;
	size_t size=0;
	printf("jwseo: save image file start\n");

	FROMF = fopen(file, "rb");
	if (FROMF ==NULL){
		printf("image file could not be opened. Ignoring it.\n");
		return -1;
	}
	printf("jwseo: open the image file success\n");


	fseek(FROMF, 0, SEEK_END); //set to end of file
	printf("jwseo: before ftell\n");
	size = ftell(FROMF); //get current file pointer
	printf("image size: %d\n", size);
	fseek(FROMF, 0, SEEK_SET); //seek back to beginning of file
	keyword = malloc(size+1);
	memset(keyword, 0,size+1);

	if(FROMF==NULL){
		printf("Key file none.\n");
		return -1;
	}
	
	printf("jwseo: image file open success\n");

	fread(keyword, 1, size, FROMF);

	*ukey = keyword;
    *ssize= size+1;


	//printf("%s\n", ukey);


	printf("jwseo: image file read success\n");
	return 0;
}

void image_size(){

	int iHeight=0, iWidth=0, iPos, i;
    char *cpFileName = "sample.bmp";

    FILE *fp = fopen(cpFileName,"rb");
    fseek(fp,0,SEEK_END);
    long len = ftell(fp);
    fseek(fp,0,SEEK_SET);

    unsigned char *ucpImageBuffer = (unsigned char*) malloc (len+1);
    fread(ucpImageBuffer,1,len,fp);
    fclose(fp);

    printf("\n\nBuffer size %ld", len); 

    /*Extract start of frame marker(FFCO) of width and hight and get the position*/
    for(i=0;i<len;i++)
    {
        if((ucpImageBuffer[i]==0xff) && (ucpImageBuffer[i+1]==0xC0) )
        {
            iPos=i;         
            break;
        }       
    }   

    /*Moving to the particular byte position and assign byte value to pointer variable*/
    iPos = iPos + 5;
    iHeight = ucpImageBuffer[iPos]<<8|ucpImageBuffer[iPos+1];
    iWidth = ucpImageBuffer[iPos+2]<<8|ucpImageBuffer[iPos+3];

    printf("\nWxH = %dx%d\n\n", iWidth, iHeight);   

    free(ucpImageBuffer);


}

void copy_image(unsigned char *out, unsigned char *in, int size){
	int i;
	for (i=0; i<size; i++)
		out[i] = in[i];
}


int main(int argc, char *argv[])
{

	double retDiff = 0;
	double smoothDiff = 0;
	struct timespec startTS;
	struct timespec endTS;
	struct timespec middle;
	struct timespec middle2;


	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;
	
	//jwseo
	TEEC_Operation teec_op;
	TEEC_SharedMemory inputSM;

	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	//cypbest
	char pub_key[256] = {9, };
	TEEC_SetSecureOS(&ctx, pub_key, 2048);

	unsigned char *in, *ukey;
    int w=0, h=0;

	char keyfile[8]="sample.bmp";

	// jwseo 1001
	size_t n, m;
	int	SIZE;

	TEEC_Operation img_op,start_op,login_op, create_op;

	/*
	 * Open a session to the "hello world" TA, the TA will print "hello
	 * world!" in the log when the session is created.
	 */
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);

	memset(&create_op, 0, sizeof(create_op));
	memset(&start_op, 0, sizeof(start_op));
	memset(&img_op, 0, sizeof(img_op));

	create_op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);



	/*
	 * Execute a function in the TA by invoking it, in this case
	 * we're incrementing a number.
	 *
	 * The value of command ID part and how the parameters are
	 * interpreted is part of the interface provided by the TA.
	 */

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));

	/*
	 * Prepare the argument. Pass a value in the first parameter,
	 * the remaining three parameters are unused.
	 */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].value.a = 50;

	/*
	 * TA_HELLO_WORLD_CMD_INC_VALUE is the actual function in the TA to be
	 * called.
	 */
	printf("Invoking TA to increment %d\n", op.params[0].value.a);
	res = TEEC_InvokeCommand(&sess, TA_HELLO_WORLD_CMD_INC_VALUE, &op,
				 &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			res, err_origin);
	printf("TA incremented value to %d\n", op.params[0].value.a);

	/*
	 * We're done with the TA, close the session and
	 * destroy the context.
	 *
	 * The TA will print "Goodbye!" in the log when the
	 * session is closed.
	 */

	TEEC_CloseSession(&sess);

	TEEC_FinalizeContext(&ctx);

	return 0;
}
