/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>

#ifdef CONFIG_DM_PMIC_SM5011
#include <power/pmic.h>
#include <power/regulator.h>
#endif

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

#ifdef CONFIG_DM_PMIC_SM5011
int power_init_board(void)
{
	struct udevice *pmic;
	int ret = -ENODEV;

	debug("%s:%d\n", __func__, __LINE__);

	ret = pmic_get("sm5011@47", &pmic);
	if (ret)
		printf("Can't get PMIC: %s!\n", "sm5011@47");

	debug("%s:%d, regulators_enable_boot_on() start\n", __func__, __LINE__);

	ret = regulators_enable_boot_on(false);
	if (ret)
		return ret;
	debug("%s:%d, regulators_enable_boot_on() end\n", __func__, __LINE__);

	return 0;
}
#endif
