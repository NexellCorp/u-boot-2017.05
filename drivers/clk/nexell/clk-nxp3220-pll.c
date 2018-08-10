/*
 * Nexell PLL helper functions for clock drivers.
 * Copyright (C) 2018 Nexell .
 * Author: Youngbok, Park <ybpark@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <clk-uclass.h>
#include <asm/io.h>
#include "clk-nxp3220.h"

DECLARE_GLOBAL_DATA_PTR;

#define PLL_LOCK_COUNT		0x200

#define PLL_DIRTYFLAG		(1 << 1)
#define PLL_RUN_CHANGE		(1 << 0)
#define PLL_UPDATE_DIRECT	(1 << 15)

#define PMS_RATE(p, i) ((&p[i])->rate)
#define PMS_P(p, i) ((&p[i])->P)
#define PMS_M(p, i) ((&p[i])->M)
#define PMS_S(p, i) ((&p[i])->S)
#define PMS_K(p, i) ((&p[i])->K)


#define PHY_BASEADDR_PLL0			0x27020000
#define PHY_BASEADDR_PLL1			0x27030000
#define PHY_BASEADDR_PLL_CPU			0x22070000
#define PHY_BASEADDR_PLL_DDR0			0x23070000
#define PHY_BASEADDR_PLL_DDR1			0x23080000

#define PLL_LOCK_SHIFT		6
#define PLL_CURST_SHIFT		8
#define PLL_MUXSEL_SHIFT	3

#define PLL_P_BITPOS 0
#define PLL_M_BITPOS 16
#define PLL_S_BITPOS 0
#define PLL_K_BITPOS 16

#define PLL_TYPE_2555 0
#define PLL_TYPE_2651 1

#define PLL_RESETB_SET		0
#define PLL_RESETB_CLEAR	1

#define PLL_INIT(clk_num, addr)	\
{				\
	.reg = addr,		\
}

/* PLL(Phase Locked Loop) Register Map */
struct nx_pll_reg {
	unsigned int pll_ctrl;
	unsigned int pll_dbg0;
	unsigned int rsv0[2];
	unsigned int pll_cnt0;
	unsigned int pll_cnt1;
	unsigned int pll_cnt2;
	unsigned int pll_cnt3;
	unsigned int pll_cfg0;
	unsigned int rsv1[3];
	unsigned int pll_cfg1;
	unsigned int rsv2[3];
	unsigned int pll_cfg2;
	unsigned int rsv3[3];
	unsigned int pll_ctrl1;
	unsigned int rsv4[3];
	unsigned int pll_lockinfo;
};

enum oscmux {
	PLL_MUX_OSCCLK,
	PLL_MUX_PLL_FOUT,
};

struct clk_priv {
	struct nx_pll_reg *reg;
	unsigned long rate;
	int p, m, s, k;
};

struct cmu_platdata {
	int initialized;
};

static unsigned long ref_clk = 24000000UL;

struct pll_pms {
	long rate;
	int P;
	int M;
	int S;
	int K;
};

static struct pll_pms pll_2555_pms[] = {
	{ .rate = 2000000, .P = 3, .M = 250, .S = 0, .K = 0 },
	{ .rate = 1900000, .P = 6, .M = 475, .S = 0, .K = 0 },
	{ .rate = 1800000, .P = 3, .M = 225, .S = 0, .K = 0 },
	{ .rate = 1700000, .P = 6, .M = 425, .S = 0, .K = 0 },
	{ .rate = 1600000, .P = 3, .M = 200, .S = 0, .K = 0 },
	{ .rate = 1500000, .P = 4, .M = 250, .S = 0, .K = 0 },
	{ .rate = 1400000, .P = 3, .M = 175, .S = 0, .K = 0 },
	{ .rate = 1300000, .P = 6, .M = 325, .S = 0, .K = 0 },
	{ .rate = 1200000, .P = 3, .M = 300, .S = 1, .K = 0 },
	{ .rate = 1100000, .P = 3, .M = 275, .S = 1, .K = 0 },
	{ .rate = 1000000, .P = 3, .M = 250, .S = 1, .K = 0 },
	{ .rate =  900000, .P = 3, .M = 225, .S = 1, .K = 0 },
	{ .rate =  800000, .P = 3, .M = 200, .S = 1, .K = 0 },
	{ .rate =  700000, .P = 3, .M = 175, .S = 1, .K = 0 },
	{ .rate =  600000, .P = 3, .M = 200, .S = 2, .K = 0 },
	{ .rate =  500000, .P = 3, .M = 250, .S = 2, .K = 0 },
	{ .rate =  400000, .P = 3, .M = 200, .S = 2, .K = 0 },
	{ .rate =  300000, .P = 3, .M = 300, .S = 3, .K = 0 },
	{ .rate =  200000, .P = 3, .M = 200, .S = 3, .K = 0 },
	{ .rate =  100000, .P = 3, .M = 200, .S = 4, .K = 0 },
};

