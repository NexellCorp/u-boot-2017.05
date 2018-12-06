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

#define T_MAX_DIV (SRC_MAX_DIV * SYS_MAX_DIV)

#define PLL_NUM 5

DECLARE_GLOBAL_DATA_PTR;

struct nx_clk_div {
	int mux;
	int p_div; /* source/parent divider */
	int d_div;
};

struct nx_cmu_priv {
	struct clk_cmu_dev *cmus;
	struct clk_cmu_dev *parent;
	int size;
	int parent_num;
	int initialized;
};

static unsigned long plls[] = { 400000000, 0, 0, 0, 0};

static int nx_pll_get_min(void)
{
	int min = 0, i;

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;
		if (plls[i] < plls[min])
			min = i;
	}
	return min;
}

static int nx_pll_get_max(void)
{
	int max = 0, i;

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;
		if (plls[i] > plls[max])
			max = i;
	}
	return max;
}

static int nx_pll_get_use_ddr(void)
{
	struct clk_cmu_reg *reg =
		(struct clk_cmu_reg *)PHY_BASEADDR_CMU_DDR_MODULE;

	return readl(&reg->clkmux_src) + 3;
}

static int nx_pll_get_rate(struct udevice *dev)
{
	struct clk pll;
	int i, ddr = nx_pll_get_use_ddr();

	clk_get_by_index(dev, 0, &pll);

	for (i = 0; i < PLL_NUM; i++) {
		if (i == ddr || i == CPU_PLL)
			continue;

		pll.id = i;
		plls[i] = clk_get_rate(&pll);
		if (i == 3 || i == 4)
			plls[i] = plls[i] / 2;
	}

	return 0;
}

static void nx_clk_calc_divider(unsigned long req, struct nx_clk_div *cdiv)
{
	int div = 0, mux = 0, dlt = 0;
	int div0 = 0, div1 = 0;
	int v, m, i, d;

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i] == 0)
			continue;

		v = plls[i] / req;
		m = plls[i] % req;

		/* under the min div_val */
		if (v == 0)
			continue;

		if (v > T_MAX_DIV) {
			div = T_MAX_DIV + 1;
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

	if (div > T_MAX_DIV || div == 0) {
		if (div == 0) {
			mux = nx_pll_get_max();
			div0 = 1;
			div1 = 1;
		} else {
			mux = nx_pll_get_min();
			div0 = SRC_MAX_DIV;
			div1 = SYS_MAX_DIV;
		}

		cdiv->mux = mux;
		cdiv->p_div = div1;
		cdiv->d_div = div0;
		return;
	}

	for (d = 1; d < 255; d++) {
		v = div / d;
		m = div % d;

		if (v > SYS_MAX_DIV)
			continue;

		if (m == 0) {
			div0 = d; div1 = v;
			break;
		}

		if (dlt == 0 || m < dlt) {
			div0 = d; div1 = v;
		}
	}

	cdiv->mux = mux;
	cdiv->p_div = div1;
	cdiv->d_div = div0;
}

static struct clk_cmu_dev *nx_clk_get_priv(struct nx_cmu_priv *priv, int id)
{
	int i;

	for (i = 0; i < priv->size; i++) {
		if (priv->cmus[i].id == id)
			return &priv->cmus[i];
	}

	return NULL;
}

static struct clk_cmu_dev *nx_clk_get_src_priv(struct nx_cmu_priv *priv, int id)
{
	int i;

	for (i = 0; i < priv->parent_num; i++) {
		if (priv->parent[i].id == id)
			return &priv->parent[i];
	}

	return NULL;
}

