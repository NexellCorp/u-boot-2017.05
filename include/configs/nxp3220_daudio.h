/*
 * (C) Copyright 2018 Nexell
 * Youngbok, Park <ybpark@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_DAUDIO_H__
#define __NXP3220_DAUDIO_H__

#include "nxp3220_common.h"

/* System memory Configuration */
#define CONFIG_SYS_SDRAM_SIZE	0x1F000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

#define CONFIG_SYS_BOOTM_LEN    (64 << 20)

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

#if defined(CONFIG_TARGET_NXP3220_DAUDIO)
#define	KERNEL_DTB		"nxp3220-daudio.dtb"
#elif defined(CONFIG_TARGET_NXP3220_DAUDIO2)
#define	KERNEL_DTB		"nxp3220-daudio2-rev00.dtb"
#else
#error "NO TARGET !!!"
#endif

#define LOG_MSG			"quiet loglevel=3 printk.time=1"
#define ENV_OPTS		"nexell_drm.fb_argb"

/* For BMP logo */
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_SPLASH_SOURCE
#define CONFIG_SPLASH_SCREEN_ALIGN	/* BMP center */
#define SPLASH_STORAGE_NAME		"mmc_fs"
#define SPLASH_STORAGE_DEVICE		SPLASH_STORAGE_MMC
#define SPLASH_STORAGE_FLAGS		SPLASH_STORAGE_FS
#define SPLASH_STORAGE_DEVPART		"0:1"
#define SPLASH_STORAGE_FILE		"logo.bmp"
#define SPLASH_STORAGE_LOAD		0x50000000

#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP

#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_rootfs\0" \
	"bootdelay="__stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_rootfs=" \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_args;" \
		"run mem_resv;" \
		"bootl ${kernel_addr} - ${fdt_addr}\0" \
	"console="CONFIG_DEFAULT_CONSOLE \
	"fdt_addr="__stringify(FDT_ADDR) "\0" \
	"fdt_file="__stringify(KERNEL_DTB) "\0" \
	"load_args=setenv bootargs \"" \
		"root=/dev/mmcblk${mmc_boot_dev}p${mmc_rootfs_part} " \
		"rootfstype=${mmc_rootfs_part_type} ${root_rw} " \
		"${console} ${log_msg} ${opts}" \
		"\"\0" \
	"load_kernel=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} " \
		"${kernel_file}\0" \
	"load_fdt=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} " \
		"${fdt_file}\0" \
	"log_msg="LOG_MSG "\0" \
	"opts="ENV_OPTS "\0" \
	"kernel_addr="__stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file=Image\0" \
	"mmc_boot_dev="__stringify(MMC_BOOT_DEV) "\0" \
	"mmc_boot_part="__stringify(MMC_BOOT_PART) "\0" \
	"mmc_boot_part_type="MMC_BOOT_PART_TYPE "\0" \
	"mmc_rootfs_part="__stringify(MMC_ROOTFS_PART) "\0" \
	"mmc_rootfs_part_type="MMC_ROOTFS_PART_TYPE "\0" \
	"splashfile="SPLASH_STORAGE_FILE "\0" \
	"splashimage="__stringify(SPLASH_STORAGE_LOAD) "\0" \
	"fb_addr=\0" \
	"mem_resv="  \
		"fdt addr ${fdt_addr}; " \
		"fdt resize; "   \
		"fdt mk /reserved-memory display_reserved; " \
		"fdt set /reserved-memory/display_reserved reg <${fb_addr} 0x546000>; " \
                "\0" \
	"root_rw=rw\0" \

#endif
