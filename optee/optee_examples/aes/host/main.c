/*
 * Copyright (c) 2017, Linaro Limited
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
        return retDiff;
        //return retDiff/1000;
}

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <aes_ta.h>

#define AES_TEST_BUFFER_SIZE	16
#define AES_TEST_KEY_SIZE	16

#define DECODE			0
#define ENCODE			1

/* TEE resources */
struct test_ctx {
	TEEC_Context ctx;
	TEEC_Session sess;
};

void prepare_tee_session(struct test_ctx *ctx)
{
	TEEC_UUID uuid = TA_AES_UUID;
	uint32_t origin;
	TEEC_Result res;

	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx->ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	/* Open a session with the TA */
	res = TEEC_OpenSession(&ctx->ctx, &ctx->sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, origin);
}

void terminate_tee_session(struct test_ctx *ctx)
{
	TEEC_CloseSession(&ctx->sess);
	TEEC_FinalizeContext(&ctx->ctx);
}

void prepare_aes(struct test_ctx *ctx, int encode)
{
	TEEC_Operation op;
	uint32_t origin;
	TEEC_Result res;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					 TEEC_VALUE_INPUT,
					 TEEC_VALUE_INPUT,
					 TEEC_NONE);

	op.params[0].value.a = TA_AES_ALGO_ECB;
	op.params[1].value.a = TA_AES_SIZE_128BIT;
	op.params[2].value.a = encode ? TA_AES_MODE_ENCODE :
					TA_AES_MODE_DECODE;

	res = TEEC_InvokeCommand(&ctx->sess, TA_AES_CMD_PREPARE,
				 &op, &origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand(PREPARE) failed 0x%x origin 0x%x",
			res, origin);
}

void set_key(struct test_ctx *ctx, char *key, size_t key_sz)
{
	TEEC_Operation op;
	uint32_t origin;
	TEEC_Result res;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE, TEEC_NONE, TEEC_NONE);

	op.params[0].tmpref.buffer = key;
	op.params[0].tmpref.size = key_sz;

	res = TEEC_InvokeCommand(&ctx->sess, TA_AES_CMD_SET_KEY,
				 &op, &origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand(SET_KEY) failed 0x%x origin 0x%x",
			res, origin);
}

void set_iv(struct test_ctx *ctx, char *iv, size_t iv_sz)
{
	TEEC_Operation op;
	uint32_t origin;
	TEEC_Result res;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					  TEEC_NONE, TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = iv;
	op.params[0].tmpref.size = iv_sz;

	res = TEEC_InvokeCommand(&ctx->sess, TA_AES_CMD_SET_IV,
				 &op, &origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand(SET_IV) failed 0x%x origin 0x%x",
			res, origin);
}

void cipher_buffer(struct test_ctx *ctx, char *in, char *out, size_t sz)
{
	TEEC_Operation op;
	uint32_t origin;
	TEEC_Result res;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = in;
	op.params[0].tmpref.size = sz;
	op.params[1].tmpref.buffer = out;
	op.params[1].tmpref.size = sz;

	res = TEEC_InvokeCommand(&ctx->sess, TA_AES_CMD_CIPHER,
				 &op, &origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand(CIPHER) failed 0x%x origin 0x%x",
			res, origin);
}

int main(void)
{
	struct test_ctx ctx;
	char key[AES_TEST_KEY_SIZE];
	char iv[AES_TEST_KEY_SIZE];
	char clear[AES_TEST_BUFFER_SIZE];
	char ciph[AES_TEST_BUFFER_SIZE];
	char temp[AES_TEST_BUFFER_SIZE];

	printf("Prepare session with the TA\n");
	prepare_tee_session(&ctx);

	printf("Prepare encode operation\n");
	prepare_aes(&ctx, ENCODE);

	printf("Load key in TA\n");
	//memset(key, 0xa5, sizeof(key)); /* Load some dummy value */
	unsigned int aes_key[4] = {0xf3eed1bd, 0xb5d2a03c, 0x064b5a7e, 0x3db181f8};

	for (int i = 0; i<16; i++)
		key[i] = (aes_key[i/4] & (0xff << (8*(3 - i%4)))) >> (8*(3 - i%4)) ;

	set_key(&ctx, key, AES_TEST_KEY_SIZE);

	//ECB doesn't need iv
	printf("Reset ciphering operation in TA (provides the initial vector)\n");
	memset(iv, 0, sizeof(iv)); /* Load some dummy value */
	set_iv(&ctx, iv, AES_TEST_KEY_SIZE);

	printf("Encore buffer from TA\n");
	memset(clear, 0x5a, sizeof(clear)); /* Load some dummy value */


	//Assume that id/password are already entered to the TA
	long long  time_sum = 0;
	unsigned int idpw_result[4] = {0};

	for(int i = 0; i<10000; i++){
		GetTimeDiff(0);
		for(int j = 0; j<4; j++) idpw_result[j] = 0;
		cipher_buffer(&ctx, clear, ciph, AES_TEST_BUFFER_SIZE);
		for(int j = 0; j<16; j++){
			idpw_result[j/4] |= ciph[j] << (8*(3 - j%4));
		}
		time_sum += GetTimeDiff(1);
	}
	
	printf("Average latency : %lld\n", time_sum/10000);
	printf("Average latency : %lld\n", time_sum);
	
	printf("AES result : %x\n", idpw_result[0]);
	printf("AES result : %x\n", idpw_result[1]);
	printf("AES result : %x\n", idpw_result[2]);
	printf("AES result : %x\n", idpw_result[3]);
	
	// clear = plaintext
	// ciph = ciphertext

/*
	printf("Prepare decode operation\n");
	prepare_aes(&ctx, ENCODE);

	printf("Load key in TA\n");
	memset(key, 0xa5, sizeof(key));// / Load some dummy value 
	set_key(&ctx, key, AES_TEST_KEY_SIZE);

	//printf("Reset ciphering operation in TA (provides the initial vector)\n");
	//memset(iv, 0, sizeof(iv)); // Load some dummy value 
	//set_iv(&ctx, iv, AES_TEST_KEY_SIZE);

	printf("Decode buffer from TA\n");
	memset(clear, 0x5a, sizeof(clear)); // Load some dummy value 
	cipher_buffer(&ctx, ciph, temp, AES_TEST_BUFFER_SIZE);

	// Check decoded is the clear content 
	if (memcmp(clear, temp, AES_TEST_BUFFER_SIZE))
		printf("Clear text and decoded text differ => ERROR\n");
	else
		printf("Clear text and decoded text match\n");

*/	terminate_tee_session(&ctx);
	return 0;
}
