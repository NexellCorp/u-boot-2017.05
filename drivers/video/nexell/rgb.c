/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <syscon.h>
#include <regmap.h>
#include <display.h>
#include <asm/io.h>

#include "display.h"

DECLARE_GLOBAL_DATA_PTR;

struct nx_rgb {
	struct nx_display_info *dpi;
	struct regmap *syscon;
	int mpu_lcd; /* i80 */
};

static void rgb_sel_pad(struct nx_rgb *rgb, int muxsel, int clksel)
{
	struct regmap *syscon = rgb->syscon;

	if (IS_ERR(syscon))
		return;

	regmap_update_bits(syscon, 0x100, 0x7 << 4, clksel << 4);
	regmap_update_bits(syscon, 0x100, 0x3, muxsel);
}

static int rgb_enable(struct udevice *dev, int panel_bpp,
		      const struct display_timing *timing)
{
	struct nx_rgb *rgb = dev_get_priv(dev);
	struct nx_display_par *dpp = &rgb->dpi->dpp;
	enum nx_dpc_padclk clksel = NX_DPC_PADCLK_CLK;
	enum nx_dpc_padmux muxsel = NX_DPC_PADNUX_MLC;

	/*
	 * get nx_display_info with display device's timing
	 */
	rgb->dpi = container_of(timing, struct nx_display_info, timing);

	/* set mpu(i80) LCD */
	rgb->dpi->mpu_lcd = rgb->mpu_lcd;

	if (rgb->mpu_lcd)
		muxsel = NX_DPC_PADNUX_MPU;

	if (dpp->out_format == DP_OUT_FORMAT_SRGB888)
		clksel = NX_DPC_PADCLK_CLK_DIV2_90;

	rgb_sel_pad(rgb, muxsel, clksel);

	return 0;
}

static int rgb_probe(struct udevice *dev)
{
	struct nx_rgb *rgb = dev_get_priv(dev);
	struct udevice *syscon;
	int err;

	err = uclass_get_device_by_phandle(UCLASS_SYSCON,
					   dev, "syscon", &syscon);
	if (err) {
		debug("%s: unable to find syscon (%d)\n", __func__, err);
		return err;
	}

	rgb->syscon = syscon_get_regmap(syscon);
	if (IS_ERR(rgb->syscon)) {
		debug("%s: unable to find syscon (%ld)\n",
		      __func__, PTR_ERR(rgb->syscon));
		return -EINVAL;
	}

	rgb->mpu_lcd = ofnode_read_u32_default(dev_ofnode(dev), "panel-mpu", 0);

	return 0;
}

static const struct dm_display_ops nx_rgb_ops = {
	.enable = rgb_enable,
};

static const struct udevice_id nx_rgb_ids[] = {
	{.compatible = "nexell,nxp3220-rgb", },
	{}
};

U_BOOT_DRIVER(nexell_rgb) = {
	.name = "nexell-rgb",
	.id = UCLASS_DISPLAY,
	.of_match = of_match_ptr(nx_rgb_ids),
	.ops = &nx_rgb_ops,
	.probe = rgb_probe,
	.priv_auto_alloc_size = sizeof(struct nx_rgb),
};
