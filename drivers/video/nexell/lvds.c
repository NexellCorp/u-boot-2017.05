/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <clk.h>
#include <malloc.h>
#include <display.h>
#include <asm/io.h>
#include <dt-bindings/video/nexell.h>

#include "display.h"

DECLARE_GLOBAL_DATA_PTR;

struct nx_lvds {
	struct nx_lvds_reg *reg;
	void __iomem *phy;
	struct nx_display_info *dpi;
	struct clk vclk, phy_clk;
	/* properties */
	unsigned int format; /* 0:VESA, 1:JEIDA, 2: Location */
	int voltage_level;
	int voltage_output;
};

struct nx_lvds_reg {
	u32 lvdsctrl0;		/* 0x00 */
	u32 lvdsvblkctrl0;	/* 0x04 */
	u32 lvdsvblkctrl1;	/* 0x08 */
	u32 lvdsctrl1;		/* 0x0c */
	u32 lvdsctrl2;		/* 0x10 */
	u32 lvdsctrl3;		/* 0x14 */
	u32 lvdsctrl4;		/* 0x18 */
	u32 lvdsloc0;		/* 0x1c */
	u32 lvdsloc1;		/* 0x20 */
	u32 lvdsloc2;		/* 0x24 */
	u32 lvdsloc3;		/* 0x28 */
	u32 lvdsloc4;		/* 0x2c */
	u32 lvdsloc5;		/* 0x30 */
	u32 lvdsloc6;		/* 0x34 */
	u32 lvdsloc7;		/* 0x38 */
	u32 lvdsloc8;		/* 0x3c */
	u32 lvdsloc9;		/* 0x40 */
	u32 lvdsmask0;		/* 0x44 */
	u32 lvdsmask1;		/* 0x48 */
	u32 lvdspol0;		/* 0x4c */
	u32 lvdspol1;		/* 0x50 */
	u32 lvdsda0;		/* 0x54 */
	u32 lvdsda1;		/* 0x58 */
	u32 lvdsda2;		/* 0x5c */
};

#define	DEF_VOLTAGE_LEVEL	(0x3f) /* 8bits width */
#define	DEF_VOLTAGE_OFFSET	LVDS_VOL_OFFS_1_1
#define MHZ(v)			(v * 1000000)

static void lvds_phy_reset(struct nx_lvds *lvds)
{
	void __iomem *set = lvds->phy + 0x40;
	void __iomem *clr = lvds->phy + 0x30;

	writel(readl(set) | (1 << 12),  set);
	writel(readl(clr) | (1 << 12),  clr);
}

static int lvds_prepare(struct nx_lvds *lvds)
{
	struct nx_lvds_reg *reg = lvds->reg;
	unsigned int format = lvds->format;
	struct display_timing *timing = &lvds->dpi->timing;
	int pixelclock = timing->pixelclock.typ;
	u32 v_level = lvds->voltage_level;
	u32 v_output = lvds->voltage_output;
	u32 val, pms_v, pms_s, pms_m, pms_p;

	debug("%s: format: %d, voltage level:%d output:0x%x\n",
	      __func__, format, v_level, v_output);

	/* lvdsctrl0 */
	val = readl(&reg->lvdsctrl0);
	val &= ~((1 << 24) | (1 << 23) | (0 << 22) | (0 << 21) | (0x3 << 19));
	val |= (0 << 23) /* DE_POL */
		| (0 << 22)	/* HSYNC_POL */
		| (0 << 21)	/* VSYNC_POL */
		| (format << 19) /* LVDS_FORMAT */
		;
	writel(val, &reg->lvdsctrl0);

	/* lvdsctrl2 */
	val = readl(&reg->lvdsctrl2);
	val &= ~((1 << 15) | (1 << 14) | (0x3 << 12) | (0x3f << 6) | (0x3f << 0));
	if (pixelclock >= MHZ(90)) {
		pms_v = 1 << 14;
		pms_s = 1 << 12;
		pms_m = 0xe << 6;
		pms_p = 0xe << 0;
	} else {
		pms_v = 0 << 14;
		pms_s = 0 << 12;
		pms_m = 0xa << 6;
		pms_p = 0xa << 0;
	}
	val |= pms_v | pms_s | pms_m | pms_p;
	writel(val, &reg->lvdsctrl2);

	/* lvdsctrl4 : CNT_VOD_H and FC_CODE */
	val = readl(&reg->lvdsctrl4);
	val &= ~((0xff << 14) | (0x7 << 3));
	val |= (((v_level & 0xff) << 14) | ((v_output & 0x7) << 3));
	writel(val, &reg->lvdsctrl4);

	return 0;
}

static int lvds_enable(struct udevice *dev, int panel_bpp,
		       const struct display_timing *timing)
{
	struct nx_lvds *lvds = dev_get_priv(dev);
	struct nx_lvds_reg *reg = lvds->reg;

	/*
	 * get nx_display_info with display device's timing
	 */
	lvds->dpi = container_of(timing, struct nx_display_info, timing);

	clk_set_rate(&lvds->vclk, timing->pixelclock.typ);

	clk_enable(&lvds->vclk);
	clk_enable(&lvds->phy_clk);

	lvds_prepare(lvds);

	/* DPCENB */
	writel(readl(&reg->lvdsctrl0) | 1 << 28, &reg->lvdsctrl0);

	/* LVDS PHY Reset */
	lvds_phy_reset(lvds);

	return 0;
}

static int lvds_probe(struct udevice *dev)
{
	struct nx_lvds *lvds = dev_get_priv(dev);
	int ret;

	lvds->reg = (struct nx_lvds_reg *)devfdt_get_addr_index(dev, 0);
	lvds->phy = (void __iomem *)devfdt_get_addr_index(dev, 1);

	ret = clk_get_by_name(dev, "vclk", &lvds->vclk);
	if (ret) {
		printf("not found lvds vclk clock\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "phy", &lvds->phy_clk);
	if (ret) {
		printf("not found lvds phy clk clock\n");
		return ret;
	}

	lvds->format =
		ofnode_read_u32_default(dev_ofnode(dev),
					"format", LVDS_FORMAT_VESA);
	lvds->voltage_level =
		ofnode_read_u32_default(dev_ofnode(dev),
					"voltage-level", DEF_VOLTAGE_LEVEL);

	lvds->voltage_output =
		ofnode_read_u32_default(dev_ofnode(dev),
					"voltage-output", DEF_VOLTAGE_OFFSET);
	return ret;
}

static const struct dm_display_ops nx_lvds_ops = {
	.enable = lvds_enable,
};

static const struct udevice_id nx_lvds_ids[] = {
	{.compatible = "nexell,nxp3220-lvds", },
	{}
};

U_BOOT_DRIVER(nexell_lvds) = {
	.name = "nexell-lvds",
	.id = UCLASS_DISPLAY,
	.of_match = of_match_ptr(nx_lvds_ids),
	.ops = &nx_lvds_ops,
	.probe = lvds_probe,
	.priv_auto_alloc_size = sizeof(struct nx_lvds),
};
