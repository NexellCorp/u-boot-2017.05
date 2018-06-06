/*
 * Nexell PLL helper functions for clock drivers.
 * Copyright (C) 2018 Nexell .
 * Youngbok Park<ybpark@nxell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <clk-uclass.h>
#include <asm/io.h>
#include "clk-nxp3220.h"
#include "clk-init-nxp3220.c"

#define SRC_MAX_DIV 256
#define SYS_MAX_DIV 256

#define T_MAX_DIV SRC_MAX_DIV * SYS_MAX_DIV;

#define PLL_NUM 5

DECLARE_GLOBAL_DATA_PTR;

struct nx_clk_div {
	int mux;
	int div_s;
	int div_o;
};

struct nx_cmu_priv {
	struct clk_cmu_dev *cmus;
	struct clk_cmu_dev *src_cmu;
	int size;
	int p_size;
};

static unsigned long plls[] = { 400000000,
			0,
			0,
			0,
			0};

static int nx_get_min_pll(void)
{
	int i;
	int min = 0;

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;
		if (plls[i] < plls[min])
			min = i;
	}

	return min;
}

static  int get_max_pll(void)
{
	int i;
	int max = 0;

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;
		if (plls[i] > plls[max])
			max = i;
	}

	return max;
}

static int nx_calc_divisor(unsigned long req, struct nx_clk_div *cdiv)
{
	int div_c_max = SYS_MAX_DIV;
	int div_max = T_MAX_DIV;
	int div = 0; int mux = 0; int dlt = 0;
	int div0 = 0, div1 = 0;
	int v, m, i, d;

	for (i = 1; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;
		v = plls[i] / req;
		m = plls[i] % req;

		/* under the min divider */
		if (v == 0) {
			div = 0;
			continue;
		}

		if (v > div_max) {
			div = div_max+1;
			continue;
		}

		if (m == 0) {
			mux = i; div = v; dlt = m;
			break;
		}

		if (dlt == 0 || m < dlt) {
			mux = i; div = v; dlt = m;
		}
	}

	if (div > div_max || div == 0) {
		if (div == 0) {
			mux = get_max_pll();
			div0 = 1;
			div1 = 1;
		} else {
			mux = nx_get_min_pll();
			div0 = SRC_MAX_DIV;
			div1 = SYS_MAX_DIV;
		}

		cdiv->mux = mux;
		cdiv->div_s = div0;
		cdiv->div_o = div1;
		return 0;
	}

	for (d = 1; d < 255; d++) {
		v = div / d;
		m = div % d;

		if (v > div_c_max)
			continue;

		if (m == 0) {
			div0 = d;
			div1 = v;
			break;
		}
		if (dlt == 0 || m < dlt) {
			div0 = d; div1 = v;
		}
	}
	cdiv->mux = mux;
	cdiv->div_s = div0;
	cdiv->div_o = div1;
	return 0;
}

static struct clk_cmu_dev *nx_get_clk_priv(struct nx_cmu_priv *priv, int id)
{
	int i;

	for (i = 0; priv->size; i++) {
		if (priv->cmus[i].id == id)
			return (struct clk_cmu_dev *)&priv->cmus[i].reg;
	}
	return (void *)NULL;
}

static struct clk_cmu_dev *nx_get_clk_src_priv(struct nx_cmu_priv *priv, int id)
{
	int i;

	for (i = 0; priv->p_size; i++) {
		if (priv->src_cmu[i].id == id)
			return (struct clk_cmu_dev *)&priv->src_cmu[i].reg;
	}
	return (void *)NULL;
}

static int clk_grp_enable(struct nx_cmu_priv *priv, unsigned int id)
{
	struct clk_cmu_dev *sys = nx_get_clk_priv(priv, id);
	struct clk_cmu_dev *src = nx_get_clk_priv(priv, sys->p_id);
	unsigned int reg_idx = sys->clkenbit / 32;
	unsigned int reg_bit = sys->clkenbit % 32;
	unsigned int src_reg_idx = src->clkenbit / 32;
	unsigned int src_bit = src->clkenbit % 32;

	if (sys->nc) {
		printf("CMU %d  is not en/disable\n", id);
		return -EINVAL;
	}
	writel(1 << reg_bit, &sys->reg->grp_clkenb[reg_idx]);
	writel(1 << src_bit, &src->reg->grp_clkenb[src_reg_idx]);

	return 0;
}

