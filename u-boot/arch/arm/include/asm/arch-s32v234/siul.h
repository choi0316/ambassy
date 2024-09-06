/*
 * (C) Copyright 2015, Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ARCH_ARM_MACH_S32V234_SIUL_H__
#define __ARCH_ARM_MACH_S32V234_SIUL_H__

#include "ddr.h"

#define SIUL2_MIDR1				(SIUL2_BASE_ADDR + 0x00000004)
#define SIUL2_MIDR2				(SIUL2_BASE_ADDR + 0x00000008)
#define SIUL2_DISR0				(SIUL2_BASE_ADDR + 0x00000010)
#define SIUL2_DIRER0				(SIUL2_BASE_ADDR + 0x00000018)
#define SIUL2_DIRSR0				(SIUL2_BASE_ADDR + 0x00000020)
#define SIUL2_IREER0				(SIUL2_BASE_ADDR + 0x00000028)
#define SIUL2_IFEER0				(SIUL2_BASE_ADDR + 0x00000030)
#define SIUL2_IFER0				(SIUL2_BASE_ADDR + 0x00000038)

#define SIUL2_IFMCR_BASE			(SIUL2_BASE_ADDR + 0x00000040)
#define SIUL2_IFMCRn(i)				(SIUL2_IFMCR_BASE + 4 * (i))

#define SIUL2_IFCPR				(SIUL2_BASE_ADDR + 0x000000C0)

/* SIUL2_MSCR specifications as stated in Reference Manual:
 * 0 - 359 Output Multiplexed Signal Configuration Registers
 * 512- 1023 Input Multiplexed Signal Configuration Registers */
#define SIUL2_MSCR_BASE				(SIUL2_BASE_ADDR + 0x00000240)
#define SIUL2_MSCRn(i)				(SIUL2_MSCR_BASE + 4 * (i))

#define SIUL2_IMCR_BASE				(SIUL2_BASE_ADDR + 0x00000A40)
#define SIUL2_IMCRn(i)				(SIUL2_IMCR_BASE +  4 * (i))

#define SIUL2_GPDO_BASE				(SIUL2_BASE_ADDR + 0x00001300)
#define SIUL2_GPDOn(i)				(SIUL2_GPDO_BASE + 4 * (i))

#define SIUL2_GPDI_BASE				(SIUL2_BASE_ADDR + 0x00001500)
#define SIUL2_GPDIn(i)				(SIUL2_GPDI_BASE + 4 * (i))

#define SIUL2_PGPDO_BASE			(SIUL2_BASE_ADDR + 0x00001700)
#define SIUL2_PGPDOn(i)				(SIUL2_PGPDO_BASE +  2 * (i))

#define SIUL2_PGPDI_BASE			(SIUL2_BASE_ADDR + 0x00001740)
#define SIUL2_PGPDIn(i)				(SIUL2_PGPDI_BASE + 2 * (i))

#define SIUL2_MPGPDO_BASE			(SIUL2_BASE_ADDR + 0x00001780)
#define SIUL2_MPGPDOn(i)			(SIUL2_MPGPDO_BASE + 4 * (i))

/* SIUL2_MSCR masks */
#define SIUL2_MSCR_DDR_DO_TRIM(v)	((v) & 0xC0000000)
#define SIUL2_MSCR_DDR_DO_TRIM_MIN	(0 << 30)
#define SIUL2_MSCR_DDR_DO_TRIM_50PS	(1 << 30)
#define SIUL2_MSCR_DDR_DO_TRIM_100PS	(2 << 30)
#define SIUL2_MSCR_DDR_DO_TRIM_150PS	(3 << 30)

#define SIUL2_MSCR_DDR_INPUT(v)		((v) & 0x20000000)
#define SIUL2_MSCR_DDR_INPUT_CMOS	(0 << 29)
#define SIUL2_MSCR_DDR_INPUT_DIFF_DDR	(1 << 29)

#define SIUL2_MSCR_DDR_SEL(v)		((v) & 0x18000000)
#define SIUL2_MSCR_DDR_SEL_DDR3		(0 << 27)
#define SIUL2_MSCR_DDR_SEL_LPDDR2	(2 << 27)

#define SIUL2_MSCR_DDR_ODT(v)		((v) & 0x07000000)
#define SIUL2_MSCR_DDR_ODT_120ohm	(1 << 24)
#define SIUL2_MSCR_DDR_ODT_60ohm	(2 << 24)
#define SIUL2_MSCR_DDR_ODT_40ohm	(3 << 24)
#define SIUL2_MSCR_DDR_ODT_30ohm	(4 << 24)
#define SIUL2_MSCR_DDR_ODT_24ohm	(5 << 24)
#define SIUL2_MSCR_DDR_ODT_20ohm	(6 << 24)
#define SIUL2_MSCR_DDR_ODT_17ohm	(7 << 24)

#define SIUL2_MSCR_DCYCLE_TRIM(v)	((v) & 0x00C00000)
#define SIUL2_MSCR_DCYCLE_TRIM_NONE	(0 << 22)
#define SIUL2_MSCR_DCYCLE_TRIM_LEFT	(1 << 22)
#define SIUL2_MSCR_DCYCLE_TRIM_RIGHT	(2 << 22)

