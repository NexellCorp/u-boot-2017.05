/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#ifdef CONFIG_CHECK_REVISION
#include <adc.h>
#endif
#ifdef CONFIG_DM_REGULATOR
#include <power/regulator.h>
#endif
#include "../common/common.h"

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	dcache_enable();
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

#ifdef CONFIG_CHECK_REVISION
static int board_check_revision(unsigned int revision[], int size)
{
	unsigned int ch[2] = { 1, 2 };
	unsigned int value;
	int i, ret;

	for (i = 0; i < 2; i++) {
		ret = adc_channel_single_shot("adc@20600000", ch[i], &value);
		if (ret) {
			debug("Cannot read ADC.%d value\n", ch[i]);
			return -EINVAL;
		}

		if (value < 227) /* 0 ~ 0.1v */
			value = 0;
		else
			value = 1;

		if (i < size)
			revision[i] = value;
	}

	return ret;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_CHECK_REVISION
	unsigned int rev[2] = { 0, };
	int ret;

	ret = board_check_revision(rev, 2);
#ifndef CONFIG_QUICKBOOT_QUIET
	if (!ret)
		printf("REV: %d.%d\n", rev[0], rev[1]);
#endif
#endif

#ifdef CONFIG_CMD_BMP
	draw_logo();
#endif
	run_fastboot_update();
	return 0;
}
#endif

#ifdef CONFIG_DM_REGULATOR
int power_init_board(void)
{
	int ret = -ENODEV;

	ret = regulators_enable_boot_on(false);
	if (ret)
		return ret;

	return 0;
}
#endif
