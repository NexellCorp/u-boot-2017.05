/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>

#ifdef CONFIG_DM_PMIC_SM5011
#include <power/pmic.h>
#include <power/regulator.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	return 0;
}
#endif
