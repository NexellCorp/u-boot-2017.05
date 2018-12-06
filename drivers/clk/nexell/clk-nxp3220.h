/*
 * Nexell PLL helper functions for clock drivers.
 * Copyright (C) 2018 Nexell .
 * Youngbok Park<ybpark@nxell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __NEXELL_CLOCK
#define __NEXELL_CLOCK

#define CMU_NAME_SRC	0
#define CMU_NAME_SYS	1
#define CMU_NAME_USB	2
#define CMU_NAME_HSIF	3
#define CMU_NAME_MM	4
#define CMU_NAME_CPU	5
#define CMU_NAME_DDR	6

#define CMU_TYPE_MAINDIV	0
#define CMU_TYPE_SUBDIV0	1
#define CMU_TYPE_SUBDIV1	2
#define CMU_TYPE_SUBDIV2	3
#define CMU_TYPE_SUBDIV3	4
#define CMU_TYPE_SUBDIV4	5
#define CMU_TYPE_SUBDIV5	6
#define CMU_TYPE_SUBDIV6	7
#define CMU_TYPE_ONLYGATE	8

#define PLL0	0
#define PLL1	1
#define CPU_PLL	2
#define DDR0	3
#define DDR4	4

#define PHY_BASEADDR_CMU_DDR_MODULE 0x23000000

#define CMU_INIT_SRC(clk_num, pset, t, addr, bitidx, pid, grp, notc)	\
{								\
	.reg		= (void *)addr,				\
	.p_set		= pset,					\
	.clkenbit	= bitidx,				\
	.type		= t,					\
	.p_id		= pid,					\
	.grp_idx	= grp,					\
	.id		= clk_num,				\
	.nc		= notc,					\
	.cmu_id		= CMU_NAME_SRC,				\
}

#define CMU_INIT_SYS(clk_num, pset, t, addr, bitidx, pid, grp, notc)	\
{								\
	.reg		= (void *)addr,				\
	.p_set		= pset,					\
	.clkenbit	= bitidx,				\
	.type		= t,					\
	.p_id		= pid,					\
	.grp_idx	= grp,					\
	.id		= clk_num,				\
	.nc		= notc,					\
	.cmu_id		= CMU_NAME_SYS,				\
}

#define CMU_INIT_MM(clk_num, pset, t, addr, bitidx, pid, grp, notc)	\
{								\
	.reg		= (void *)addr,				\
	.p_set		= pset,					\
	.clkenbit	= bitidx,				\
	.type		= t,					\
	.p_id		= pid,					\
	.grp_idx	= grp,					\
	.id		= clk_num,				\
	.nc		= notc,					\
	.cmu_id		= CMU_NAME_MM				\
}

#define CMU_INIT_USB(clk_num, pset, t, addr, bitidx, pid, grp, notc)	\
{								\
	.reg		= (void *)addr,				\
	.p_set		= pset,					\
	.clkenbit	= bitidx,				\
	.type		= t,					\
	.p_id		= pid,					\
	.grp_idx	= grp,					\
	.id		= clk_num,				\
	.nc		= notc,					\
	.cmu_id		= CMU_NAME_USB				\
}

/* CMU register set structure */
struct clk_cmu_reg {
	u32 clkmux_src;
	u32 rsv0[3];
	u32 clkenb_set[4];
	u32 clkenb_clr[4];
	u32 rst_release[4];
	u32 rst_enter[4];
	u32 rst_mode[4];
	u32 div_val[7];
};

struct clk_cmu_dev {
	struct clk_cmu_reg *reg;
	int id;
	unsigned char cmu_id;
	unsigned int p_set;
	unsigned int type;
	unsigned int clkenbit;
	int p_id;
	unsigned long freq;
	int num_clk;
	int grp_idx;
	bool nc;
};

#endif
