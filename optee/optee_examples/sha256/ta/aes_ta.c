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
#include <inttypes.h>

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <aes_ta.h>
#include <string.h>

#define AES128_KEY_BIT_SIZE		128
#define AES128_KEY_BYTE_SIZE		(AES128_KEY_BIT_SIZE / 8)
#define AES256_KEY_BIT_SIZE		256
#define AES256_KEY_BYTE_SIZE		(AES256_KEY_BIT_SIZE / 8)
/*
 * Ciphering context: each opened session relates to a cipehring operation.
 * - configure the AES flavour from a command.
 * - load key from a command (here the key is provided by the REE)
 * - reset init vector (here IV is provided by the REE)
 * - cipher a buffer frame (here input and output buffers are non-secure)
 */
struct aes_cipher {
	uint32_t algo;			/* AES flavour */
	uint32_t mode;			/* Encode or decode */
	uint32_t key_size;		/* AES key size in byte */
	TEE_OperationHandle op_handle;	/* AES ciphering operation */
	TEE_ObjectHandle key_handle;	/* transient object to load the key */
};

/*
 * Few routines to convert IDs from TA API into IDs from OP-TEE.
 */
static TEE_Result ta_hmac_sha256(void __unused *session, uint32_t __unused param_types,
				  TEE_Param __unused params[4])
{
	TEE_Result ret;
	TEE_ObjectHandle hmac_key = (TEE_ObjectHandle)NULL;
	TEE_OperationHandle hmac_handle = (TEE_OperationHandle)NULL;
	
	size_t key_size = 256; //same with 
	uint32_t alg = TEE_ALG_HMAC_SHA256;
	
	const char *bracket_start = "{";
	const char *bracket_end = "}";
	
	const char *comma = ",";
	const char *alg_ = "\"alg\"";
	const char *HS256 = "\"HS256\"";
	const char *typ = "\"typ\"";
	const char *JWT = "\"JWT\"";
	const char *page = "\"page\"";
	const char *id = "\"id\"";
	const char *input_id = "idid"; //entered before this function
	const char *input_page = "page";

	uint32_t mac_len = 256;
	size_t msg_len = 512;

	void *mac = NULL;
	void *msg = NULL;
  	
	int64_t virtual_timer_value1;
	int64_t virtual_timer_value2;

        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	DMSG("=============================first count %ld\n", virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	mac = TEE_Malloc(mac_len, 0);
	msg = TEE_Malloc(msg_len, 0);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= Malloc%ld\n", virtual_timer_value2- virtual_timer_value1);

        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	TEE_MemMove(msg, bracket_start, 1);
	TEE_MemMove((void*)((uint64_t)msg+2), alg_, 5);
	TEE_MemMove((void*)((uint64_t)msg+10), HS256, 7);
	TEE_MemMove((void*)((uint64_t)msg+24), comma, 1);
	TEE_MemMove((void*)((uint64_t)msg+26), typ, 5);
	TEE_MemMove((void*)((uint64_t)msg+36), JWT, 5);
	TEE_MemMove((void*)((uint64_t)msg+46), bracket_end, 1);

	TEE_MemMove((void*)((uint64_t)msg+48), bracket_start, 1);
	TEE_MemMove((void*)((uint64_t)msg+60), page, 6);
	TEE_MemMove((void*)((uint64_t)msg+68), input_page, 4);
	TEE_MemMove((void*)((uint64_t)msg+70), comma, 1);
	TEE_MemMove((void*)((uint64_t)msg+79), id, 4);
	TEE_MemMove((void*)((uint64_t)msg+86), input_id, 4);
	TEE_MemMove((void*)((uint64_t)msg+100), bracket_end, 1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= MemMove%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	ret = TEE_AllocateTransientObject(TEE_TYPE_HMAC_SHA256, key_size, &hmac_key);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= AllocTransient%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	ret = TEE_GenerateKey(hmac_key, key_size, (TEE_Attribute *)NULL, 0);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= Generate%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	ret = TEE_AllocateOperation(&hmac_handle, alg, TEE_MODE_MAC, key_size);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= Alloc OP%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	ret = TEE_SetOperationKey(hmac_handle, hmac_key);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= operation key%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	TEE_MACInit(hmac_handle, NULL, 0);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= macinit %ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	TEE_MACUpdate(hmac_handle, msg, msg_len);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= hmac handle%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	ret = TEE_MACComputeFinal(hmac_handle, NULL, 0, mac, &mac_len);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= maccompute%ld\n", virtual_timer_value2- virtual_timer_value1);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value1));	
	TEE_FreeTransientObject(hmac_key);
	TEE_FreeOperation(hmac_handle);
	TEE_Free(mac);
	TEE_Free(msg);
        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= free%ld\n", virtual_timer_value2- virtual_timer_value1);

        asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value2));	
	DMSG("============================= last count%ld\n", virtual_timer_value2);
	return ret;
}
/*
 * Process command TA_AES_CMD_SET_KEY. API in aes_ta.h
 */
