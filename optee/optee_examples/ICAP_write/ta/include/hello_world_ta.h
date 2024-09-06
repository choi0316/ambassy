/*
 * Copyright (c) 2016-2017, Linaro Limited
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
#ifndef TA_HELLO_WORLD_H
#define TA_HELLO_WORLD_H


/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define TA_HELLO_WORLD_UUID \
	{ 0xc2b4d887, 0x52d7, 0x4e60, \
		{ 0x9d, 0x33, 0x56, 0x18, 0x40, 0x41, 0x14, 0x58} }

/* The function IDs implemented in this TA */
#define TA_EMBASSY_CMD_SET_ICAP				0
#define TA_EMBASSY_CMD_UNSET_ICAP			1
#define TA_EMBASSY_CMD_WRITE_ICAP_SETUP			2
#define TA_EMBASSY_CMD_WRITE_ICAP_TRANSMIT		3
#define TA_EMBASSY_CMD_WRITE_ICAP_RESET			4
#define TA_EMBASSY_CMD_XMPU				5
#define TA_EMBASSY_CMD_ATTEST				6
#define TA_EMBASSY_CMD_REG_CHECK			7
#define TA_EMBASSY_CMD_REG_DISCONNECTED			8
#define TA_EMBASSY_CMD_CONFIGURE			11
#define TA_EMBASSY_CMD_SET_ACCESS			12

#endif /*TA_HELLO_WORLD_H*/
