/*
 * (C) Copyright 2018 Nexell
 * Youngbok, Park <ybpark@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_EVB_H__
#define __NXP3220_EVB_H__

#include "nxp3220_common.h"

/* System memory Configuration */
#define CONFIG_SYS_SDRAM_SIZE	0x1F000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

/* Memory Test (up to 0x3C00000:60MB) */
#define MEMTEST_SIZE			(32 * SZ_1M)
#define CONFIG_SYS_MEMTEST_START	(CONFIG_SYS_SDRAM_BASE + 0)
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + MEMTEST_SIZE)

#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_MMC_ENV_DEV		0
#endif

/* For SD/MMC */
#define MMC_BOOT_DEV		0
#define MMC_ROOT_DEV		0
#define MMC_BOOT_PART		1
#define MMC_ROOTFS_PART		3

/* set 'mmc rst-function' in fastboot */
#define MMC_BOOT_RESET_FUNCTION

#define MMC_BOOT_PART_TYPE	"ext4"
#define MMC_ROOTFS_PART_TYPE	"ext4"

#define	KERNEL_DTB		"nxp3220-evb.dtb"
#define LOG_STATUS		"quiet systemd.log_level=info systemd.show_status=false"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_rootfs\0" \
	"bootdelay="__stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_rootfs=" \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_args;" \
		"bootz ${kernel_addr} - ${fdt_addr}\0" \
	"console="CONFIG_DEFAULT_CONSOLE \
	"fdt_addr="__stringify(FDT_ADDR) "\0" \
	"fdt_file="__stringify(KERNEL_DTB) "\0" \
	"load_args=setenv bootargs \"" \
		"root=/dev/mmcblk${mmc_boot_dev}p${mmc_rootfs_part} " \
		"rootfstype=${mmc_rootfs_part_type} ${root_rw} " \
		"${console} ${log_msg} ${opt_log} ${opts}" \
		"\"\0" \
	"load_kernel=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} " \
		"${kernel_file}\0" \
	"load_fdt=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} " \
		"${fdt_file}\0" \
	"log_msg=loglevel=7 printk.time=1\0" \
	"kernel_addr="__stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file=zImage\0" \
	"mmc_boot_dev="__stringify(MMC_BOOT_DEV) "\0" \
	"mmc_boot_part="__stringify(MMC_BOOT_PART) "\0" \
	"mmc_boot_part_type="MMC_BOOT_PART_TYPE "\0" \
	"mmc_rootfs_part="__stringify(MMC_ROOTFS_PART) "\0" \
	"mmc_rootfs_part_type="MMC_ROOTFS_PART_TYPE "\0" \
	"root_rw=rw\0" \

#endif
