#include <stdint.h>
#include <string.h>
#include <trace.h>

#include <io.h>
#include <drivers/hdlcd.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>

static vaddr_t sec_hdlcd_base;
static vaddr_t hdlcd_base;
static vaddr_t fb_base;

static uint64_t sw_fb_offset;

void init_hdlcd(void)
{
        hdlcd_base = (vaddr_t)phys_to_virt(HDLCD0_BASE, MEM_AREA_IO_SEC);
        fb_base = (vaddr_t)phys_to_virt(HDLCD_FB_BASE, MEM_AREA_IO_SEC_U);
        sec_hdlcd_base = (vaddr_t)phys_to_virt(SEC_HDLCD, MEM_AREA_IO_SEC);

        sw_fb_offset = 0;
}

void set_secure_hdlcd(bool secure)
{
        static paddr_t nw_fb;
        static bool first = true;
        
        if (first) {
                nw_fb = (paddr_t)read32(hdlcd_base + HDLCD_REG_FB_BASE);
                first = false;
        }
        
        write32(HDLCD_COMMAND_DISABLE, hdlcd_base + HDLCD_REG_COMMAND);               
        if (secure) {
                write32(0, sec_hdlcd_base);
                write32((intptr_t)HDLCD_FB_BASE + sw_fb_offset, hdlcd_base + HDLCD_REG_FB_BASE);
        }
        else {
                write32(1, sec_hdlcd_base);
                write32((intptr_t)nw_fb, hdlcd_base + HDLCD_REG_FB_BASE);
        }
        write32(HDLCD_COMMAND_ENABLE, hdlcd_base + HDLCD_REG_COMMAND);        
}

bool is_secure_hdlcd(void)
{
        return !(read32(sec_hdlcd_base) & 1);
}

void get_frame_buffer(uint32_t *fb, uint64_t *offset)
{
        *fb = (uint64_t)fb_base;
        *offset = sw_fb_offset;
}

void set_frame_buffer_offset(uint64_t offset)
{
        sw_fb_offset = offset;
        write32((intptr_t)HDLCD_FB_BASE + sw_fb_offset, hdlcd_base + HDLCD_REG_FB_BASE);        
}
