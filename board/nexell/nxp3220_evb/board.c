/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include "../common/common.h"

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
#ifndef CONFIG_NAND_NXP3220
	dcache_enable();
#endif
}
#endif

int board_init(void)
{
	boot_check_mode();
	return 0;
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_CMD_BMP
	draw_logo();
#endif
	run_fastboot_update();
	return 0;
}
#endif