static int nx_clk_enable(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	clk_grp_enable(priv, clk->id);

	return 0;
}

static int nx_clk_disable(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	struct clk_cmu_dev *sys = nx_get_clk_priv(priv, clk->id);
	struct clk_cmu_dev *src = nx_get_clk_priv(priv, sys->p_id);
	unsigned int reg_idx = sys->clkenbit / 32;
	unsigned int reg_bit = sys->clkenbit % 32;
	unsigned int src_reg_idx = src->clkenbit / 32;
	unsigned int src_bit = src->clkenbit % 32;

	if (sys->nc) {
		printf("CMU %ld  is not disable\n", clk->id);
		return -EINVAL;
	}
	writel(1<<reg_bit, &sys->reg->grp_clkenb_clr[reg_idx]);
	writel(1<<src_bit, &src->reg->grp_clkenb_clr[src_reg_idx]);

	return 0;
}

static ulong set_rate(struct clk_cmu_dev *sys,  struct clk_cmu_dev *src,
			  unsigned long freq)
{
	unsigned long cal_freq;
	struct nx_clk_div cdiv;

	nx_calc_divisor(freq, &cdiv);

	writel((cdiv.mux - 1) & 0xFFFF, &src->reg->grp_clk_src);
	writel((cdiv.div_s - 1) & 0xFFFF, &src->reg->divider[0]);
	writel((cdiv.div_o - 1) & 0xFFFF, &sys->reg->divider[0]);

	cal_freq = plls[cdiv.mux] / cdiv.div_s / cdiv.div_o;
	sys->c_freq = cal_freq;

	return cal_freq;
}

static ulong nx_set_rate(struct nx_cmu_priv *priv , int id, unsigned long freq)
{
	struct clk_cmu_dev *sys = nx_get_clk_priv(priv, id);
	struct clk_cmu_dev *par;
	struct clk_cmu_dev *src;
	unsigned long cal_freq;
	int div, div_index;

	switch (sys->type) {
	case CMU_TYPE_MAINDIV:
		switch (sys->cmu_id) {
		case CMU_NAME_SRC:
			break;
		case CMU_NAME_SYS:
		case CMU_NAME_MM:
		case CMU_NAME_USB:
			src = nx_get_clk_src_priv(priv, sys->p_id);
			cal_freq = set_rate(sys, src, freq);
			return cal_freq;
			break;
		}
		break;
	case CMU_TYPE_SUBDIV0:
	case CMU_TYPE_SUBDIV1:
	case CMU_TYPE_SUBDIV2:
		src = nx_get_clk_priv(priv, sys->p_id);
		div_index = sys->type - CMU_TYPE_SUBDIV0;
		if (!src->c_freq || src->c_freq < freq) {
			div = 1;
			cal_freq = src->c_freq;
		} else {
			div = src->c_freq / freq;
			cal_freq = src->c_freq / div;
			sys->c_freq = cal_freq;
		}
		writel(div - 1, &sys->reg->divider[div_index]);
		return cal_freq;
		break;
	case CMU_TYPE_ONLYGATE:
		par = nx_get_clk_priv(priv, sys->p_id);
		src = nx_get_clk_src_priv(priv, par->p_id);
		if (!src->c_freq)
			set_rate(par, src, freq);
		else
			sys->c_freq = src->c_freq;
		return sys->c_freq;
		break;
	}
	return 0;
}

static ulong nx_cmu_set_rate(struct clk *clk, unsigned long freq)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);

	return nx_set_rate(priv, clk->id, freq);
}

