/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
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
#include <printk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_api_defines.h>
#include <tee_api.h>
#include <tee_api_types.h>
#include <tee_arith_internal.h>
#include <tee_internal_api_extensions.h>
#include <tee_isocket.h>
#include <user_ta_header.h>
#include <utee_syscalls.h>
#include <util.h>

#include "string_ext.h"
#include "base64.h"

TEE_Result TEE_SetICAP(uint32_t access_type,
                          uint32_t access_number, uint32_t access_base,
                          uint32_t access_size)
{
	DMSG("TEE_SetICAP=======================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_SET_ICAP, (unsigned long)access_type, (unsigned long)access_number, (unsigned long)access_base, (unsigned long)access_size);	
	
	return res;
}

TEE_Result TEE_UnsetICAP(const char *afea,
				   const char *name, char *value,
				   uint32_t *value_len)
{
	DMSG("TEE_UnsetICAP====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_UNSET_ICAP, 0, 0, 0, 0);	
	
	return res;
}

TEE_Result TEE_WriteICAP_Setup(const char *config_file, 
				   const char *name, char *value,
				   uint32_t *value_len)
{
	DMSG("TEE_WriteICAP_Setup====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_WRITE_ICAP_SETUP, 0, 0, 0, 0);	
	
	return res;
}

TEE_Result TEE_WriteICAP_Transmit(const char *config_file, 
				   const char * bitstream, uint32_t length,
				   int *access_range)
{
	//DMSG("TEE_WriteICAP_Transmit ==================%x, %x\n", packet, value);
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_WRITE_ICAP_TRANSMIT, (unsigned long)bitstream, (unsigned long)length, (unsigned long)access_range, 0);	
	
	return res;
}

TEE_Result TEE_WriteICAP_Reset(const char *config_file, 
				   const char *name, char *value,
				   uint32_t *value_len)
{
	DMSG("TEE_WriteICAP_Reset========dihwang============\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_WRITE_ICAP_RESET, 0, 0, 0, 0);	
	
	return res;
}

TEE_Result TEE_Xmpu(const char *config_file, 
				   const char *name, char *value,
				   uint32_t *value_len)
{
	DMSG("TEE_Xmpu====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_XMPU, 0, 0, 0, 0);	
	
	return res;
}

TEE_Result TEE_Attest(const char *config_file, 
				   const char *string, uint32_t length,
				   const char *result)
{
	DMSG("TEE_Attest====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_ATTEST, (unsigned long)string, (unsigned long)length, (unsigned long)result, 0);	
	
	return res;
}

TEE_Result TEE_Reg_Check(const char *config_file, 
				   const char *string, uint32_t length,
				   const char *result)
{

	DMSG("TEE_Attest====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_REG_CHECK, (unsigned long)string, (unsigned long)length, (unsigned long)result, 0);	
	
	return res;

}

TEE_Result TEE_Reg_Disconnected(const char *config_file, 
				   const char *string, uint32_t length,
				   const char *result)
{

	DMSG("TEE_Attest====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_REG_DISCONNECTED, (unsigned long)string, (unsigned long)length, (unsigned long)result, 0);	
	
	return res;

}
TEE_Result TEE_Reg_Configure(const char *config_file, 
				   const char *string, uint32_t length,
				   const char *result)
{

	DMSG("TEE_Attest====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_CONFIGURE, (unsigned long)string, (unsigned long)length, (unsigned long)result, 0);	
	
	return res;

}
TEE_Result TEE_Set_Identity(const char *config_file, 
				   uint32_t *user_identity, uint32_t *terminal_identity,
				   uint32_t *result)
{

	DMSG("TEE_Set_Identity====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_SET_IDENTITY, (unsigned long)user_identity, (unsigned long)terminal_identity, (unsigned long)result, 0);	
	
	return res;

}
TEE_Result TEE_Get_Identity(const char *config_file, 
				   uint32_t *user_identity, uint32_t *terminal_identity,
				   uint32_t *result)
{

	DMSG("TEE_Get_Identity====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_GET_IDENTITY, (unsigned long)user_identity, (unsigned long)terminal_identity, (unsigned long)result, 0);	
	
	return res;

}

TEE_Result TEE_Set_Access(uint32_t access_type,
                          uint32_t access_number, uint32_t access_base,
                          uint32_t access_size)
{

	DMSG("TEE_Set_Access====================\n");
	TEE_Result res;
	
	utee_embassy_cmd(TEE_PLAYGROUND_CMD_SET_ACCESS, (unsigned long)access_type, (unsigned long)access_number, (unsigned long)access_base, (unsigned long)access_size);	
	
	return res;

}
