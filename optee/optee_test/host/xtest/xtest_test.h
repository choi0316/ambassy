/*
 * Copyright (c) 2016, Linaro Limited
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

#ifndef XTEST_TEST_H
#define XTEST_TEST_H
#include <adbg.h>
#include <tee_client_api.h>

ADBG_SUITE_DECLARE(benchmark);
#ifdef WITH_GP_TESTS
ADBG_SUITE_DECLARE(gp);
#endif
ADBG_SUITE_DECLARE(regression);
//jwseo
ADBG_SUITE_DECLARE(jwseo);
ADBG_SUITE_DECLARE(notrelated);
ADBG_SUITE_DECLARE(notworking);


/* TEEC_Result */
ADBG_ENUM_TABLE_DECLARE(TEEC_Result);

#define ADBG_EXPECT_TEEC_RESULT(c, exp, got) \
	ADBG_EXPECT_ENUM(c, exp, got, ADBG_EnumTable_TEEC_Result)

#define ADBG_EXPECT_TEEC_SUCCESS(c, got) \
	ADBG_EXPECT_ENUM(c, TEEC_SUCCESS, got, ADBG_EnumTable_TEEC_Result)

/* TEEC_ErrorOrigin */
ADBG_ENUM_TABLE_DECLARE(TEEC_ErrorOrigin);

#define ADBG_EXPECT_TEEC_ERROR_ORIGIN(c, exp, got) \
	ADBG_EXPECT_ENUM(c, exp, got, ADBG_EnumTable_TEEC_ErrorOrigin)

/* bass_return_code */
ADBG_ENUM_TABLE_DECLARE(bass_return_code);

#define ADBG_EXPECT_BASS_RETURN_CODE(c, exp, got) \
	ADBG_EXPECT_ENUM(c, exp, got, ADBG_EnumTable_bass_return_code)

#define ADBG_EXPECT_BASS_RC_SUCCESS(c, got) \
	ADBG_EXPECT_ENUM(c, BASS_RC_SUCCESS, got, \
			 ADBG_EnumTable_bass_return_code)


extern const char crypt_user_ta[];
extern const unsigned int crypt_user_ta_size;

extern const char os_test_ta[];
extern const unsigned int os_test_ta_size;

extern const char create_fail_test_ta[];
extern const unsigned int create_fail_test_ta_size;

extern const char rpc_test_ta[];
extern const unsigned int rpc_test_ta_size;

extern const char sims_test_ta[];
extern const unsigned int sims_test_ta_size;

extern const char gp_tta_testing_client_api_ta[];
extern const unsigned int gp_tta_testing_client_api_ta_size;

extern const char gp_tta_answer_success_to_open_session_invoke_ta[];
extern const unsigned int gp_tta_answer_success_to_open_session_invoke_ta_size;

extern const char gp_tta_answer_error_to_invoke_ta[];
extern const unsigned int gp_tta_answer_error_to_invoke_ta_size;

extern const char gp_tta_answer_error_to_open_session_ta[];
extern const unsigned int gp_tta_answer_error_to_open_session_ta_size;

extern const char gp_tta_check_open_session_with_4_parameters_ta[];
extern const unsigned int gp_tta_check_open_session_with_4_parameters_ta_size;

extern const char gp_tta_ds_ta[];
extern const unsigned int gp_tta_ds_ta_size;

extern const char storage_ta[];
extern const unsigned int storage_ta_size;

extern const char gp_tta_time_ta[];
extern const unsigned int gp_tta_time_ta_size;

extern const char gp_tta_tcf_ta[];
extern const unsigned int gp_tta_tcf_ta_size;

extern const char gp_tta_crypto_ta[];
extern const unsigned int gp_tta_crypto_ta_size;

extern const char gp_tta_arithm_ta[];
extern const unsigned int gp_tta_arithm_ta_size;

extern const char gp_tta_ica_ta[];
extern const unsigned int gp_tta_ica_ta_size;

extern const char gp_tta_ica2_ta[];
extern const unsigned int gp_tta_ica2_ta_size;

extern const char gp_tta_tcf_singleinstance_ta[];
extern const unsigned int gp_tta_tcf_singleinstance_ta_size;

extern const char gp_tta_tcf_multipleinstance_ta[];
extern const unsigned int gp_tta_tcf_multipleinstance_ta_size;

extern const TEEC_UUID crypt_user_ta_uuid;
extern const TEEC_UUID os_test_ta_uuid;
extern const TEEC_UUID create_fail_test_ta_uuid;
extern const TEEC_UUID rpc_test_ta_uuid;
extern const TEEC_UUID sims_test_ta_uuid;
extern const TEEC_UUID gp_tta_testing_client_api_uuid;
extern const TEEC_UUID gp_tta_answer_success_to_open_session_invoke_uuid;
extern const TEEC_UUID gp_tta_answer_error_to_invoke_uuid;
extern const TEEC_UUID gp_tta_answer_error_to_open_session_uuid;
extern const TEEC_UUID gp_tta_check_OpenSession_with_4_parameters_uuid;
extern const TEEC_UUID gp_tta_ds_uuid;
extern const TEEC_UUID storage_ta_uuid;
extern const TEEC_UUID storage2_ta_uuid;
extern const TEEC_UUID enc_fs_key_manager_test_ta_uuid;
extern const TEEC_UUID ecc_test_ta_uuid;
extern const TEEC_UUID pta_invoke_tests_ta_uuid;
extern const TEEC_UUID gp_tta_time_uuid;
extern const TEEC_UUID concurrent_ta_uuid;
extern const TEEC_UUID concurrent_large_ta_uuid;
extern const TEEC_UUID storage_benchmark_ta_uuid;
extern const TEEC_UUID socket_ta_uuid;
extern const TEEC_UUID sdp_basic_ta_uuid;
extern char *_device;

#endif /*XTEST_TEST_H*/
