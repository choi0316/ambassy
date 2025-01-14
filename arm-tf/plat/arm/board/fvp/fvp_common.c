/*
 * Copyright (c) 2013-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arm_config.h>
#include <arm_def.h>
#include <assert.h>
#include <cci.h>
#include <ccn.h>
#include <debug.h>
#include <gicv2.h>
#include <mmio.h>
#include <plat_arm.h>
#include <v2m_def.h>
#include "../fvp_def.h"

/* Defines for GIC Driver build time selection */
#define FVP_GICV2		1
#define FVP_GICV3		2
#define FVP_GICV3_LEGACY	3

/*******************************************************************************
 * arm_config holds the characteristics of the differences between the three FVP
 * platforms (Base, A53_A57 & Foundation). It will be populated during cold boot
 * at each boot stage by the primary before enabling the MMU (to allow
 * interconnect configuration) & used thereafter. Each BL will have its own copy
 * to allow independent operation.
 ******************************************************************************/
arm_config_t arm_config;

#define MAP_DEVICE0	MAP_REGION_FLAT(DEVICE0_BASE,			\
					DEVICE0_SIZE,			\
					MT_DEVICE | MT_RW | MT_SECURE)

#define MAP_DEVICE1	MAP_REGION_FLAT(DEVICE1_BASE,			\
					DEVICE1_SIZE,			\
					MT_DEVICE | MT_RW | MT_SECURE)

#define MAP_DEVICE2	MAP_REGION_FLAT(DEVICE2_BASE,			\
					DEVICE2_SIZE,			\
					MT_DEVICE | MT_RW | MT_SECURE)


/*
 * Table of memory regions for various BL stages to map using the MMU.
 * This doesn't include Trusted SRAM as arm_setup_page_tables() already
 * takes care of mapping it.
 *
 * The flash needs to be mapped as writable in order to erase the FIP's Table of
 * Contents in case of unrecoverable error (see plat_error_handler()).
 */
#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	ARM_MAP_SHARED_RAM,
	V2M_MAP_FLASH0_RW,
	V2M_MAP_IOFPGA,
	MAP_DEVICE0,
	MAP_DEVICE1,
	MAP_DEVICE2,
#if TRUSTED_BOARD_BOOT
	ARM_MAP_NS_DRAM1,
#endif
	{0}
};
#endif
#ifdef IMAGE_BL2
const mmap_region_t plat_arm_mmap[] = {
	ARM_MAP_SHARED_RAM,
	V2M_MAP_FLASH0_RW,
	V2M_MAP_IOFPGA,
	MAP_DEVICE0,
	MAP_DEVICE1,
	MAP_DEVICE2,
	ARM_MAP_NS_DRAM1,
	ARM_MAP_TSP_SEC_MEM,
#if ARM_BL31_IN_DRAM
	ARM_MAP_BL31_SEC_DRAM,
#endif
	{0}
};
#endif
#ifdef IMAGE_BL2U
const mmap_region_t plat_arm_mmap[] = {
	MAP_DEVICE0,
	V2M_MAP_IOFPGA,
	{0}
};
#endif
#ifdef IMAGE_BL31
const mmap_region_t plat_arm_mmap[] = {
	ARM_MAP_SHARED_RAM,
	V2M_MAP_IOFPGA,
	MAP_DEVICE0,
	MAP_DEVICE1,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
#ifdef AARCH32
	ARM_MAP_SHARED_RAM,
#endif
	V2M_MAP_IOFPGA,
	MAP_DEVICE0,
	MAP_DEVICE1,
	{0}
};
#endif

ARM_CASSERT_MMAP

#if FVP_INTERCONNECT_DRIVER != FVP_CCN
static const int fvp_cci400_map[] = {
	PLAT_FVP_CCI400_CLUS0_SL_PORT,
	PLAT_FVP_CCI400_CLUS1_SL_PORT,
};

static const int fvp_cci5xx_map[] = {
	PLAT_FVP_CCI5XX_CLUS0_SL_PORT,
	PLAT_FVP_CCI5XX_CLUS1_SL_PORT,
};

static unsigned int get_interconnect_master(void)
{
	unsigned int master;
	u_register_t mpidr;

	mpidr = read_mpidr_el1();
	master = (arm_config.flags & ARM_CONFIG_FVP_SHIFTED_AFF) ?
		MPIDR_AFFLVL2_VAL(mpidr) : MPIDR_AFFLVL1_VAL(mpidr);

	assert(master < FVP_CLUSTER_COUNT);
	return master;
}
#endif

/*******************************************************************************
 * A single boot loader stack is expected to work on both the Foundation FVP
 * models and the two flavours of the Base FVP models (AEMv8 & Cortex). The
 * SYS_ID register provides a mechanism for detecting the differences between
 * these platforms. This information is stored in a per-BL array to allow the
 * code to take the correct path.Per BL platform configuration.
 ******************************************************************************/
