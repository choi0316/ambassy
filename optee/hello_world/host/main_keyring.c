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

void TA_login(){

	TEEC_Result res;
	TEEC_Session sess;
	TEEC_Operation op;
	uint32_t err_origin;

	char *u;

	u = (char*)malloc(20);

	printf("USER: ");
	scanf("%s", u);

	memset(&op, 0, sizeof(op));
	op.params[0].tmpref.buffer = u;
	op.params[0].tmpref.size = strlen(u)+1;


	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_NONE, TEEC_NONE, TEEC_NONE);
	res=TEEC_InvokeCommand(&sess, TA_LOGIN, &op, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "jwseo: InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

}



int main(int argc, char *argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;

	double retDiff=0;
	double smoothDiff=0;
	struct timespec startTS;
	struct timespec endTS;
	
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

	// jwseo 1001
	size_t n, m;
	int	SIZE;

	TEEC_Operation img_op,start_op;

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);

	char u[5] = "temp1";
	char u1[5];

	printf("ID: ");
	scanf("%s", u1);

	clock_gettime(CLOCK_MONOTONIC, &startTS);

	memset(&start_op, 0, sizeof(start_op));
	start_op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	start_op.params[0].tmpref.buffer = u1;
	start_op.params[0].tmpref.size = strlen(u1)+1;


	res = TEEC_InvokeCommand(&sess, TA_LOGIN, &start_op, &err_origin);
		if (res != TEEC_SUCCESS)
			errx(1, "jwseo(ukey1): InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

	clock_gettime(CLOCK_MONOTONIC, &endTS);

    retDiff = (endTS.tv_sec - startTS.tv_sec);
    retDiff += (endTS.tv_nsec - startTS.tv_nsec) / 1000000000.0;
    printf("%f microseconds\n", retDiff * 1000.0);



	TEEC_CloseSession(&sess);

	TEEC_FinalizeContext(&ctx);

	return 0;
}
