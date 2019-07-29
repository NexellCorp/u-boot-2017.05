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

DECLARE_GLOBAL_DATA_PTR;

#define SRC_MAX_DIV	256
#define SYS_MAX_DIV	256
#define T_MAX_DIV	(SRC_MAX_DIV * SYS_MAX_DIV)
#define PLL_NUM		5
#define	debug	printf

struct nx_cmu_priv {
	struct clk_cmu_dev *cmus;
	struct clk_cmu_dev *parent;
	int size;
	int parent_num;
	int initialized;
};

enum nx_pll_assign {
	NX_PLL_ASSIGN_PLL,
	NX_PLL_ASSIGN_NONE,
};

struct nx_pll {
	unsigned long value;
	int divide;
	enum nx_pll_assign assign;
};

static struct nx_pll plls[] = {
	{ 0, 1, NX_PLL_ASSIGN_PLL },
	{ 0, 2, NX_PLL_ASSIGN_PLL },
	{ 0, 1, NX_PLL_ASSIGN_NONE }, /* CPU DVFS */
	{ 0, 2, NX_PLL_ASSIGN_PLL },
	{ 0, 2, NX_PLL_ASSIGN_PLL },
};

struct nx_clk_div {
	int mux;
	int parent; /* source/parent divider */
	int divide;
};

static char *get_cmu_type(int type)
{
	switch (type) {
	case CMU_NAME_SRC:
		return "SRC ";
	case CMU_NAME_SYS:
		return "SYS ";
	case CMU_NAME_USB:
		return "USB ";
	case CMU_NAME_HSIF:
		return "HSIF";
	case CMU_NAME_MM:
		return "MM  ";
	case CMU_NAME_CPU:
		return "CPU ";
	case CMU_NAME_DDR:
		return "DDR";
	default:
		return "unknown";
	}

	return NULL;
}

static int nx_pll_get_min(void)
{
	int min = 0, i;

	for (i = 0; i < PLL_NUM; i++) {
		if (!plls[i].value)
			continue;
		if (plls[i].value < plls[min].value)
			min = i;
	}

	return min;
}

static int nx_pll_get_max(void)
{
	int max = 0, i;

	for (i = 0; i < PLL_NUM; i++) {
		if (!plls[i].value)
			continue;
		if (plls[i].value > plls[max].value)
			max = i;
	}

	return max;
}

static int nx_pll_get_rate(struct udevice *dev)
{
	struct clk pll;
	int i;

	clk_get_by_index(dev, 0, &pll);

	for (i = 0; i < PLL_NUM; i++) {
		if (plls[i].assign == NX_PLL_ASSIGN_NONE)
			continue;

		pll.id = i;
		plls[i].value = clk_get_rate(&pll) / plls[i].divide;
	}

	return 0;
}

static void nx_clk_calc_divider(struct nx_clk_div *cdiv, unsigned long request)
{
	int divide = 0, select = 0, delta = 0;
	int div0 = 0, div1 = 0;
	int i, v, m, d;

	for (i = 0; i < PLL_NUM; i++) {
		if (!plls[i].value)
			continue;

		v = plls[i].value / request;
		if (!v)
			continue;

		m = plls[i].value / v - request;

		if (v > T_MAX_DIV) {
			divide = T_MAX_DIV + 1;
			continue;
		}

		if (!m) {
			select = i;
			divide = v;
			delta = m;
			break;
		}

		debug("CAL: %luhz, %8d (%8d) [%d]=%lu/%d\n",
		      request, delta, m, i, plls[i].value, v);

		if (!delta || m < delta) {
			select = i;
			divide = v;
			delta = m;
		}
	}

	if (divide > T_MAX_DIV || !divide) {
		if (!divide) {
			select = nx_pll_get_max();
			div0 = 1;
			div1 = 1;
		} else {
			select = nx_pll_get_min();
			div0 = SRC_MAX_DIV;
			div1 = SYS_MAX_DIV;
		}

		cdiv->mux = select;
		cdiv->parent = div1;
		cdiv->divide = div0;
		return;
	}

	for (d = 1; d < 255; d++) {
		v = divide / d;
		m = divide / v - d;

		if (v > SYS_MAX_DIV)
			continue;

		if (!m) {
			div0 = d;
			div1 = v;
			break;
		}

		if (!delta || m < delta) {
			div0 = d;
			div1 = v;
		}
	}

	cdiv->mux = select;
	cdiv->parent = div1;
	cdiv->divide = div0;
}

static struct clk_cmu_dev *nx_clk_get_dev(struct nx_cmu_priv *priv,
					  int id, bool src)
{
	struct clk_cmu_dev *cdev = priv->cmus;
	int size = priv->size;
	int i;

	if (src) {
		cdev = priv->parent;
		size = priv->parent_num;
	}

	for (i = 0; i < size; i++) {
		if (cdev[i].id == id)
			return &cdev[i];
	}

	return NULL;
}

