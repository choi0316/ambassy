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

/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
#define IMG_BUFFER_SIZE		100000

TEE_Result get_image_wrap(uint32_t nParamTypes, TEE_Param pParams[4]);
TEE_Result command(uint32_t nParamTypes, TEE_Param pParams[4]);
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


TEE_Result command(uint32_t nParamTypes, TEE_Param pParams[4]){
	    (void)nParamTypes;
/*
	if (pParams[0].value.a==1) //secure
		TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_SECURE_SCREEN, 1, 0, 0, 0);
	else if(pParams[0].value.a==2) //nonsecure
		TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_SECURE_SCREEN, 2, 0, 0, 0);
	return TEE_SUCCESS;
*/

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

    DMSG("\n----------START jwseo --------------\n\n");


	//TEE_Playground_CMD(TEE_PLAYGROUND_CMD_SET_FRAME_BUFFER_OFFSET, pParams[1].value.a,0,0,0);
	TEE_Playground_CMD(TEE_PLAYGROUND_CMD_GET_FRAME_BUFFER, (intptr_t)&fb, (intptr_t)&offset, (intptr_t)&width, (intptr_t)&height);


	img = (uint32_t*)(pParams[0].memref.buffer);
	fb = (uint32_t*)((intptr_t)fb+(uint32_t)pParams[1].value.a);
	
//	DMSG("received size: %d, status: %d\n",pParams[1].value.a, pParams[1].value.b);
	

	for(i=0; i<pParams[0].memref.size/4; i++)
			*fb++ = *img++;

			
	DMSG("\n---FINISH jwseo----\n\n");
	


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
	case TA_CMD_IMAGE:
		return get_image_wrap(param_types, params);
		//return TA_CreateEntryPoint();
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