static int nx_clk_enable_all(struct nx_cmu_priv *priv, unsigned int id)
{
	struct clk_cmu_dev *cmu = nx_clk_get_priv(priv, id);
	unsigned int idx, bit;

	if (cmu->nc)
		return 0;

	idx = cmu->clkenbit / 32;
	bit = cmu->clkenbit % 32;

	writel(1 << bit, &cmu->reg->clkenb_set[idx]);

	if (cmu->type == CMU_TYPE_MAINDIV) {
		cmu = nx_clk_get_src_priv(priv, cmu->p_id);
		if (!cmu)
			return 0;

		idx = cmu->clkenbit / 32;
		bit = cmu->clkenbit % 32;
		writel(1 << bit, &cmu->reg->clkenb_set[idx]);
	}

	return 0;
}

static int nx_clk_enable(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);

	return nx_clk_enable_all(priv, clk->id);
}

static int nx_clk_disable(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	struct clk_cmu_dev *sys = nx_clk_get_priv(priv, clk->id);
	struct clk_cmu_dev *src = NULL;
	unsigned int idx, bit;

	if (sys->nc)
		return 0;

	idx = sys->clkenbit / 32;
	bit = sys->clkenbit % 32;

	writel(1 << bit, &sys->reg->clkenb_clr[idx]);

	if (sys->type == CMU_TYPE_MAINDIV)
		src = nx_clk_get_src_priv(priv, sys->p_id);

	if (src) {
		idx = src->clkenbit / 32;
		bit = src->clkenbit % 32;
		writel(1 << bit, &src->reg->clkenb_clr[idx]);
	}

	return 0;
}

static ulong nx_clk_set_reg(struct clk_cmu_dev *sys,
			    struct clk_cmu_dev *src,
			    unsigned long freq)
{
	struct nx_clk_div cdiv;

	nx_clk_calc_divider(freq, &cdiv);

	writel((cdiv.d_div - 1) & 0xFFFF, &sys->reg->div_val[0]);
	writel((cdiv.p_div - 1) & 0xFFFF, &src->reg->div_val[0]);
	writel((cdiv.mux) & 0xFFFF, &src->reg->clkmux_src);

	sys->freq = plls[cdiv.mux] / cdiv.p_div / cdiv.d_div;

	return sys->freq;
}

static ulong nx_clk_set_rate(struct nx_cmu_priv *priv,
			     int id, unsigned long freq)
{
	struct clk_cmu_dev *sys, *src, *par;
	int idx, div;

	sys = nx_clk_get_priv(priv, id);

	switch (sys->type) {
	case CMU_TYPE_MAINDIV:
		switch (sys->cmu_id) {
		case CMU_NAME_SRC:
			break;
		case CMU_NAME_SYS:
		case CMU_NAME_MM:
		case CMU_NAME_USB:
			src = nx_clk_get_src_priv(priv, sys->p_id);
			return nx_clk_set_reg(sys, src, freq);
		}
		break;

	case CMU_TYPE_SUBDIV0:
	case CMU_TYPE_SUBDIV1:
	case CMU_TYPE_SUBDIV2:
		idx = sys->type - CMU_TYPE_SUBDIV0;
		div = 1;
		src = nx_clk_get_priv(priv, sys->p_id);

		if (src->freq && src->freq >= freq)
			div = src->freq / freq;

		sys->freq = src->freq / div;
		writel(div - 1, &sys->reg->div_val[idx]);

		return sys->freq;

	case CMU_TYPE_ONLYGATE:
		par = nx_clk_get_priv(priv, sys->p_id);
		src = nx_clk_get_src_priv(priv, par->p_id);

		if (!src->freq)
			sys->freq = nx_clk_set_reg(par, src, freq);

		return sys->freq;
	}
	return 0;
}

static ulong nx_cmu_set_rate(struct clk *clk, unsigned long freq)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);

	return nx_clk_set_rate(priv, clk->id, freq);
}

static ulong nx_cmu_get_rate(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	struct clk_cmu_dev *sys = nx_clk_get_priv(priv, clk->id);
	unsigned long rate = 0;

	switch (sys->type) {
	case CMU_TYPE_MAINDIV:
		if (sys->cmu_id != CMU_NAME_SRC)
			rate = sys->freq;
		break;
	case CMU_TYPE_SUBDIV0:
	case CMU_TYPE_SUBDIV1:
	case CMU_TYPE_SUBDIV2:
		rate = sys->freq;
		break;
	case CMU_TYPE_ONLYGATE:
		rate = sys->freq;
		break;
	}

	return rate;
}

