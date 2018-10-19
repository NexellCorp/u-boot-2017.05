/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_VTK_H__
#define __NXP3220_VTK_H__

#include "nxp3220_common.h"

/* System memory Configuration */
#define	CONFIG_SYS_TEXT_BASE	0x43C00000
#define	CONFIG_SYS_INIT_SP_ADDR	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_BASE	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_SDRAM_BASE	0x40000000
#define CONFIG_SYS_SDRAM_SIZE	0x1F000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

/* Memory Test (up to 0x3C00000:60MB) */
#define MEMTEST_SIZE			(32 * SZ_1M)
#define CONFIG_SYS_MEMTEST_START	(CONFIG_SYS_SDRAM_BASE + 0)
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + MEMTEST_SIZE)

/* kernel load address */
#define CONFIG_SYS_LOAD_ADDR	0x48000000
#define FDT_ADDR		0x49000000
#define INITRD_ADDR		0x49100000

/* environments */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_initrd\0" \
	"bootdelay=" __stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_initrd=" \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_initrd;" \
		"run load_args;" \
		"bootz ${kernel_addr} ${initrd_addr} ${fdt_addr}\0" \
	"console=" CONFIG_DEFAULT_CONSOLE \
	"dfu_bufsiz=0x2000000\0" \
	DFU_ALT_INFO \
	DFU_ALT_INFO_RAM \
	"fdt_addr=" __stringify(FDT_ADDR) "\0" \
	"fdt_file=nxp3220-vtk.dtb\0" \
	"format_emmc=" \
		"gpt write mmc 0 $partitions;" \
		"mmc rescan;" \
		"mmc partconf 0 0 1 1;" \
		"mmc bootbus 0 2 0 0;" \
		"mmc rst-function 0 1;" \
		"mmc rescan\0" \
	"initrd_addr=" __stringify(INITRD_ADDR) "\0" \
	"initrd_file=uInitrd\0" \
	"load_args=setenv bootargs \"" \
		"root=/dev/mmcblk${mmc_boot_dev}p${mmc_rootfs_part} " \
		"rootfstype=${mmc_rootfs_part_type} ${root_rw} " \
		"bootpart=/dev/mmcblk${mmc_boot_dev}p${mmc_boot_part} " \
		"bootpart_type=${mmc_boot_part_type} " \
		"modulespart=/dev/mmcblk${mmc_boot_dev}p${mmc_modules_part} " \
		"modulespart_type=${mmc_modules_part_type} " \
		"firmwarepart=/dev/mmcblk${mmc_boot_dev}p${mmc_firmware_part} " \
		"firmwarespart_type=${mmc_firmware_part_type} " \
		"secpart=/dev/mmcblk${mmc_boot_dev}p${mmc_sec_part} " \
		"secpart_type=${mmc_sec_part_type} " \
		"datapart=/dev/mmcblk${mmc_boot_dev}p${mmc_data_part} " \
		"datapart_type=${mmc_data_part_type} " \
		"${console} ${log_msg} ${opts}" \
		"\"\0" \
	"load_kernel=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} " \
		"${kernel_file}\0" \
	"load_initrd=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${initrd_addr} " \
		"${initrd_file}\0" \
	"load_fdt=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} " \
		"${fdt_file}\0" \
	"log_msg=loglevel=7 printk.time=1 earlyprintk\0" \
	"kernel_addr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file=zImage\0" \
	"mmc_boot_dev=" __stringify(MMC_BOOT_DEV) "\0" \
	"mmc_boot_part=" __stringify(MMC_BOOT_PART) "\0" \
	"mmc_boot_part_type=" MMC_BOOT_PART_TYPE "\0" \
	"mmc_modules_part=" __stringify(MMC_MODULES_PART) "\0" \
	"mmc_modules_part_type=" MMC_MODULES_PART_TYPE "\0" \
	"mmc_firmware_part=" __stringify(MMC_FIRMWARE_PART) "\0" \
	"mmc_firmware_part_type=" MMC_FIRMWARE_PART_TYPE "\0" \
	"mmc_rootfs_part=" __stringify(MMC_ROOTFS_PART) "\0" \
	"mmc_rootfs_part_type=" MMC_ROOTFS_PART_TYPE "\0" \
	"mmc_sec_part=" __stringify(MMC_SEC_PART) "\0" \
	"mmc_sec_part_type=" MMC_SEC_PART_TYPE "\0" \
	"mmc_data_part=" __stringify(MMC_DATA_PART) "\0" \
	"mmc_data_part_type=" MMC_DATA_PART_TYPE "\0" \
	"partitions=" PARTS_DEFAULT \
	"root_rw=rw\0" \

#endif
