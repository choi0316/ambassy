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

#define STR_TRACE_USER_TA "HELLO_WORLD"

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "hello_world_ta.h"
#include "string.h"
/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
#define IMG_BUFFER_SIZE		100000

TEE_Result get_image_wrap(uint32_t nParamTypes, TEE_Param pParams[4]);
TEE_Result command(uint32_t nParamTypes, TEE_Param pParams[4]);
TEE_Result login(uint32_t nParamTypes, TEE_Param pParams[4]);

size_t tee_fread(void *ptr, size_t size, size_t count, FILE *fp) {
	TEE_Result res;
	uint32_t cnt;

	if (ptr ==NULL)
		return NULL;
	res = TEE_ReadObjectData((TEE_ObjectHandle)fp, ptr, size*count, &cnt);
	if (res!=TEE_SUCCESS)
		return NULL;

	return cnt;
}

TEE_Result TA_CreateEntryPoint(void)
{
        uint32_t *fb;
        uint64_t offset, width, height;        
        uint32_t i;
        
    TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_SECURE_SCREEN, 2, 0, 0, 0);
	TEE_Playground_CMD(TEE_PLAYGROUND_CMD_GET_FRAME_BUFFER, (intptr_t)&fb, (intptr_t)&offset, (intptr_t)&width, (intptr_t)&height);

        for (i=0; i< width*height; i++) {
               fb[i] = 0x00ff0000;
        }

	DMSG("has been called");
	return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed if the TA has not
 * crashed or panicked. This is the last call in the TA.
 */
void TA_DestroyEntryPoint(void)
{
	DMSG("has been called");
}

/*
 * Called when a new session is opened to the TA. *sess_ctx can be updated
 * with a value to be able to identify this session in subsequent calls to the
 * TA. In this function you will normally do the global initialization for the
 * TA.
 */
TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;
	(void)&sess_ctx;

	/*
	 * The DMSG() macro is non-standard, TEE Internal API doesn't
	 * specify any means to logging from a TA.
	 */
	DMSG("Hello World!\n");

	/* If return value != TEE_SUCCESS the session will not be created. */
	return TEE_SUCCESS;
}

/*
 * Called when a session is closed, sess_ctx hold the value that was
 * assigned by TA_OpenSessionEntryPoint().
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx; /* Unused parameter */
	DMSG("Goodbye!\n");
}

static TEE_Result inc_value(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("has been called");
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	DMSG("Got value: %u from NW", params[0].value.a);
	params[0].value.a++;
	DMSG("Increase value to: %u", params[0].value.a);
	return TEE_SUCCESS;
}

FILE* tee_fcreate(){
	uint32_t storageID = 1;
	void* objectID;
	size_t objectIDLen;
	uint32_t flags;
	FILE *object;
	TEE_Result res=TEE_SUCCESS;

	char* filename = "temp1";

	objectID = (void*)filename;
	objectIDLen = strlen(filename);

	flags = TEE_DATA_FLAG_ACCESS_READ|TEE_DATA_FLAG_ACCESS_WRITE | TEE_DATA_FLAG_ACCESS_WRITE_META | TEE_DATA_FLAG_OVERWRITE | TEE_DATA_FLAG_SHARE_READ | TEE_DATA_FLAG_SHARE_WRITE;

	res = TEE_CreatePersistentObject(storageID, objectID, objectIDLen, flags,NULL, NULL, 0, (TEE_ObjectHandle*)&object);

	if(res!=TEE_SUCCESS){
		EMSG("Failed to create persistent object, res=0x%08x\n\n", res);
		return NULL;
	}
	res = TEE_WriteObjectData(object, "1234", 4);
	if(res!=TEE_SUCCESS){
		EMSG("Failed to write data, res=0x%08x\n\n", res);
		TEE_CloseObject(object);
		return NULL;
	}
	TEE_CloseObject(object);

	return object;

}


