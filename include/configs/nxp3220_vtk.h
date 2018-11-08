/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_VTK_H__
#define __NXP3220_VTK_H__

#include "nxp3220_common.h"

/* kernel load address */
#define CONFIG_SYS_LOAD_ADDR	0x48000000
#define FDT_ADDR		0x49000000
#define INITRD_ADDR		0x49100000

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER
#define CONFIG_SUPPORT_EMMC_BOOT

#define MMC_BOOT_DEV		0
#define MMC_ROOT_DEV		0
#define MMC_BOOT_PART		1
#define MMC_ROOTFS_PART		2

#define MMC_BOOT_PART_TYPE	"ext4"
#define MMC_ROOTFS_PART_TYPE	"ext4"

/* environments */
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_MMC_ENV_DEV	0
#define CONFIG_ENV_OFFSET	(0x2E0200)
#define CONFIG_ENV_SIZE		(0x4000) /* env size */
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_rootfs\0" \
	"bootdelay=" __stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_rootfs=" \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_args;" \
		"bootz ${kernel_addr} - ${fdt_addr}\0" \
	"console=" CONFIG_DEFAULT_CONSOLE \
	"fdt_addr=" __stringify(FDT_ADDR) "\0" \
	"fdt_file=nxp3220-vtk.dtb\0" \
	"load_args=setenv bootargs \"" \
		"root=/dev/mmcblk${mmc_boot_dev}p${mmc_rootfs_part} " \
		"rootfstype=${mmc_rootfs_part_type} ${root_rw} " \
		"${console} ${log_msg} ${opts}" \
		"\"\0" \
	"load_kernel=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} " \
		"${kernel_file}\0" \
	"load_fdt=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} " \
		"${fdt_file}\0" \
	"log_msg=loglevel=7 printk.time=1 earlyprintk\0" \
	"kernel_addr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file=zImage\0" \
	"mmc_boot_dev=" __stringify(MMC_BOOT_DEV) "\0" \
	"mmc_boot_part=" __stringify(MMC_BOOT_PART) "\0" \
	"mmc_boot_part_type=" MMC_BOOT_PART_TYPE "\0" \
	"mmc_rootfs_part=" __stringify(MMC_ROOTFS_PART) "\0" \
	"mmc_rootfs_part_type=" MMC_ROOTFS_PART_TYPE "\0" \
	"root_rw=rw\0" \

#endif