static ulong nx_cmu_get_rate(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	struct clk_cmu_dev *sys = nx_get_clk_priv(priv, clk->id);
	unsigned long rate = 0;

	switch (sys->type) {
	case CMU_TYPE_MAINDIV:
		if (sys->cmu_id != CMU_NAME_SRC)
			rate = sys->c_freq;
		break;
	case CMU_TYPE_SUBDIV0:
	case CMU_TYPE_SUBDIV1:
	case CMU_TYPE_SUBDIV2:
		rate = sys->c_freq;
		break;
	case CMU_TYPE_ONLYGATE:
		rate = sys->c_freq;
		break;
	}
	return rate;
}

static int nx_get_use_ddr_pll(void)
{
	struct clk_cmu_reg *reg =
		(struct clk_cmu_reg *)PHY_BASEADDR_CMU_DDR_MODULE;

	return readl(&reg->grp_clk_src) + 3;
}

static int nx_get_pll_rate(struct udevice *dev)
{
	struct clk pll;
	int i, ddr = nx_get_use_ddr_pll();

	clk_get_by_index(dev, 0, &pll);

	for (i = 0; i < PLL_NUM; i++) {
		if (i == ddr || i == CPU_PLL)
			continue;
		pll.id = i;
		plls[i] = clk_get_rate(&pll);
		if (i == 3 || i == 4)
			plls[i] = plls[i]/2;
	}

	return 0;
}

static int nx_parse_cmu_dt(struct udevice *dev)
{
	const void *blob = gd->fdt_blob;
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	int node = dev_of_offset(dev);
	const fdt32_t *list;
	unsigned int init[128][2];
	int length, i, en;

	list = fdt_getprop(blob, node, "bus-init-frequency", &length);
	if (list) {
		length /= sizeof(*list);
		for (i = 0; i < length/3; i++) {
			init[i][0] = fdt32_to_cpu(*list++);
			init[i][1] = fdt32_to_cpu(*list++);
			en = fdt32_to_cpu(*list++);

			nx_set_rate(priv, init[i][0], init[i][1]);
			if (en)
				clk_grp_enable(priv, init[i][0]);
		}
	}

	list = fdt_getprop(blob, node, "clk-init-frequency", &length);
	if (list) {
		length /= sizeof(*list);
		for (i = 0; i < length/3; i++) {
			init[i][0] = fdt32_to_cpu(*list++);
			init[i][1] = fdt32_to_cpu(*list++);
			en = fdt32_to_cpu(*list++);

			nx_set_rate(priv, init[i][0], init[i][1]);
			if (en)
				clk_grp_enable(priv, init[i][0]);
		}
	}

	return 0;
}

static int clk_cmu_mm_probe(struct udevice *dev)
{
	struct nx_cmu_priv *mm_priv = dev_get_priv(dev);
	unsigned int reg = dev_get_addr(dev);
	struct clk src;
	int i;

	mm_priv->cmus = (struct clk_cmu_dev *)&cmu_mm;
	mm_priv->size = ARRAY_SIZE(cmu_mm);
	mm_priv->src_cmu = (struct clk_cmu_dev *)&cmu_src;
	mm_priv->p_size = ARRAY_SIZE(cmu_src);

	for (i = 0; i < mm_priv->size; i++)
		mm_priv->cmus[i].reg = (void *)mm_priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_parse_cmu_dt(dev);

	return 0;
}

static int clk_cmu_usb_probe(struct udevice *dev)
{
	struct nx_cmu_priv *usb_priv = dev_get_priv(dev);
	unsigned int reg = dev_get_addr(dev);
	struct clk src;
	int i;

	usb_priv->cmus = (struct clk_cmu_dev *)&cmu_usb;
	usb_priv->size = ARRAY_SIZE(cmu_usb);
	usb_priv->src_cmu = (struct clk_cmu_dev *)&cmu_src;
	usb_priv->p_size = ARRAY_SIZE(cmu_src);

	for (i = 0; i < usb_priv->size; i++)
		usb_priv->cmus[i].reg =
			(void *)usb_priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_parse_cmu_dt(dev);

	return 0;
}

