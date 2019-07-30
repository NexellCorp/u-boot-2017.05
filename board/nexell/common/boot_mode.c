// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018 Nexell
 * Junghyun, Kim <jhkim@nexell.co.kr>
 *
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <mach/cpu.h>
#include <linux/err.h>

int run_fastboot_update(void)
{
#ifdef CONFIG_CMD_FASTBOOT
	if (boot_current_device() == BOOT_DEV_USB)
			return run_command("fastboot 0 nexell", 0);
#endif
	return 0;
}
