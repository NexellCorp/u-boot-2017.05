/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>

#define REG_RST_CONFIG	0x2008c86c
#define BOOTMODE_MASK	0x7
#define BOOTMODE_SDMMC	0x3

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
void set_board_info(void)
{
	u32 rst_config;

	rst_config = readl(REG_RST_CONFIG);
	rst_config &= BOOTMODE_MASK;

	/* set boot device only if it was sdmmc boot */
	if (rst_config == BOOTMODE_SDMMC)
		env_set_ulong("mmc_boot_dev", 1);
}
#endif

int board_init(void)
{
	return 0;
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	set_board_info();
#endif

	return 0;
}
#endif
