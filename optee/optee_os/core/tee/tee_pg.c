#include <tee/tee_pg.h>
#include <stdlib.h>
#include <tee_api_defines.h>
#include <trace.h>
#include <kernel/tee_ta_manager.h>
#include <tee_internal_api_extensions.h>
//#include <drivers/hdlcd.h>
#include <string.h>


TEE_Result syscall_playground_cmd(unsigned long cmd, unsigned long __unused arg0, unsigned long __unused arg1, unsigned long __unused arg2, unsigned long __unused arg3)
{
        switch (cmd) {
	case TEE_PLAYGROUND_CMD_SET_SECURE_SCREEN:
	        //jwseo
        	asm ("smc #0xffff\n");
                
//                if (arg0 == 0 || arg0 == 1)                        
//	                set_secure_hdlcd(arg0);
//                else if (arg0 == 2)
//                        set_secure_hdlcd(!is_secure_hdlcd());
                break;
        case TEE_PLAYGROUND_CMD_GET_FRAME_BUFFER:
//                get_frame_buffer((uint32_t*)arg0, (uint64_t*)arg1);
//                *(unsigned long*)arg2 = HDLCD_SCREEN_WIDTH;
//                *(unsigned long*)arg3 = HDLCD_SCREEN_HEIGHT;
                break;
        case TEE_PLAYGROUND_CMD_SET_FRAME_BUFFER_OFFSET:
//                set_frame_buffer_offset((uint64_t)arg0);
                break;
        default:                
                break;
        }

        return TEE_SUCCESS;
}