static struct pll_pms pll_2651_pms[] = {
	{ .rate = 1000000, .P = 3, .M = 250, .S = 0, .K = 0 },
	{ .rate =  900000, .P = 3, .M = 225, .S = 0, .K = 0 },
	{ .rate =  800000, .P = 3, .M = 200, .S = 0, .K = 0 },
	{ .rate =  700000, .P = 3, .M = 175, .S = 0, .K = 0 },
	{ .rate =  600000, .P = 3, .M = 300, .S = 1, .K = 0 },
	{ .rate =  500000, .P = 3, .M = 250, .S = 1, .K = 0 },
	{ .rate =  400000, .P = 3, .M = 200, .S = 1, .K = 0 },
	{ .rate =  300000, .P = 3, .M = 200, .S = 2, .K = 0 },
	{ .rate =  200000, .P = 3, .M = 200, .S = 2, .K = 0 },
	{ .rate =  100000, .P = 3, .M = 200, .S = 3, .K = 0 },
};

static struct clk_priv cmu_pll[] = {
	PLL_INIT(0, (struct nx_pll_reg *)PHY_BASEADDR_PLL0),
	PLL_INIT(1, (struct nx_pll_reg *)PHY_BASEADDR_PLL1),
	PLL_INIT(2, (struct nx_pll_reg *)PHY_BASEADDR_PLL_CPU),
	PLL_INIT(3, (struct nx_pll_reg *)PHY_BASEADDR_PLL_DDR0),
	PLL_INIT(4, (struct nx_pll_reg *)PHY_BASEADDR_PLL_DDR1)
};

static int get_pll_baseaddr(int index)
{
	const unsigned int baseaddr[5] = {
		PHY_BASEADDR_PLL0,
		PHY_BASEADDR_PLL1,
		PHY_BASEADDR_PLL_CPU,
		PHY_BASEADDR_PLL_DDR0,
		PHY_BASEADDR_PLL_DDR1
	};

	return baseaddr[index];
}

static int get_use_ddr_pll(void)
{
	struct clk_cmu_reg *reg =
		(struct clk_cmu_reg *)PHY_BASEADDR_CMU_DDR_MODULE;

	return readl(&reg->grp_clk_src) + 3;
}

static unsigned long pll_round_rate(int pllno, unsigned long rate, int *p,
		int *m, int *s, int *k)
{
	struct pll_pms *pms = NULL;
	int len = 0, l = 0;

	rate /= 1000;

	switch (pllno) {
	case 0:
	case 2:
	case 3:
		pms = pll_2555_pms;
		len = ARRAY_SIZE(pll_2555_pms);
		break;
	case 1:
	case 4:
		pms = pll_2651_pms;
		len = ARRAY_SIZE(pll_2651_pms);
		break;
	default:
		printf("invalid pll number:%d\n", pllno);
		return 0;
	}

	for (l = 0; l < len; l++) {
		if (rate >= PMS_RATE(pms, l))
			break;
	}

	if (p)
		*p = PMS_P(pms, l);
	if (m)
		*m = PMS_M(pms, l);
	if (s)
		*s = PMS_S(pms, l);
	if (k)
		*k = PMS_K(pms, l);

	debug("real %ld Khz, P=%d ,M=%3d, S=%d K=%d\n", PMS_RATE(pms, l),
	      PMS_P(pms, l), PMS_M(pms, l), PMS_S(pms, l), PMS_K(pms, l));

	return PMS_RATE(pms, l) * 1000;
}

static void set_oscmux(unsigned int pll_num, unsigned int muxsel)
{
	struct nx_pll_reg *base
		= (struct nx_pll_reg *)get_pll_baseaddr(pll_num);
	unsigned int tmp;

	tmp = readl(&base->pll_ctrl);
	writel((tmp&(~(1<<3))) | (muxsel << 3), &base->pll_ctrl);
}

static int get_pll_type(int pllno)
{
	if (pllno == 1 || pllno == 4)
		return PLL_TYPE_2651;

	return PLL_TYPE_2555;
}

static int check_pll_lock(int pll_num)
{
	struct nx_pll_reg *base
		= (struct nx_pll_reg *)get_pll_baseaddr(pll_num);

	unsigned int st_count, st_check;
	unsigned int cur_st, lock, mux, tmp;

	st_count = readl(&base->pll_dbg0);
	tmp = readl(&base->pll_ctrl);
	lock = ((tmp >> PLL_LOCK_SHIFT) & 0x1);
	cur_st = (tmp >> PLL_CURST_SHIFT) & 0x1F;

	mux = (tmp >> PLL_MUXSEL_SHIFT) & 0x1;

	if (mux)
		st_check = 1;
	else
		st_check = (st_count > PLL_LOCK_COUNT);

	if (((cur_st == 1) && (lock == 1)) && st_check)
		return true;
	else
		return false;
}