FILE* tee_fopen(const char *filename/*, const char* mode*/){

	uint32_t storageID = 1;
	void* objectID;
	size_t objectIDLen;
	uint32_t flags;
	FILE *object;
	TEE_Result res=0;


	objectID = (void*)filename;
	objectIDLen = strlen(filename);

	flags = TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_ACCESS_WRITE | TEE_DATA_FLAG_ACCESS_WRITE_META | TEE_DATA_FLAG_OVERWRITE | TEE_DATA_FLAG_SHARE_READ | TEE_DATA_FLAG_SHARE_WRITE;

	res = TEE_OpenPersistentObject(storageID, objectID, objectIDLen, TEE_DATA_FLAG_ACCESS_READ, (TEE_ObjectHandle*)&object);

	if(res!=TEE_SUCCESS){
		EMSG("Failed to open file, res=0x%08x\n\n", res);
		return NULL;
	}
	return object;
}


TEE_Result login(uint32_t nParamTypes, TEE_Param pParams[4]){

	(void)nParamTypes;
	FILE *FROMF;
	char c;
	uint32_t count;
	char *buf;
	int i;
	TEE_Result res=0;


	tee_fcreate();

	//DMSG("\n\nuser : %s\n\n", (char *)pParams[0].memref.buffer);

	FROMF = tee_fopen((char *)pParams[0].memref.buffer);

/*
	for(i=0; i<10 ; i++){
	res = TEE_ReadObjectData((TEE_ObjectHandle)FROMF, &c, 1, &count);
	if (res!=TEE_SUCCESS)
		return NULL;
	buf[i] = c;
	if(c=='\n')
		break;
	}
	
	printf("buf: %s\n\n", buf);
*/
	
	if(FROMF ==NULL){
		printf("keyfile could not be opened.\n\n");
		return -1;
	}
	TEE_CloseObject(FROMF);

	return TEE_SUCCESS;

}


TEE_Result command(uint32_t nParamTypes, TEE_Param pParams[4]){
	    (void)nParamTypes;

	TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_SECURE_SCREEN, 2, 0, 0, 0);
	DMSG("Got value: %u from NW", pParams[0].value.a);
	
	return TEE_SUCCESS;
	}




TEE_Result get_image_wrap(uint32_t nParamTypes, TEE_Param pParams[4]){

    uint32_t *fb;
    uint64_t offset, width, height;        
    uint32_t i;

	uint32_t* img;
		
    (void)nParamTypes;


	//TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_FRAME_BUFFER_OFFSET, pParams[1].value.a,0,0,0);
	TEE_Playground_CMD(TEE_PLAYGROUND_CMD_GET_FRAME_BUFFER, (intptr_t)&fb, (intptr_t)&offset, (intptr_t)&width, (intptr_t)&height);


	img = (uint32_t*)(pParams[0].memref.buffer);
	fb = (uint32_t*)((intptr_t)fb+(uint32_t)pParams[1].value.a);
	

	for(i=0; i<pParams[0].memref.size/4; i++)
			*fb++ = *img++;

	return TEE_SUCCESS;
}


/*
 * Called when a TA is invoked. sess_ctx hold that value that was
 * assigned by TA_OpenSessionEntryPoint(). The rest of the paramters
 * comes from normal world.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx; /* Unused parameter */

	switch (cmd_id) {
	case TA_HELLO_WORLD_CMD_INC_VALUE:
		return inc_value(param_types, params);
	case TA_LOGIN:
		return login(param_types, params);
	case TA_STORAGE_CMD_WRITE:
		return get_image_wrap(param_types, params);
	case TA_START_WRITE:
		return command(param_types, params);
#if 0
	case TA_HELLO_WORLD_CMD_XXX:
		return ...
		break;
	case TA_HELLO_WORLD_CMD_YYY:
		return ...
		break;
	case TA_HELLO_WORLD_CMD_ZZZ:
		return ...
		break;
	...
#endif
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
