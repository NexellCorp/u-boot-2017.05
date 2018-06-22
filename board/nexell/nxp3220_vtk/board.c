/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>

#ifdef CONFIG_DM_PMIC_SM5011
#include <power/pmic.h>
#include <power/regulator.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

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

