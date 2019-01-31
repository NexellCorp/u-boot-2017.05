// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 Samsung Electronics
 * Chanho Park <chanho61.park@samsung.com>
 *
 */
#include <common.h>
#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <linux/err.h>
#include "boot_mode.h"

static struct regmap *get_boot_mode_regmap(unsigned int *offset)
{
	int node, ret;
	struct fdtdec_phandle_args args;
	struct udevice *syscon;
	struct regmap *regmap;

	node = fdt_node_offset_by_compatible(gd->fdt_blob, 0,
					     "nexell,boot-mode");
	if (node < 0)
		return NULL;

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, node, "syscon",
					     NULL, 0, 0, &args);
	if (ret) {
		pr_err("%s: Cannot get syscon phandle: ret=%d\n", __func__,
		       ret);
		return NULL;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_SYSCON, args.node,
					     &syscon);
	if (ret) {
		pr_err("%s: Cannot get SYSCON: ret=%d\n", __func__, ret);
		return NULL;
	}

	regmap = syscon_get_regmap(syscon);
	if (!regmap) {
		pr_err("unable to find regmap\n");
		return NULL;
	}

	*offset = ofnode_read_u32_default(offset_to_ofnode(node), "offset", 0);

	return regmap;
}

int check_boot_mode(void)
{
	int ret;
	struct regmap *regmap;
	unsigned int offset;
	unsigned int boot_mode;

	regmap = get_boot_mode_regmap(&offset);
	if (!regmap) {
		pr_err("unable to retrieve regmap\n");
		return -ENODEV;
	}

	ret = regmap_read(regmap, offset, &boot_mode);
	if (ret) {
		pr_err("unable to read regmap\n");
		return -ENODEV;
	}

	/* Clear boot mode once read */
	regmap_write(regmap, offset, BOOT_NORMAL);

	switch (boot_mode) {
	case BOOT_FASTBOOT:
		env_set("preboot", "fastboot 0");
		break;
	case BOOT_RECOVERY:
		/* TODO: enter recovery mode */
		break;
	case BOOT_DFU:
		env_set("preboot", "dfu 0 mmc 0");
		break;
	}

	return 0;
}

int fb_set_reboot_flag(void)
{
	struct regmap *regmap;
	unsigned int offset;

	regmap = get_boot_mode_regmap(&offset);
	if (!regmap) {
		pr_err("unable to retrieve regmap\n");
		return -ENODEV;
	}

	regmap_write(regmap, offset, BOOT_FASTBOOT);

	return 0;
}
