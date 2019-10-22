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

/* high 24 bits(0x6e7870) is tag, low 8 bits is type of reboot */
#define REBOOT_MAGIC		0x6e787000
#define BOOT_NORMAL		(REBOOT_MAGIC + 0)	/* normal boot */
#define BOOT_FASTBOOT		(REBOOT_MAGIC + 1)	/* fastboot mode */
#define BOOT_RECOVERY		(REBOOT_MAGIC + 2)	/* recovery mode */
#define BOOT_DFU		(REBOOT_MAGIC + 3)	/* dfu downloader mode */

unsigned int boot_check_reboot_mode(void);

#endif