void fvp_config_setup(void)
{
	unsigned int rev, hbi, bld, arch, sys_id;

	sys_id = mmio_read_32(V2M_SYSREGS_BASE + V2M_SYS_ID);
	rev = (sys_id >> V2M_SYS_ID_REV_SHIFT) & V2M_SYS_ID_REV_MASK;
	hbi = (sys_id >> V2M_SYS_ID_HBI_SHIFT) & V2M_SYS_ID_HBI_MASK;
	bld = (sys_id >> V2M_SYS_ID_BLD_SHIFT) & V2M_SYS_ID_BLD_MASK;
	arch = (sys_id >> V2M_SYS_ID_ARCH_SHIFT) & V2M_SYS_ID_ARCH_MASK;

	if (arch != ARCH_MODEL) {
		ERROR("This firmware is for FVP models\n");
		panic();
	}

	/*
	 * The build field in the SYS_ID tells which variant of the GIC
	 * memory is implemented by the model.
	 */
	switch (bld) {
	case BLD_GIC_VE_MMAP:
		ERROR("Legacy Versatile Express memory map for GIC peripheral"
				" is not supported\n");
		panic();
		break;
	case BLD_GIC_A53A57_MMAP:
		break;
	default:
		ERROR("Unsupported board build %x\n", bld);
		panic();
	}

	/*
	 * The hbi field in the SYS_ID is 0x020 for the Base FVP & 0x010
	 * for the Foundation FVP.
	 */
	switch (hbi) {
	case HBI_FOUNDATION_FVP:
		arm_config.flags = 0;

		/*
		 * Check for supported revisions of Foundation FVP
		 * Allow future revisions to run but emit warning diagnostic
		 */
		switch (rev) {
		case REV_FOUNDATION_FVP_V2_0:
		case REV_FOUNDATION_FVP_V2_1:
		case REV_FOUNDATION_FVP_v9_1:
		case REV_FOUNDATION_FVP_v9_6:
			break;
		default:
			WARN("Unrecognized Foundation FVP revision %x\n", rev);
			break;
		}
		break;
	case HBI_BASE_FVP:
		arm_config.flags |= (ARM_CONFIG_BASE_MMAP | ARM_CONFIG_HAS_TZC);

		/*
		 * Check for supported revisions
		 * Allow future revisions to run but emit warning diagnostic
		 */
		switch (rev) {
		case REV_BASE_FVP_V0:
			arm_config.flags |= ARM_CONFIG_FVP_HAS_CCI400;
			break;
		case REV_BASE_FVP_REVC:
			arm_config.flags |= (ARM_CONFIG_FVP_SHIFTED_AFF |
					ARM_CONFIG_FVP_HAS_SMMUV3 |
					ARM_CONFIG_FVP_HAS_CCI5XX);
			break;
		default:
			WARN("Unrecognized Base FVP revision %x\n", rev);
			break;
		}
		break;
	default:
		ERROR("Unsupported board HBI number 0x%x\n", hbi);
		panic();
	}
}


void fvp_interconnect_init(void)
{
#if FVP_INTERCONNECT_DRIVER == FVP_CCN
	if (ccn_get_part0_id(PLAT_ARM_CCN_BASE) != CCN_502_PART0_ID) {
		ERROR("Unrecognized CCN variant detected. Only CCN-502"
				" is supported");
		panic();
	}

	plat_arm_interconnect_init();
#else
	uintptr_t cci_base = 0;
	const int *cci_map = 0;
	unsigned int map_size = 0;

	if (!(arm_config.flags & (ARM_CONFIG_FVP_HAS_CCI400 |
				ARM_CONFIG_FVP_HAS_CCI5XX))) {
		return;
	}

	/* Initialize the right interconnect */
	if (arm_config.flags & ARM_CONFIG_FVP_HAS_CCI5XX) {
		cci_base = PLAT_FVP_CCI5XX_BASE;
		cci_map = fvp_cci5xx_map;
		map_size = ARRAY_SIZE(fvp_cci5xx_map);
	} else if (arm_config.flags & ARM_CONFIG_FVP_HAS_CCI400) {
		cci_base = PLAT_FVP_CCI400_BASE;
		cci_map = fvp_cci400_map;
		map_size = ARRAY_SIZE(fvp_cci400_map);
	}

	assert(cci_base);
	assert(cci_map);
	cci_init(cci_base, cci_map, map_size);
#endif
}

void fvp_interconnect_enable(void)
{
#if FVP_INTERCONNECT_DRIVER == FVP_CCN
	plat_arm_interconnect_enter_coherency();
#else
	unsigned int master;

	if (arm_config.flags & (ARM_CONFIG_FVP_HAS_CCI400 |
				ARM_CONFIG_FVP_HAS_CCI5XX)) {
		master = get_interconnect_master();
		cci_enable_snoop_dvm_reqs(master);
	}
#endif
}

void fvp_interconnect_disable(void)
{
#if FVP_INTERCONNECT_DRIVER == FVP_CCN
	plat_arm_interconnect_exit_coherency();
#else
	unsigned int master;

	if (arm_config.flags & (ARM_CONFIG_FVP_HAS_CCI400 |
				ARM_CONFIG_FVP_HAS_CCI5XX)) {
		master = get_interconnect_master();
		cci_disable_snoop_dvm_reqs(master);
	}
#endif
}
