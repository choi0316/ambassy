#ifndef TEE_PG_H
#define TEE_PG_H

#include <types_ext.h>
#include <tee_api_types.h>

TEE_Result syscall_playground_cmd(unsigned long cmd, unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3);


#endif /*TEE_PG*/
