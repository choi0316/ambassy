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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <hello_world_ta.h>
#include <time.h>


long long GetTimeDiff(unsigned int nFlag)
{
	    const long long NANOS = 1000000000LL;
	    static struct timespec startTS, endTS;
	    static long long retDiff = 0;
	    if(nFlag == 0)
	    {
	            retDiff = 0;
	            if(-1 == clock_gettime(CLOCK_MONOTONIC, &startTS))
			    printf("Failed to call clock_gettime\n");
				        }
		    else
		        {
			        if(-1 == clock_gettime(CLOCK_MONOTONIC, &endTS))
				printf("Failed to call clock_gettime\n");
		        retDiff = NANOS * (endTS.tv_sec-startTS.tv_sec) + (endTS.tv_nsec-startTS.tv_nsec);
    }
        return retDiff/1000;
}


int main(int argc, char* argv[])
{
	TEEC_Result res;
	TEEC_SharedMemory buf_shm = {
		.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
		.buffer = NULL,
		.size = 64
	};
	TEEC_SharedMemory buf_shm2 = {
		.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
		.buffer = NULL,
		.size = 64
	};
	const char *src = "HOST";
	char *bitstream_buffer = 0;
	
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;

	if(argc != 3) {
	          printf("Usage: tee_embassy load/unload app_name\ne.g.) tee_embassy load safeLogin");
	          return 1;
	}
	else if (strcmp(argv[1], "disconnected") == 0){ 
		//This part is used for setting the configure regsters
		//after booting (automatically called by script)
		
		res = TEEC_InitializeContext(NULL, &ctx);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
		
		res = TEEC_OpenSession(&ctx, &sess, &uuid,
				       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
		if (res != TEEC_SUCCESS)
			errx(1, "TEEC_Opensession failed with --code 0x%x origin 0x%x",
				res, err_origin);
		
		memset(&op, 0, sizeof(op));

		op.params[0].value.a = 0;

		res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_REG_DISCONNECTED, &op,
				 &err_origin);
		TEEC_CloseSession(&sess);
		TEEC_FinalizeContext(&ctx);

		printf("Register disconnected configure=====\n");
		return 0;
	}

	int memfd;
	char *mapped_base;
	int mapped_length;
	struct stat sb;
	
//	FILE *fp = fopen(argv[1], "rb");

	int i=0, j=0;
        int bit_size = 0;
        unsigned int nonce;


	GetTimeDiff(0);

	char *filename1 = "partial/config_";
	char *filename2_load = "_partial.bin";
	char *filename2_unload = "_partial_clear.bin";

	char *bin_name; 

	if(!strcmp(argv[1], "load")){
		bin_name = malloc(sizeof(char) * (strlen(filename1) + strlen(argv[2]) + strlen(filename2_load) + 1));
		strcpy(bin_name, filename1);
		strcat(bin_name, argv[2]);
		strcat(bin_name, filename2_load);
	}
	if(!strcmp(argv[1], "unload")){
		bin_name = malloc(sizeof(char) * (strlen(filename1) + strlen(argv[2]) + strlen(filename2_unload) + 1));
		strcpy(bin_name, filename1);
		strcat(bin_name, argv[2]);
		strcat(bin_name, filename2_unload);
	}

	
	memfd = open(bin_name, O_RDONLY);


	if(memfd == -1) {
		printf("Can't open the file! %s\n", strerror(errno));
		exit(0);
	}
	if(fstat(memfd, &sb) < 0){
		printf("fstat error\n");
		exit(0);
	}
	mapped_length = sb.st_size;
	mapped_base = mmap(0, mapped_length, PROT_READ, MAP_SHARED, memfd, 0);
	printf("mapped address is %x, size: %d\n", mapped_base, mapped_length);

	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	/*
	 * Open a session to the "hello world" TA, the TA will print "hello
	 * world!" in the log when the session is created.
	 */
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with --code 0x%x origin 0x%x",
			res, err_origin);
	
	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));

	printf("=======================whole TA load init time is %d\n", GetTimeDiff(1));
	/*
	 * Prepare the argument. Pass a value in the first parameter,
	 * the remaining three parameters are unused.
	 */
	
	/* generate a nonce (which has to be generated by the server)*/
	srand(time(NULL));
        nonce = rand();
	printf("Nonce is %x\n", nonce);
	char expected[32] = {0}; //this value must be the expected sha1 result from the server
	
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_VALUE_INOUT,
					 TEEC_MEMREF_WHOLE, TEEC_NONE);
	/* Attestation of the bitstream */
	GetTimeDiff(0)	;

	/* allocate shared buffer between host and TA */
	/* one for nonce+bitstream string, one for the sha1 result */
	printf("Allocate memory 1\n");
	buf_shm.size = mapped_length + 4;
	buf_shm.buffer = calloc(mapped_length + 4, sizeof(char));
	buf_shm2.size = 32; //sha1 result size
	buf_shm2.buffer = calloc(32, sizeof(char));

	op.params[0].memref.parent = &buf_shm;
	op.params[1].value.a = mapped_length + 4;
	op.params[2].memref.parent = &buf_shm2;
	
	*(char*)(buf_shm.buffer + 0) = (char)(0xff & nonce);
	*(char*)(buf_shm.buffer + 1) = (char)((0xff00 & nonce) >> 8);
	*(char*)(buf_shm.buffer + 2) = (char)((0xff0000 & nonce) >> 16);
	*(char*)(buf_shm.buffer + 3) = (char)((0xff000000 & nonce) >> 24);
	
	printf("Allocate memory 2\n");
	memcpy(buf_shm.buffer + 4, mapped_base, mapped_length);

	printf("regisetrer sahred memory\n");
	/* register buffer as shared memory */
	if(TEEC_RegisterSharedMemory(&ctx, &buf_shm) != TEEC_SUCCESS){
		printf("reigster fail..\n");
		free(buf_shm.buffer);
	}
	printf("Allocate shared memory2\n");
	if(TEEC_RegisterSharedMemory(&ctx, &buf_shm2) != TEEC_SUCCESS){
		printf("reigster fail..\n");
		free(buf_shm2.buffer);
	}



	printf("Invoking TA to attest the bitstream.\n");
	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_ATTEST, &op,
				 &err_origin);

	printf("======================Attestation time is %d\n", GetTimeDiff(1));
	for (j = 0; j<32; j++){
		if(((char *)(buf_shm2.buffer))[j] != expected[j]){
			printf("attestation failed %x\n", ((char*)(buf_shm2.buffer))[j]);
		//	break;
		}
	}
	free(buf_shm.buffer);
	free(buf_shm2.buffer);

	printf("Invoking TA to check registers==========\n");
	GetTimeDiff(0);
	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_REG_CHECK, &op,
		 &err_origin);
	
	printf("=======================Check registers time is %d\n", GetTimeDiff(1));
	

        /*     Set Acesses which TA can use   */
	printf("====================Setting access  \n");
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_VALUE_INOUT,
					 TEEC_VALUE_INOUT, TEEC_VALUE_INOUT);

	char *conf_name; 
	char *filename_conf1 = "access_config/";
	char *filename_conf2 = ".conf";

	conf_name = malloc(sizeof(char) * (strlen(filename_conf1) + strlen(argv[2]) + strlen(filename_conf2) + 1));
	strcpy(conf_name, filename_conf1);
	strcat(conf_name, argv[2]);
	strcat(conf_name, filename_conf2);
	

	FILE *fp = fopen(conf_name, "r");
	char type[20], number[20], base[20], size[20];
	int number_h, base_h, size_h;
	while(fscanf(fp, "%s %s %s %s\n", type, number, base, size) != EOF){
		printf("%s, %s, %s, %s\n", type, number, base, size); 
		if(!strcmp(type, "ACCESS")){
			printf("type is Access\n");
			number_h = strtol(number, NULL, 16);
			base_h = strtol(base, NULL, 16);
			size_h = strtol(size, NULL, 16);
			printf("hex: %x, %x, %x\n", number_h, base_h, size_h);
				 
			op.params[0].value.a = 0x0;
			op.params[1].value.a = number_h;
			op.params[2].value.a = base_h;
			op.params[3].value.a = size_h;
			res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_SET_ICAP, &op,
				 &err_origin);
		}
		else if(!strcmp(type, "EXCEPTION")){
			printf("type is Exception\n");
			number_h = strtol(number, NULL, 16);
			base_h = strtol(base, NULL, 16);
			printf("hex: %x, %x, %x\n", number_h, base_h, size_h);
				 
			op.params[0].value.a = 0x1;
			op.params[1].value.a = number_h;
			op.params[2].value.a = base_h;
			res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_SET_ICAP, &op,
				 &err_origin);
		}
	}
	
        /*       Load TA bitstream via ICAP       */
	
	printf("Invoking TA to load bitstream \n");
	GetTimeDiff(0);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE, TEEC_VALUE_INOUT,
					 TEEC_MEMREF_WHOLE, TEEC_NONE);
	buf_shm.size = mapped_length;
	buf_shm.buffer = calloc(mapped_length, sizeof(char));
	buf_shm2.size = 8;
	buf_shm2.buffer = calloc(8, sizeof(int));

	
	op.params[0].memref.parent = &buf_shm;
	op.params[1].value.a = mapped_length;
	op.params[2].memref.parent = &buf_shm2;
	
	memcpy(buf_shm.buffer, mapped_base, mapped_length);

	
	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_WRITE_ICAP_TRANSMIT, &op,
		 &err_origin);
	
	printf("=======================whole TA load time is %d\n", GetTimeDiff(1));
	free(buf_shm.buffer);

//	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_SET_ICAP, &op,
//				 &err_origin);
//	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_WRITE_ICAP_SETUP, &op,
//				 &err_origin);
//	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_WRITE_ICAP_RESET, &op,
//				 &err_origin);
//	res = TEEC_InvokeCommand(&sess, TA_EMBASSY_CMD_UNSET_ICAP, &op,
//				 &err_origin);
	
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			res, err_origin);
	

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