static int clk_cmu_sys_probe(struct udevice *dev)
{
	struct nx_cmu_priv *sys_priv = dev_get_priv(dev);
	unsigned int reg = dev_get_addr(dev);
	struct clk src;
	int i;

	sys_priv->cmus = (struct clk_cmu_dev *)&cmu_sys;
	sys_priv->size = ARRAY_SIZE(cmu_sys);
	sys_priv->src_cmu = (struct clk_cmu_dev *)&cmu_src;
	sys_priv->p_size = ARRAY_SIZE(cmu_src);

	for (i = 0; i < sys_priv->size; i++)
		sys_priv->cmus[i].reg =	(void *)sys_priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_parse_cmu_dt(dev);

	return 0;
}

static int nx_clk_src_probe(struct udevice *dev)
{
	struct nx_cmu_priv *src_priv = dev_get_priv(dev);
	unsigned int reg = dev_get_addr(dev);
	int i;

	src_priv->cmus = (struct clk_cmu_dev *)&cmu_src;
	src_priv->size = ARRAY_SIZE(cmu_src);

	for (i = 0; i < src_priv->size; i++)
		src_priv->cmus[i].reg = (void *)src_priv->cmus[i].reg + reg;

	nx_get_pll_rate(dev);

	return 0;
}

static struct clk_ops nx_cmu_src_ops = {
	.get_rate	= nx_cmu_get_rate,
	.set_rate	= nx_cmu_set_rate,
	.enable		= nx_clk_enable,
	.disable	= nx_clk_disable,
};

static const struct udevice_id nx_cmu_src_compat[] = {
	{ .compatible = "nexell,nxp3220-cmu-src" },
	{ }
};

U_BOOT_DRIVER(nx_cmu_src) = {
	.name = "nx-cmu-src",
	.id = UCLASS_CLK,
	.of_match = nx_cmu_src_compat,
	.probe = nx_clk_src_probe,
	.ops = &nx_cmu_src_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size = sizeof(struct nx_cmu_priv),
};

static struct clk_ops nx_cmu_sys_ops = {
	.get_rate	= nx_cmu_get_rate,
	.set_rate	= nx_cmu_set_rate,
	.enable		= nx_clk_enable,
	.disable	= nx_clk_disable,
};

static const struct udevice_id nx_cmu_sys_compat[] = {
	{ .compatible = "nexell,nxp3220-cmu-sys" },
	{ }
};

U_BOOT_DRIVER(nx_cmu_sys) = {
	.name = "nx-cmu-sys",
	.id = UCLASS_CLK,
	.of_match = nx_cmu_sys_compat,
	.probe = clk_cmu_sys_probe,
	.ops = &nx_cmu_sys_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size = sizeof(struct nx_cmu_priv),
};

static struct clk_ops nx_cmu_mm_ops = {
	.get_rate	= nx_cmu_get_rate,
	.set_rate	= nx_cmu_set_rate,
	.enable		= nx_clk_enable,
	.disable	= nx_clk_disable,
};

static const struct udevice_id nx_cmu_mm_compat[] = {
	{ .compatible = "nexell,nxp3220-cmu-mm" },
	{ }
};

U_BOOT_DRIVER(nx_cmu_mm) = {
	.name = "nx-cmu-mm",
	.id = UCLASS_CLK,
	.of_match = nx_cmu_mm_compat,
	.probe = clk_cmu_mm_probe,
	.ops = &nx_cmu_mm_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size = sizeof(struct nx_cmu_priv),
};

static struct clk_ops nx_cmu_usb_ops = {
	.get_rate	= nx_cmu_get_rate,
	.set_rate	= nx_cmu_set_rate,
	.enable		= nx_clk_enable,
	.disable	= nx_clk_disable,
};

static const struct udevice_id nx_cmu_usb_compat[] = {
	{ .compatible = "nexell,nxp3220-cmu-usb" },
	{ }
};

U_BOOT_DRIVER(nx_cmu_usb) = {
	.name = "nx-cmu-usb",
	.id = UCLASS_CLK,
	.of_match = nx_cmu_usb_compat,
	.probe = clk_cmu_usb_probe,
	.ops = &nx_cmu_usb_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size = sizeof(struct nx_cmu_priv),
};
