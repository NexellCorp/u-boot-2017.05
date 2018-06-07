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

/* CMU TOPC block device structure */
struct clk_cmu_reg {
	unsigned int grp_clk_src;
	unsigned int grp_all_div_rst;
	unsigned int rsv0[2];
	unsigned int grp_clkenb[4];
	unsigned int grp_clkenb_clr[4];
	unsigned int grp_rst_set[4];
	unsigned int grp_rst_clr[4];
	unsigned int grp_rst_mode[4];
	unsigned int divider[7];
	unsigned int rsv1;
	unsigned int cur_divider[7];
	unsigned int rsv2;
};

struct clk_cmu_dev {
	struct clk_cmu_reg *reg;
	unsigned int id;
	unsigned char cmu_id;
	unsigned int p_set;
	unsigned int type;
	unsigned int clkenbit;
	unsigned int p_id;
	unsigned long c_freq;
	unsigned int num_clk;
	unsigned int grp_idx;
	bool nc;
};

#endif
