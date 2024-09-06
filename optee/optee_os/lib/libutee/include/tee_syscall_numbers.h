/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
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

#ifndef TEE_SYSCALL_NUMBERS_H
#define TEE_SYSCALL_NUMBERS_H

#define TEE_SCN_RETURN				0
#define TEE_SCN_LOG				1
#define TEE_SCN_PANIC				2
#define TEE_SCN_GET_PROPERTY			3
#define TEE_SCN_GET_PROPERTY_NAME_TO_INDEX	4
#define TEE_SCN_OPEN_TA_SESSION			5
#define TEE_SCN_CLOSE_TA_SESSION		6
#define TEE_SCN_INVOKE_TA_COMMAND		7
#define TEE_SCN_CHECK_ACCESS_RIGHTS		8
#define TEE_SCN_GET_CANCELLATION_FLAG		9
#define TEE_SCN_UNMASK_CANCELLATION		10
#define TEE_SCN_MASK_CANCELLATION		11
#define TEE_SCN_WAIT				12
#define TEE_SCN_GET_TIME			13
#define TEE_SCN_SET_TA_TIME			14
#define TEE_SCN_CRYP_STATE_ALLOC		15
#define TEE_SCN_CRYP_STATE_COPY			16
#define TEE_SCN_CRYP_STATE_FREE			17
#define TEE_SCN_HASH_INIT			18
#define TEE_SCN_HASH_UPDATE			19
#define TEE_SCN_HASH_FINAL			20
#define TEE_SCN_CIPHER_INIT			21
#define TEE_SCN_CIPHER_UPDATE			22
#define TEE_SCN_CIPHER_FINAL			23
#define TEE_SCN_CRYP_OBJ_GET_INFO		24
#define TEE_SCN_CRYP_OBJ_RESTRICT_USAGE		25
#define TEE_SCN_CRYP_OBJ_GET_ATTR		26
#define TEE_SCN_CRYP_OBJ_ALLOC			27
#define TEE_SCN_CRYP_OBJ_CLOSE			28
#define TEE_SCN_CRYP_OBJ_RESET			29
#define TEE_SCN_CRYP_OBJ_POPULATE		30
#define TEE_SCN_CRYP_OBJ_COPY			31
#define TEE_SCN_CRYP_DERIVE_KEY			32
#define TEE_SCN_CRYP_RANDOM_NUMBER_GENERATE	33
#define TEE_SCN_AUTHENC_INIT			34
#define TEE_SCN_AUTHENC_UPDATE_AAD		35
#define TEE_SCN_AUTHENC_UPDATE_PAYLOAD		36
#define TEE_SCN_AUTHENC_ENC_FINAL		37
#define TEE_SCN_AUTHENC_DEC_FINAL		38
#define TEE_SCN_ASYMM_OPERATE			39
#define TEE_SCN_ASYMM_VERIFY			40
#define TEE_SCN_STORAGE_OBJ_OPEN		41
#define TEE_SCN_STORAGE_OBJ_CREATE		42
#define TEE_SCN_STORAGE_OBJ_DEL			43
#define TEE_SCN_STORAGE_OBJ_RENAME		44
#define TEE_SCN_STORAGE_ENUM_ALLOC		45
#define TEE_SCN_STORAGE_ENUM_FREE		46
#define TEE_SCN_STORAGE_ENUM_RESET		47
#define TEE_SCN_STORAGE_ENUM_START		48
#define TEE_SCN_STORAGE_ENUM_NEXT		49
#define TEE_SCN_STORAGE_OBJ_READ		50
#define TEE_SCN_STORAGE_OBJ_WRITE		51
#define TEE_SCN_STORAGE_OBJ_TRUNC		52
#define TEE_SCN_STORAGE_OBJ_SEEK		53
#define TEE_SCN_CRYP_OBJ_GENERATE_KEY		54
#define TEE_SCN_SE_SERVICE_OPEN			55
#define TEE_SCN_SE_SERVICE_CLOSE		56
#define TEE_SCN_SE_SERVICE_GET_READERS		57
#define TEE_SCN_SE_READER_GET_PROP		58
#define TEE_SCN_SE_READER_GET_NAME		59
#define TEE_SCN_SE_READER_OPEN_SESSION		60
#define TEE_SCN_SE_READER_CLOSE_SESSIONS	61
#define TEE_SCN_SE_SESSION_IS_CLOSED		62
#define TEE_SCN_SE_SESSION_GET_ATR		63
#define TEE_SCN_SE_SESSION_OPEN_CHANNEL		64
#define TEE_SCN_SE_SESSION_CLOSE		65
#define TEE_SCN_SE_CHANNEL_SELECT_NEXT		66
#define TEE_SCN_SE_CHANNEL_GET_SELECT_RESP	67
#define TEE_SCN_SE_CHANNEL_TRANSMIT		68
#define TEE_SCN_SE_CHANNEL_CLOSE		69
#define TEE_SCN_CACHE_OPERATION			70

//jwseo
#define TEE_SCN_PLAYGROUND_CMD          71
//dihwang
#define TEE_SCN_EMBASSY_CMD          72

#define TEE_SCN_MAX				72

/* Maximum number of allowed arguments for a syscall */
#define TEE_SVC_MAX_ARGS			8

#endif /* TEE_SYSCALL_NUMBERS_H */
