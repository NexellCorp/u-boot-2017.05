/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __NEXELL_CPU_H
#define __NEXELL_CPU_H

enum boot_device {
	BOOT_DEV_EMMC,
	BOOT_DEV_USB,
	BOOT_DEV_SD,
	BOOT_DEV_NAND,
	BOOT_DEV_SPI,
	BOOT_DEV_UART,
};

int boot_check_bootup_mode(void);
enum boot_device boot_current_device(void);

#endif
