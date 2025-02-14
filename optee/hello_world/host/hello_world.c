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
#include <time.h>
#include <pthread.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <hello_world_ta.h>

static struct timespec t0, t1;

static long pg_get_current_time(struct timespec *ts)
{
	if (clock_gettime(CLOCK_MONOTONIC, ts) < 0) {
		perror("clock_gettime");
		exit(1);
	}
	return 0;
}

static uint64_t pg_timespec_diff_ns(struct timespec *start, struct timespec *end)
{
	uint64_t ns = 0;

	if (end->tv_nsec < start->tv_nsec) {
		ns += 1000000000 * (end->tv_sec - start->tv_sec - 1);
		ns += 1000000000 - start->tv_nsec + end->tv_nsec;
	} else {
		ns += 1000000000 * (end->tv_sec - start->tv_sec);
		ns += end->tv_nsec - start->tv_nsec;
	}

//	printf("sec : %llu\t, millisec : %llu\n", (unsigned long long)(end->tv_sec - start->tv_sec), (unsigned long long)(end->tv_nsec-start->tv_nsec)*1000000);
	printf("NANOSEC : %llu\n", ns);
	return ns;
}

static void *cancellation_thread(void *arg)
{
	/*
	 * Sleep 0.5 seconds before cancellation to make sure that the other
	 * thread is in RPC_WAIT.
	 */
	(void)usleep(500000);
	TEEC_RequestCancellation(arg);
	return NULL;
}

int main(int argc, char *argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;
	uint32_t ret_orig;
	TEEC_SharedMemory shm_in;
	uint32_t buffer[1024];
		
	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);

	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	//cypbest
	char pub_key[256] = {9, };
//	TEEC_SetSecureOS(&ctx, pub_key, 2048);
	printf("setsecure OS\n");

	/*
	 * Open a session to the "hello world" TA, the TA will print "hello
	 * world!" in the log when the session is created.
	 */

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);

	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);
	else
		printf("jwseo: opensession \n");

	
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
	op.params[0].value.a = 42;

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
	else
		printf("jwseo: invoke command\n");
	printf("TA incremented value to %d\n", op.params[0].value.a);


	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
						TEEC_NONE,
						TEEC_NONE, TEEC_NONE);



	/*
	 * We're done with the TA, close the session and
	 * destroy the context.
	 *
	 * The TA will print "Goodbye!" in the log when the
	 * session is closed.
	 */      


	TEEC_CloseSession(&sess);

//	TEEC_UnSetSecureOS(&ctx, pub_key, 2048);        
	TEEC_FinalizeContext(&ctx);

	return 0;
}
