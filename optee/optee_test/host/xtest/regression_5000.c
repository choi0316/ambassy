/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "xtest_test.h"
#include "xtest_helpers.h"
#include "tee_api_defines.h"
#include "tee_client_api.h"

#define OFFSET0 0

#define PARAM_0 0
#define PARAM_1 1
#define PARAM_2 2
#define PARAM_3 3

struct xtest_session {
	ADBG_Case_t *c;
	TEEC_Session session;
	TEEC_Context context;
};

/* Compares two memories and checks if their length and content is the same */
#define EXPECT_SHARED_MEM_BUFFER(c, exp_buf, exp_blen, op, param_num, shrm) \
	do { \
		if ((exp_buf) == NULL) { \
			ADBG_EXPECT((c), exp_blen, \
				    (op)->params[(param_num)].memref.size); \
		} else { \
			ADBG_EXPECT_COMPARE_POINTER((c), (shrm), ==, \
			    (op)->params[(param_num)].memref.parent); \
			ADBG_EXPECT_BUFFER((c), (exp_buf), (exp_blen), \
			   (shrm)->buffer, \
			   (op)->params[(param_num)].memref.size); \
		} \
	} while (0)

/*
 * Compares the content of the memory cells in OP with the expected value
 * contained.
 */
#define EXPECT_OP_TMP_MEM_BUFFER(c, exp_buf, exp_blen, op, param_num, buf) \
	do { \
		if ((exp_buf) == NULL) { \
			ADBG_EXPECT((c), exp_blen, \
			    (op)->params[(param_num)].tmpref.size); \
		} else { \
			ADBG_EXPECT_COMPARE_POINTER((c), (buf), ==, \
			    (op)->params[(param_num)].tmpref.buffer); \
			ADBG_EXPECT_BUFFER((c), (exp_buf), (exp_blen), \
			   (buf), \
			   (op)->params[(param_num)].memref.size); \
		} \
	} while (0)

static void nothing(void)
{
	return;
}

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



/* Registers the TEEC_SharedMemory to the TEE. */
static TEEC_Result RegisterSharedMemory(TEEC_Context *ctx,
					TEEC_SharedMemory *shm, size_t size,
					uint32_t flags)
{
	shm->flags = flags;
	shm->size = size;
	return TEEC_RegisterSharedMemory(ctx, shm);
}

/* Allocates shared memory inside of the TEE. */
static TEEC_Result AllocateSharedMemory(TEEC_Context *ctx,
					TEEC_SharedMemory *shm, size_t size,
					uint32_t flags)
{
	shm->flags = flags;
	shm->size = size;
	return TEEC_AllocateSharedMemory(ctx, shm);
}

static void CloseSession_null(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "CloseSession_null");
	{
		pg_get_current_time(&t0);
		/* In reality doesn't test anything. */
		TEEC_CloseSession(NULL);
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	}
	Do_ADBG_EndSubCase(cs->c, "CloseSession_null");
}

static void Allocate_In(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Allocate_In");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		size_t size = 1024;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm, size,
					     TEEC_MEM_INPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Allocate_In");
}

static void Allocate_out_of_memory(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Allocate_out_of_memory");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		size_t SIZE_OVER_MEMORY_CAPACITY = SIZE_MAX;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		ADBG_EXPECT_TEEC_RESULT(cs->c, TEEC_ERROR_OUT_OF_MEMORY,
			AllocateSharedMemory(&cs->context, &shm,
					     SIZE_OVER_MEMORY_CAPACITY,
					     TEEC_MEM_INPUT));
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Allocate_out_of_memory");
}

static void OpenSession_error_notExistingTA(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "OpenSession_error_notExistingTA");
	{
		pg_get_current_time(&t0);
		TEEC_UUID NONEXISTING_TA_UUID = { 0x534D1192, 0x6143, 0x234C,
						  { 0x47, 0x55, 0x53, 0x52,
						    0x54, 0x4F, 0x4F, 0x59 } };
		uint32_t ret_orig;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		ADBG_EXPECT_COMPARE_UNSIGNED(cs->c, TEEC_SUCCESS, !=,
			TEEC_OpenSession(&cs->context, &cs->session,
					 &NONEXISTING_TA_UUID,
					 TEEC_LOGIN_PUBLIC, NULL, NULL,
					 &ret_orig));
		ADBG_EXPECT_COMPARE_UNSIGNED(cs->c, TEEC_ORIGIN_TRUSTED_APP, !=,
					     ret_orig);
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "OpenSession_error_notExistingTA");
}

static void Allocate_InOut(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Allocate_InOut");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		uint8_t val[] = { 54, 76, 98, 32 };

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Allocate_InOut");
}

static void Register_In(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Register_In");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		uint8_t val[] = { 32, 65, 43, 21, 98 };

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_INPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Register_In");
}

static void Register_notZeroLength_Out(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Register_notZeroLength_Out");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		uint8_t val[] = { 56, 67, 78, 99 };

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_OUTPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:	
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Register_notZeroLength_Out");
}

