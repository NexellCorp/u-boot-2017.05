/*
 * (C) Copyright 2018 Nexell
 * JongKeun, Choi <jkchoi@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <asm/mach-types.h>

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_ARCH_NXP3220) || defined(CONFIG_ARCH_SIP_S31NX)
#define MACH_TYPE_NUMBER	MACH_TYPE_NXP3220
#else
#define MACH_TYPE_NUMBER	2009
#endif

#if defined(CONFIG_SYS_SDRAM_SIZE)
#define SYS_SDRAM_SIZE		CONFIG_SYS_SDRAM_SIZE
#else
#error "Not defined CONFIG_SYS_SDRAM_SIZE"
#endif

int dram_init(void)
{
	gd->ram_size = SYS_SDRAM_SIZE;
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_arch_number = MACH_TYPE_NUMBER;
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x00000100;

	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size	 = SYS_SDRAM_SIZE;

	return 0;
}
