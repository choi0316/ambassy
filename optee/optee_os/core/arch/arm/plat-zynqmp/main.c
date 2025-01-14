/*
 * Copyright (c) 2016, Xilinx Inc.
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

#include <platform_config.h>

#include <stdint.h>
#include <string.h>

#include <drivers/gic.h>
#include <drivers/cdns_uart.h>

#include <arm.h>
#include <console.h>
#include <kernel/generic_boot.h>
#include <kernel/pm_stubs.h>
#include <kernel/misc.h>
#include <kernel/tee_time.h>
#include <mm/core_memprot.h>
#include <tee/entry_fast.h>
#include <tee/entry_std.h>
#include <trace.h>
#include <io.h>

static void main_fiq(void);
static struct gic_data gic_data;
static struct cdns_uart_data console_data;

unsigned int peri_regs[1000];

register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(CONSOLE_UART_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);

register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(GIC_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);

register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(GIC_BASE + GICD_OFFSET, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);

register_phys_mem(MEM_AREA_IO_SEC, // PSU
		  0xFFCA3000,
		  0x100);

//register_phys_mem(MEM_AREA_IO_SEC, //AXI-Stream
//		  0xA0000000,
//		  0x100);

register_phys_mem(MEM_AREA_IO_SEC, //AXI-Stream
		  0x80000000,
		  0x30000000);

register_phys_mem(MEM_AREA_IO_SEC, // XMPU
		  0xFD000000,
		  0x10000);

register_phys_mem(MEM_AREA_IO_SEC, // XMPU
		  0xFE200000,
		  0x10000);
register_phys_mem(MEM_AREA_IO_SEC, // XMPU
		  0xFD4A0000,
		  0x10000);
register_phys_mem(MEM_AREA_IO_SEC, // XMPU
		  0xFD4C0000,
		  0x10000);


static const struct thread_handlers handlers = {
	.std_smc = tee_entry_std,
	.fast_smc = tee_entry_fast,
	.nintr = main_fiq,
#if defined(CFG_WITH_ARM_TRUSTED_FW)
	.cpu_on = cpu_on_handler,
	.cpu_off = pm_do_nothing,
	.cpu_suspend = pm_do_nothing,
	.cpu_resume = pm_do_nothing,
	.system_off = pm_do_nothing,
	.system_reset = pm_do_nothing,
#else
	.cpu_on = pm_panic,
	.cpu_off = pm_panic,
	.cpu_suspend = pm_panic,
	.cpu_resume = pm_panic,
	.system_off = pm_panic,
	.system_reset = pm_panic,
#endif
};

const struct thread_handlers *generic_boot_get_handlers(void)
{
	return &handlers;
}

void main_init_gic(void)
{
	vaddr_t gicc_base, gicd_base;

	gicc_base = (vaddr_t)phys_to_virt(GIC_BASE + GICC_OFFSET,
					  MEM_AREA_IO_SEC);
	gicd_base = (vaddr_t)phys_to_virt(GIC_BASE + GICD_OFFSET,
					  MEM_AREA_IO_SEC);
	/* On ARMv8, GIC configuration is initialized in ARM-TF */
	gic_init_base_addr(&gic_data, gicc_base, gicd_base);
}

static void main_fiq(void)
{
	gic_it_handle(&gic_data);
}

void console_init(void)
{
	cdns_uart_init(&console_data, CONSOLE_UART_BASE,
		       CONSOLE_UART_CLK_IN_HZ, CONSOLE_BAUDRATE);
	register_serial_console(&console_data.chip);
}