#define SIUL2_MSCR_OBE(v)		((v) & 0x00200000)
#define SIUL2_MSCR_OBE_EN		(1 << 21)

#define SIUL2_MSCR_ODE(v)		((v) & 0x00100000)
#define SIUL2_MSCR_ODE_EN		(1 << 20)

#define SIUL2_MSCR_IBE(v)		((v) & 0x00010000)
#define SIUL2_MSCR_IBE_EN		(1 << 19)

#define SIUL2_MSCR_HYS(v)		((v) & 0x00400000)
#define SIUL2_MSCR_HYS_EN		(1 << 18)

#define SIUL2_MSCR_INV(v)		((v) & 0x00020000)
#define SIUL2_MSCR_INV_EN		(1 << 17)

#define SIUL2_MSCR_PKE(v)		((v) & 0x00010000)
#define SIUL2_MSCR_PKE_EN		(1 << 16)

#define SIUL2_MSCR_SRE(v)		((v) & 0x0000C000)
#define SIUL2_MSCR_SRE_SPEED_LOW_50	(0 << 14)
#define SIUL2_MSCR_SRE_SPEED_LOW_100	(1 << 14)
#define SIUL2_MSCR_SRE_SPEED_HIGH_100	(2 << 14)
#define SIUL2_MSCR_SRE_SPEED_HIGH_200	(3 << 14)

#define SIUL2_MSCR_PUE(v)		((v) & 0x00002000)
#define SIUL2_MSCR_PUE_EN		(1 << 13)

#define SIUL2_MSCR_PUS(v)		((v) & 0x00001800)
#define SIUL2_MSCR_PUS_100K_DOWN	(0 << 11)
#define SIUL2_MSCR_PUS_50K_DOWN		(1 << 11)
#define SIUL2_MSCR_PUS_100K_UP		(2 << 11)
#define SIUL2_MSCR_PUS_33K_UP		(3 << 11)

#define SIUL2_MSCR_DSE(v)		((v) & 0x00000700)
#define SIUL2_MSCR_DSE_240ohm		(1 << 8)
#define SIUL2_MSCR_DSE_120ohm		(2 << 8)
#define SIUL2_MSCR_DSE_80ohm		(3 << 8)
#define SIUL2_MSCR_DSE_60ohm		(4 << 8)
#define SIUL2_MSCR_DSE_48ohm		(5 << 8)
#define SIUL2_MSCR_DSE_40ohm		(6 << 8)
#define SIUL2_MSCR_DSE_34ohm		(7 << 8)

#define SIUL2_MSCR_CRPOINT_TRIM(v)	((v) & 0x000000C0)
#define SIUL2_MSCR_CRPOINT_TRIM_1	(1 << 6)

#define SIUL2_MSCR_SMC(v)		((v) & 0x00000020)
#define SIUL2_MSCR_MUX_MODE(v)		((v) & 0x0000000f)
#define SIUL2_MSCR_MUX_MODE_ALT1	(0x1)
#define SIUL2_MSCR_MUX_MODE_ALT2	(0x2)
#define SIUL2_MSCR_MUX_MODE_ALT3	(0x3)

/* UART settings */
#define SIUL2_UART0_TXD_PAD	12
#define SIUL2_UART_TXD		(SIUL2_MSCR_OBE_EN | SIUL2_MSCR_PUS_100K_UP | SIUL2_MSCR_DSE_60ohm |	\
				SIUL2_MSCR_SRE_SPEED_LOW_100 | SIUL2_MSCR_MUX_MODE_ALT1)

#define SIUL2_UART0_MSCR_RXD_PAD	11
#define SIUL2_UART0_IMCR_RXD_PAD	200

#define SIUL2_UART_MSCR_RXD	(SIUL2_MSCR_PUE_EN | SIUL2_MSCR_IBE_EN | SIUL2_MSCR_DCYCLE_TRIM_RIGHT)
#define SIUL2_UART_IMCR_RXD	(SIUL2_MSCR_MUX_MODE_ALT2)

/* uSDHC settings */
#define SIUL2_USDHC_PAD_CTRL_BASE	(SIUL2_MSCR_SRE_SPEED_HIGH_200 | SIUL2_MSCR_OBE_EN |	\
						SIUL2_MSCR_DSE_34ohm | SIUL2_MSCR_PKE_EN | SIUL2_MSCR_IBE_EN |		\
						SIUL2_MSCR_PUS_100K_UP | SIUL2_MSCR_PUE_EN )
#define SIUL2_USDHC_PAD_CTRL_CMD	(SIUL2_USDHC_PAD_CTRL_BASE | SIUL2_MSCR_MUX_MODE_ALT1)
#define SIUL2_USDHC_PAD_CTRL_CLK	(SIUL2_USDHC_PAD_CTRL_BASE | SIUL2_MSCR_MUX_MODE_ALT2)
#define SIUL2_USDHC_PAD_CTRL_DAT0_3	(SIUL2_USDHC_PAD_CTRL_BASE | SIUL2_MSCR_MUX_MODE_ALT2)
#define SIUL2_USDHC_PAD_CTRL_DAT4_7	(SIUL2_USDHC_PAD_CTRL_BASE | SIUL2_MSCR_MUX_MODE_ALT3)

#endif /*__ARCH_ARM_MACH_S32V234_SIUL_H__ */
