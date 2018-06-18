/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>

#ifdef CONFIG_DM_PMIC_SM5011
#include <dm.h>
#include <dm/uclass-internal.h>
#include <power/pmic.h>
#include <power/sm5011.h>
#endif

#ifdef CONFIG_DM_REGULATOR_SM5011
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
#ifdef CONFIG_DM_REGULATOR_SM5011
	struct dm_regulator_uclass_platdata *uc_pdata;
	struct udevice *dev;
	struct udevice *regulator;
#endif
	int ret = -ENODEV;

	debug("%s:%d\n", __func__, __LINE__);

	ret = pmic_get("sm5011@47", &pmic);
	if (ret)
		printf("Can't get PMIC: %s!\n", "sm5011@47");

#ifdef CONFIG_DM_REGULATOR_SM5011
	if (device_has_children(pmic)) {
		for (ret = uclass_find_first_device(UCLASS_REGULATOR, &dev);
			dev;
			ret = uclass_find_next_device(&dev)) {
			if (ret)
				continue;

			uc_pdata = dev_get_uclass_platdata(dev);
			if (!uc_pdata)
				continue;

			uclass_get_device_tail(dev, 0, &regulator);
		}
	}
#endif
}
#endif