static int nx_cmu_parse_dt(struct udevice *dev)
{
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	const fdt32_t *list;
	int id, rate, on;
	int length, i;

	list = ofnode_get_property(dev_ofnode(dev),
				   "bus-init-frequency", &length);
	if (list) {
		length /= (sizeof(*list) * 3);
		for (i = 0; i < length; i++) {
			id = fdt32_to_cpu(*list++);
			rate = fdt32_to_cpu(*list++);
			on = fdt32_to_cpu(*list++);

			nx_clk_set_rate(priv, id, rate);
			if (on)
				nx_clk_enable_all(priv, id);
		}
	}

	list = ofnode_get_property(dev_ofnode(dev),
				   "clk-init-frequency", &length);
	if (list) {
		length /= (sizeof(*list) * 3);
		for (i = 0; i < length; i++) {
			id = fdt32_to_cpu(*list++);
			rate = fdt32_to_cpu(*list++);
			on = fdt32_to_cpu(*list++);

			nx_clk_set_rate(priv, id, rate);
			if (on)
				nx_clk_enable_all(priv, id);
		}
	}

	return 0;
}

static int clk_cmu_mm_probe(struct udevice *dev)
{
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	unsigned int reg = devfdt_get_addr(dev);
	struct clk src;
	int i;

	if (priv->initialized)
		return 0;

	priv->cmus = cmu_mm;
	priv->size = ARRAY_SIZE(cmu_mm);
	priv->parent = cmu_src;
	priv->parent_num = ARRAY_SIZE(cmu_src);

	for (i = 0; i < priv->size; i++)
		priv->cmus[i].reg = (void *)priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_cmu_parse_dt(dev);
	priv->initialized = 1;

	return 0;
}

static int clk_cmu_usb_probe(struct udevice *dev)
{
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	unsigned int reg = devfdt_get_addr(dev);
	struct clk src;
	int i;

	if (priv->initialized)
		return 0;

	priv->cmus = cmu_usb;
	priv->size = ARRAY_SIZE(cmu_usb);
	priv->parent = cmu_src;
	priv->parent_num = ARRAY_SIZE(cmu_src);

	for (i = 0; i < priv->size; i++)
		priv->cmus[i].reg = (void *)priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_cmu_parse_dt(dev);
	priv->initialized = 1;

	return 0;
}

static int clk_cmu_sys_probe(struct udevice *dev)
{
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	unsigned int reg = devfdt_get_addr(dev);
	struct clk src;
	int i;

	if (priv->initialized)
		return 0;

	priv->cmus = cmu_sys;
	priv->size = ARRAY_SIZE(cmu_sys);
	priv->parent = cmu_src;
	priv->parent_num = ARRAY_SIZE(cmu_src);

	for (i = 0; i < priv->size; i++)
		priv->cmus[i].reg = (void *)priv->cmus[i].reg + reg;

	clk_get_by_index(dev, 0, &src);
	nx_cmu_parse_dt(dev);
	priv->initialized = 1;

	return 0;
}

static int nx_cmu_src_probe(struct udevice *dev)
{
	struct nx_cmu_priv *priv = dev_get_priv(dev);
	unsigned int reg = devfdt_get_addr(dev);
	int i;

	if (priv->initialized)
		return 0;

	priv->cmus = cmu_src;
	priv->size = ARRAY_SIZE(cmu_src);

	for (i = 0; i < priv->size; i++)
		priv->cmus[i].reg = (void *)priv->cmus[i].reg + reg;

	nx_pll_get_rate(dev);
	priv->initialized = 1;

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
	.probe = nx_cmu_src_probe,
	.ops = &nx_cmu_src_ops,
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
	.priv_auto_alloc_size = sizeof(struct nx_cmu_priv),
};