static int clock_is_stable(int pll_num)
{
	int timeout = 0x200000;

	while (check_pll_lock(pll_num) == false) {
		if (timeout-- <= 0)
			return -ETIMEDOUT;
	}

	return 0;
}

static int set_pll_rate(int pllno, int p, int m, int s, int k)
{
	struct nx_pll_reg *base;
	unsigned int tmp;

	base = (struct nx_pll_reg *)get_pll_baseaddr(pllno);

	if (clock_is_stable(pllno))
		return -EINVAL;

	set_oscmux(pllno, PLL_MUX_OSCCLK);

	writel(p << PLL_P_BITPOS | m << PLL_M_BITPOS, &base->pll_cfg1);
	writel(s << PLL_S_BITPOS | k << PLL_K_BITPOS, &base->pll_cfg2);

	writel(PLL_RESETB_SET, &base->pll_cfg0);

	tmp = readl(&base->pll_ctrl);
	writel(tmp | PLL_DIRTYFLAG , &base->pll_ctrl);
	writel(tmp | PLL_UPDATE_DIRECT, &base->pll_ctrl);

	udelay(10);

	writel(PLL_RESETB_CLEAR, &base->pll_cfg0);

	tmp = readl(&base->pll_ctrl);
	writel(tmp | PLL_DIRTYFLAG , &base->pll_ctrl);
	writel(tmp | PLL_UPDATE_DIRECT, &base->pll_ctrl);

	udelay(200);
	set_oscmux(pllno, PLL_MUX_PLL_FOUT);

	return 0;
}

static inline unsigned int pll_rate(unsigned int pllno, unsigned int xtal)
{
	struct nx_pll_reg *reg =
		(struct nx_pll_reg *)get_pll_baseaddr(pllno);
	unsigned int val, val1, nP, nM, nS, nK;
	unsigned int temp = 0;

	val = reg->pll_cfg1;
	val1 = reg->pll_cfg2;
	xtal /= 1000; /* Unit Khz */

	nP = val & 0x03F;
	nM = (val >> 16) & 0x3FF;
	nS = (val1 >> 0) & 0x0FF;
	nK = (val1 >> 16) & 0xFFFF;

	if (get_pll_type(pllno) && nK)
		temp = ((((nK * 1000) / 65536) * xtal) / nP) >> nS;

	return (unsigned int)((((nM * xtal) / nP) >> nS) * 1000) + temp;
}

static ulong nx_pll_get_rate(struct clk *clk)
{
	ulong rate;

	rate = pll_rate(clk->id, ref_clk);

	if (clk->id == 1 || clk->id >= 3)
		return rate/2;

	return rate;
}

static ulong nx_pll_set_rate(int pllno, ulong freq)
{
	int p = 0, m = 0, s = 0, k = 0;
	ulong rate = 0;

	/* sanity check of pll number */
	if (pllno < PLL0 || pllno > DDR4) {
		printf("invalid pll number: %d\n", pllno);
		return -1;
	}

	if (pllno == CPU_PLL) {
		printf(" This channel(%d) is in used for CPU\n", pllno);
		return -1;
	}

	if (pllno == get_use_ddr_pll()) {
		printf(" This channel(%d) is in used for DDR Memory\n", pllno);
		return -1;
	}

	rate = pll_round_rate(pllno, freq, &p, &m, &s, &k);

	set_pll_rate(pllno, p, m, s, k);

	cmu_pll[pllno].p = p;
	cmu_pll[pllno].m = m;
	cmu_pll[pllno].s = s;
	cmu_pll[pllno].k = k;

	return rate;
}

int nx_pll_parse_dt(struct udevice *dev)
{
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(dev);
	int length, i;
	const fdt32_t *list;
	unsigned int init[5][2] = { 0, };

	list = fdt_getprop(blob, node, "frequency", &length);
	if (!list)
		return -1;

	length /= sizeof(*list);
	for (i = 0; i < length/2; i++) {
		init[i][0] = fdt32_to_cpu(*list++);
		init[i][1] = fdt32_to_cpu(*list++);
		nx_pll_set_rate(init[i][0], init[i][1]);
	}

	return 0;
}

static int nx_pll_probe(struct udevice *dev)
{
	return nx_pll_parse_dt(dev);
}

static struct clk_ops nx_pll_ops = {
	.get_rate = nx_pll_get_rate,
};

static const struct udevice_id nx_pll_compat[] = {
	{ .compatible = "nexell,nxp3220-clock-pll" },
	{ }
};

U_BOOT_DRIVER(nx_pll) = {
	.name = "nexell-clock-pll",
	.id = UCLASS_CLK,
	.of_match = nx_pll_compat,
	.ops = &nx_pll_ops,
	.probe = nx_pll_probe,
	.priv_auto_alloc_size = sizeof(struct clk_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