static void Register_InOut(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Register_InOut");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		uint8_t val[] = { 54, 76, 23, 98, 255, 23, 86 };

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;
		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Register_InOut");
}

static void Register_zeroLength_Out(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Register_zeroLength_Out");
	{
		pg_get_current_time(&t0);
		uint8_t val[] = { 65, 76, 98, 32 };
		TEEC_SharedMemory shm;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;
		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, 0,
					     TEEC_MEM_OUTPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Register_zeroLength_Out");
}

static void Allocate_Out(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Allocate_Out");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm, 0,
					     TEEC_MEM_OUTPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Allocate_Out");
}

static void FinalizeContext_null(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "FinalizeContext_null");
	{
		pg_get_current_time(&t0);
		TEEC_FinalizeContext(NULL);
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);

	}
	Do_ADBG_EndSubCase(cs->c, "FinalizeContext_null");
}

static void InitializeContext_NotExistingTEE(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "InitializeContext_NotExistingTEE");
	{
		pg_get_current_time(&t0);
		if (!ADBG_EXPECT_COMPARE_UNSIGNED(cs->c, TEEC_SUCCESS, !=,
			TEEC_InitializeContext("Invalid TEE name",
					       &cs->context)))
			TEEC_FinalizeContext(&cs->context);
		pg_get_current_time(&t1);
		pg_timespec_diff_ns(&t0, &t1);
	}
	Do_ADBG_EndSubCase(cs->c, "InitializeContext_NotExistingTEE");
}

static void AllocateThenRegister_SameMemory(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "AllocateThenRegister_SameMemory");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		size_t size_allocation = 32;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm,
					     size_allocation, TEEC_MEM_INPUT)))
			goto out_final;

		ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm,
					     size_allocation, TEEC_MEM_INPUT));

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "AllocateThenRegister_SameMemory");
}

static void AllocateSameMemory_twice(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "AllocateSameMemory_twice");
	{
		pg_get_current_time(&t0);
		TEEC_SharedMemory shm;
		size_t size_allocation = 32;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm,
					     size_allocation, TEEC_MEM_INPUT)))
			goto out_final;

		ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm,
					     size_allocation, TEEC_MEM_INPUT));

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "AllocateSameMemory_twice");
}

static void RegisterSameMemory_twice(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "RegisterSameMemory_twice");
	{
		pg_get_current_time(&t0);
		uint8_t val[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
		TEEC_SharedMemory shm;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;
		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_INPUT)))
			goto out_final;

		ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, sizeof(val),
					     TEEC_MEM_INPUT));

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "RegisterSameMemory_twice");
}

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

static void Allocate_sharedMemory_32k(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Allocate_sharedMemory_32k");
	{
		pg_get_current_time(&t0);
		size_t size = MIN(32 * 1024,
				  TEEC_CONFIG_SHAREDMEM_MAX_SIZE);
		TEEC_SharedMemory shm;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			AllocateSharedMemory(&cs->context, &shm, size,
					     TEEC_MEM_INPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);
	}
out:
	//nothing();
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Allocate_sharedMemory_32k");
}

static void Register_sharedMemory_32k(struct xtest_session *cs)
{
	Do_ADBG_BeginSubCase(cs->c, "Register_sharedMemory_32k");
	{
		pg_get_current_time(&t0);
		size_t size = MIN(32 * 1024,
				  TEEC_CONFIG_SHAREDMEM_MAX_SIZE);
		uint8_t val[size];
		TEEC_SharedMemory shm;

		if (!ADBG_EXPECT(cs->c, TEEC_SUCCESS,
			TEEC_InitializeContext(_device, &cs->context)))
			goto out;

		shm.buffer = val;
		if (!ADBG_EXPECT_TEEC_SUCCESS(cs->c,
			RegisterSharedMemory(&cs->context, &shm, size,
					     TEEC_MEM_INPUT)))
			goto out_final;

		TEEC_ReleaseSharedMemory(&shm);
out_final:
		TEEC_FinalizeContext(&cs->context);

	}
out:
	//nothing();
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

	Do_ADBG_EndSubCase(cs->c, "Register_sharedMemory_32k");
}

static void xtest_teec_TEE(ADBG_Case_t *c)
{
	struct xtest_session connection = { c };

	//pg_get_current_time(&t0);
	CloseSession_null(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Allocate_In(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Allocate_out_of_memory(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	OpenSession_error_notExistingTA(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Allocate_InOut(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Register_In(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Register_notZeroLength_Out(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Register_InOut(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Register_zeroLength_Out(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Allocate_Out(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	FinalizeContext_null(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	InitializeContext_NotExistingTEE(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	AllocateThenRegister_SameMemory(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	AllocateSameMemory_twice(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	RegisterSameMemory_twice(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Allocate_sharedMemory_32k(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

	//pg_get_current_time(&t0);
	Register_sharedMemory_32k(&connection);
	//pg_get_current_time(&t1);
	//pg_timespec_diff_ns(&t0, &t1);

}

ADBG_CASE_DEFINE(regression, 5006, xtest_teec_TEE,
		"Tests for Global platform TEEC");