static int nx_clk_enable_hw(struct nx_cmu_priv *priv, unsigned int id)
{
	struct clk_cmu_dev *cmu = nx_clk_get_dev(priv, id, false);
	unsigned int idx, bit;

	if (cmu->nc)
		return 0;

	idx = cmu->clkenbit / 32;
	bit = cmu->clkenbit % 32;

	writel(1 << bit, &cmu->reg->enb_set[idx]);

	debug("ON : %s:%3d:%p, idx:%d, bit:%d enbable\n",
	      get_cmu_type(cmu->cmu_id), cmu->id, cmu->reg, idx, bit);

	if (cmu->type == CMU_TYPE_MAINDIV) {
		cmu = nx_clk_get_dev(priv, cmu->p_id, true);
		if (!cmu)
			return 0;

		idx = cmu->clkenbit / 32;
		bit = cmu->clkenbit % 32;
		writel(1 << bit, &cmu->reg->enb_set[idx]);

		debug("ON : %s:%3d:%p, idx:%d, bit:%d enbable\n",
		      get_cmu_type(cmu->cmu_id), cmu->id, cmu->reg, idx, bit);
	}

	return 0;
}

static int nx_clk_enable(struct clk *clk)
{
	return nx_clk_enable_hw(dev_get_priv(clk->dev), clk->id);
}

static int nx_clk_disable(struct clk *clk)
{
	struct nx_cmu_priv *priv = dev_get_priv(clk->dev);
	struct clk_cmu_dev *cmu = nx_clk_get_dev(priv, clk->id, false);
	unsigned int idx, bit;

	if (cmu->nc)
		return 0;

	idx = cmu->clkenbit / 32;
	bit = cmu->clkenbit % 32;

	writel(1 << bit, &cmu->reg->enb_clr[idx]);

	debug("OFF: %s:%3d:%p, idx:%d, bit:%d disable\n",
	      get_cmu_type(cmu->cmu_id), cmu->id, cmu->reg, idx, bit);

	if (cmu->type == CMU_TYPE_MAINDIV) {
		cmu = nx_clk_get_dev(priv, cmu->p_id, true);
		if (!cmu)
			return 0;

		idx = cmu->clkenbit / 32;
		bit = cmu->clkenbit % 32;
		writel(1 << bit, &cmu->reg->enb_clr[idx]);

		debug("OFF: %s:%3d:%p, idx:%d, bit:%d disable\n",
		      get_cmu_type(cmu->cmu_id), cmu->id, cmu->reg, idx, bit);
	}

	return 0;
}

static ulong nx_clk_set_hw(struct clk_cmu_dev *src,
			   struct clk_cmu_dev *sys,
			   unsigned long freq)
{
	struct nx_clk_div cdiv;

	nx_clk_calc_divider(&cdiv, freq);

	writel((cdiv.divide - 1) & 0xFFFF, &sys->reg->div_val[0]);
	writel((cdiv.parent - 1) & 0xFFFF, &src->reg->div_val[0]);
	writel((cdiv.mux) & 0xFFFF, &src->reg->clkmux_src);

	sys->freq = plls[cdiv.mux].value / cdiv.parent / cdiv.divide;

	debug("SET: %s:%3d:%p, %s:%3d:%p mux:%d, div:%d/%d, freq:%ld\n",
	      get_cmu_type(src->cmu_id), src->id, src->reg,
	      get_cmu_type(sys->cmu_id), sys->id, sys->reg,
	      cdiv.mux, cdiv.divide, cdiv.parent, freq);

	return sys->freq;
}

static ulong nx_clk_set_rate(struct nx_cmu_priv *priv,
			     int id, unsigned long freq)
{
	struct clk_cmu_dev *sys, *src, *cmu;
	int idx, div;

	sys = nx_clk_get_dev(priv, id, false);

	switch (sys->type) {
	case CMU_TYPE_MAINDIV:
		switch (sys->cmu_id) {
		case CMU_NAME_SRC:
			break;
		case CMU_NAME_SYS:
		case CMU_NAME_MM:
		case CMU_NAME_USB:
			src = nx_clk_get_dev(priv, sys->p_id, true);
			return nx_clk_set_hw(src, sys, freq);
		}
		break;

	case CMU_TYPE_SUBDIV0:
	case CMU_TYPE_SUBDIV1:
	case CMU_TYPE_SUBDIV2:
		idx = sys->type - CMU_TYPE_SUBDIV0;
		div = 1;
		src = nx_clk_get_dev(priv, sys->p_id, false);

		if (src->freq && src->freq >= freq)
			div = src->freq / freq;

		sys->freq = src->freq / div;
		writel(div - 1, &sys->reg->div_val[idx]);

		return sys->freq;

	case CMU_TYPE_ONLYGATE:
		cmu = nx_clk_get_dev(priv, sys->p_id, false);
		src = nx_clk_get_dev(priv, cmu->p_id, true);

		if (!src->freq)
			sys->freq = nx_clk_set_hw(src, cmu, freq);

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
	struct clk_cmu_dev *sys = nx_clk_get_dev(priv, clk->id, false);
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
				nx_clk_enable_hw(priv, id);
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
				nx_clk_enable_hw(priv, id);
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