static TEE_Result set_aes_key(void *session, uint32_t param_types,
				TEE_Param params[4])
{
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);
	struct aes_cipher *sess;
	TEE_Attribute attr;
	TEE_Result res;
	uint32_t key_sz;
	char *key;

	/* Get ciphering context from session ID */
	DMSG("Session %p: load key material===========", session);
	sess = (struct aes_cipher *)session;

	/* Safely get the invocation parameters */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	key = params[0].memref.buffer;
	key_sz = params[0].memref.size;

	if (key_sz != sess->key_size) {
		EMSG("Wrong key size %" PRIu32 ", expect %" PRIu32 " bytes",
		     key_sz, sess->key_size);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/*
	 * Load the key material into the configured operation
	 * - create a secret key attribute with the key material
	 *   TEE_InitRefAttribute()
	 * - reset transient object and load attribute data
	 *   TEE_ResetTransientObject()
	 *   TEE_PopulateTransientObject()
	 * - load the key (transient object) into the cihering operation
	 *   TEE_SetOperationKey()
	 *
	 * TEE_SetOperationKey() requires operation to be in "initial state".
	 * We can use TEE_ResetOperation() to reset the operation but this
	 * api cannot be used on operation with key(s) not yet set. Hence,
	 * when allocating the operation handle, we prevovision a dummy key.
	 * Thus, set_key sequence always reset then set key on operation.
	 */

	TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key, key_sz);

	TEE_ResetTransientObject(sess->key_handle);
	res = TEE_PopulateTransientObject(sess->key_handle, &attr, 1);
	if (res != TEE_SUCCESS) {
		EMSG("TEE_PopulateTransientObject failed, %x", res);
		return res;
	}

	TEE_ResetOperation(sess->op_handle);
	res = TEE_SetOperationKey(sess->op_handle, sess->key_handle);
	if (res != TEE_SUCCESS) {
		EMSG("TEE_SetOperationKey failed %x", res);
		return res;
	}

	return res;
}

/*
 * Process command TA_AES_CMD_SET_IV. API in aes_ta.h
 */
static TEE_Result reset_aes_iv(void *session, uint32_t param_types,
				TEE_Param params[4])
{
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);
	struct aes_cipher *sess;
	size_t iv_sz;
	char *iv;

	/* Get ciphering context from session ID */
	DMSG("Session %p: reset initial vector", session);
	sess = (struct aes_cipher *)session;

	/* Safely get the invocation parameters */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	iv = params[0].memref.buffer;
	iv_sz = params[0].memref.size;

	/*
	 * Init cipher operation with the initialization vector.
	 */
	TEE_CipherInit(sess->op_handle, iv, iv_sz);

	return TEE_SUCCESS;
}

/*
 * Process command TA_AES_CMD_CIPHER. API in aes_ta.h
 */
static TEE_Result cipher_buffer(void *session, uint32_t param_types,
				TEE_Param params[4])
{
	const uint32_t exp_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				TEE_PARAM_TYPE_MEMREF_OUTPUT,
				TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);
	struct aes_cipher *sess;

	/* Get ciphering context from session ID */
	//DMSG("Session %p: cipher buffer", session);
	sess = (struct aes_cipher *)session;

	/* Safely get the invocation parameters */
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	if (params[1].memref.size < params[0].memref.size) {
		EMSG("Bad sizes: in %d, out %d", params[0].memref.size,
						 params[1].memref.size);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (sess->op_handle == TEE_HANDLE_NULL)
		return TEE_ERROR_BAD_STATE;

	/*
	 * Process ciphering operation on provided buffers
	 */
	return TEE_CipherUpdate(sess->op_handle,
				params[0].memref.buffer, params[0].memref.size,
				params[1].memref.buffer, &params[1].memref.size);
}

TEE_Result TA_CreateEntryPoint(void)
{
	/* Nothing to do */
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	/* Nothing to do */
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
					TEE_Param __unused params[4],
					void __unused **session)
{
	struct aes_cipher *sess;

	/*
	 * Allocate and init ciphering materials for the session.
	 * The address of the structure is used as session ID for
	 * the client.
	 */
	sess = TEE_Malloc(sizeof(*sess), 0);
	if (!sess)
		return TEE_ERROR_OUT_OF_MEMORY;

	sess->key_handle = TEE_HANDLE_NULL;
	sess->op_handle = TEE_HANDLE_NULL;

	*session = (void *)sess;
	DMSG("Session %p: newly allocated", *session);

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *session)
{
	struct aes_cipher *sess;

	/* Get ciphering context from session ID */
	DMSG("Session %p: release session", session);
	sess = (struct aes_cipher *)session;

	/* Release the session resources */
	if (sess->key_handle != TEE_HANDLE_NULL)
		TEE_FreeTransientObject(sess->key_handle);
	if (sess->op_handle != TEE_HANDLE_NULL)
		TEE_FreeOperation(sess->op_handle);
	TEE_Free(sess);
}

TEE_Result TA_InvokeCommandEntryPoint(void *session,
					uint32_t cmd,
					uint32_t param_types,
					TEE_Param params[4])
{
	switch (cmd) {
	case TA_HMAC_SHA256:
		return ta_hmac_sha256(session, param_types, params);
	case TA_AES_CMD_SET_KEY:
		return set_aes_key(session, param_types, params);
	case TA_AES_CMD_SET_IV:
		return reset_aes_iv(session, param_types, params);
	case TA_AES_CMD_CIPHER:
		return cipher_buffer(session, param_types, params);
	default:
		EMSG("Command ID 0x%x is not supported", cmd);
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
