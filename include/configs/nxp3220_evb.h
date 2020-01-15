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

/* For BMP logo */
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_SPLASH_SOURCE
#define CONFIG_SPLASH_SCREEN_ALIGN	/* BMP center */
#define SPLASH_STORAGE_FLAGS		SPLASH_STORAGE_FS
#define SPLASH_STORAGE_FILE		"logo.bmp"
#define SPLASH_STORAGE_LOAD		0x50000000
#define ENV_SPLASH_MEM_RESERVE		"fdt addr ${fdt_addr}; " \
					"fdt resize; "   \
					"fdt mk /reserved-memory display_reserved; " \
					"fdt set /reserved-memory/display_reserved reg <${fb_addr} 0x546000>; "

/* For SD/MMC splash image */
#if defined(CONFIG_ENV_IS_IN_MMC)
	#define SPLASH_STORAGE_DEVICE		SPLASH_STORAGE_MMC
	#define SPLASH_STORAGE_DEVPART		"0:1"
	#define SPLASH_STORAGE_NAME		"mmc_fs"
/* For NAND splash image */
#elif defined(CONFIG_ENV_IS_IN_NAND)
	#define SPLASH_STORAGE_DEVICE		SPLASH_STORAGE_NAND
	#define SPLASH_STORAGE_DEVPART		"boot"		/* ubi mtdpart */
	#define SPLASH_STORAGE_NAME		"ubi0:boot"	/* ubivol */
	#define SPLASH_STORAGE_NAND_UBI
#endif

#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP

/* For SPI FLASH */
#define	CONFIG_SF_DEFAULT_CS		0
#define	CONFIG_SF_DEFAULT_SPEED		40000000
#define	CONFIG_SF_DEFAULT_MODE		0
#define	CONFIG_ENV_SECT_SIZE		0x4000

#if defined(CONFIG_ENV_IS_IN_MMC)
/*
 * For SD/MMC Environment
 */
#if defined(CONFIG_TARGET_NXP3220_EVB)
#define ENV_KERNEL_DTB		"nxp3220-evb.dtb"
#elif defined(CONFIG_TARGET_NXP3220_EVB2)
#define ENV_KERNEL_DTB		"nxp3220-evb2-rev00.dtb"
#else
#error "NO TARGET !!!"
#endif

#define MMC_BOOT_RESET_FUNCTION		/* set 'mmc rst-function' in fastboot */
#define CONFIG_SYS_MMC_ENV_DEV		0
#define ENV_MMC_BOOT_DEV		0
#define ENV_MMC_ROOT_DEV		0
#define ENV_MMC_BOOT_PART		1
#define ENV_MMC_ROOTFS_PART		3
#define ENV_MMC_BOOT_PART_TYPE		"ext4"
#define ENV_MMC_ROOTFS_PART_TYPE	"ext4"
#define ENV_BOOT_PRECOMMAND		""
#define ENV_BOOT_POSTCOMMAND	"bootz ${kernel_addr} - ${fdt_addr}"
#define ENV_BOOT_ARGS		"root=/dev/mmcblk${mmc_root_dev}p${mmc_rootfs_part} " \
				"rootfstype=${mmc_rootfs_part_type} ${root_rw} "
#define ENV_LOAD_KERNEL_FILE	"load_kernel=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} ${kernel_file}"
#define ENV_LOAD_KERNEL_FDT	"load_fdt=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} ${fdt_file}"
#define ENV_EXTRA_SETTINGS 	"mmc_boot_dev="__stringify(ENV_MMC_BOOT_DEV) "\0" \
				"mmc_boot_part="__stringify(ENV_MMC_BOOT_PART) "\0" \
				"mmc_boot_part_type="ENV_MMC_BOOT_PART_TYPE "\0" \
				"mmc_root_dev="__stringify(ENV_MMC_ROOT_DEV) "\0" \
				"mmc_rootfs_part="__stringify(ENV_MMC_ROOTFS_PART) "\0" \
				"mmc_rootfs_part_type="ENV_MMC_ROOTFS_PART_TYPE "\0"
#define ENV_LOAD_INITRD_FILE	"load_initrd=ext4load mmc ${mmc_boot_dev}:${mmc_boot_part} ${initrd_addr} ${initrd_file}"

#elif defined(CONFIG_ENV_IS_IN_NAND)
/*
 * For NAND Environment
 */
#if defined(CONFIG_TARGET_NXP3220_EVB)
#define ENV_KERNEL_DTB		"nxp3220-evb-nand.dtb"
#elif defined(CONFIG_TARGET_NXP3220_EVB2)
#define ENV_KERNEL_DTB		"nxp3220-evb2-nand-rev00.dtb"
#else
#error "NO TARGET !!!"
#endif

#define ENV_NAND_BOOTSECTOR_PART_SIZE	"5m"	/* reserve partition size */
#define ENV_NAND_ENV_PART_SIZE		"1m"	/* environment partition size */
#define ENV_NAND_BOOT_PART_SIZE		"32m"	/* boot partition size */
#define ENV_NAND_MISC_PART_SIZE		"8m"	/* misc partition size */
#define ENV_NAND_ENV_PART		1	/* environment partition */
#define ENV_NAND_BOOT_PART		2	/* boot partition */
#define ENV_NAND_MISC_PART		3	/* misc partition */
#define ENV_NAND_ROOT_PART		4	/* root partition */

#define	ENV_MTDPARTS		"mtdparts=mtd-nand:"ENV_NAND_BOOTSECTOR_PART_SIZE"(bootsector),"\
					""ENV_NAND_ENV_PART_SIZE"(env),"\
					""ENV_NAND_BOOT_PART_SIZE"(boot),"\
					""ENV_NAND_MISC_PART_SIZE"(misc),"\
					"-(rootfs)"
#define ENV_BOOT_PRECOMMAND	"ubifsmount ubi0:boot; " /* ubi part boot;ubifsmount ubi0:boot; */
#define ENV_BOOT_POSTCOMMAND	"bootl ${kernel_addr} - ${fdt_addr}"
#define ENV_BOOT_ARGS		"ubi.mtd="__stringify(ENV_NAND_ROOT_PART)" " \
				"ubi.mtd="__stringify(ENV_NAND_BOOT_PART)" " \
				"ubi.mtd="__stringify(ENV_NAND_MISC_PART)" " \
				"rootfstype=ubifs " \
				"root=${ubiroot} ${root_rw} "
#define ENV_LOAD_KERNEL_FILE	"load_kernel=ubifsload ${kernel_addr} ${kernel_file}"
#define ENV_LOAD_KERNEL_FDT	"load_fdt=ubifsload ${fdt_addr} ${fdt_file}"
#define ENV_EXTRA_SETTINGS 	"mtdids=" CONFIG_MTDIDS_DEFAULT "\0" \
				"mtdparts="ENV_MTDPARTS "\0" \
				"ubiroot=ubi0:rootfs\0"
#define ENV_LOAD_INITRD_FILE	"load_initrd=ubifsload ${initrd_addr} ${initrd_file}"

#else /* CONFIG_ENV_IS_IN_NAND */
/*
 * Environment is not stored
 */
#define ENV_BOOT_PRECOMMAND	""
#define ENV_BOOT_POSTCOMMAND	""
#define ENV_BOOT_ARGS		""
#define ENV_LOAD_KERNEL_FILE	""
#define ENV_LOAD_KERNEL_FDT	""
#define ENV_EXTRA_SETTINGS	""
#define ENV_LOAD_INITRD_FILE	""
#endif /* CONFIG_ENV_IS_NOWHERE */

#define ENV_KERNEL_FILE		"zImage"
#define ENV_LOG_MSG		"loglevel=7 printk.time=1"
#define ENV_OPTS		"nexell_drm.fb_argb"

#define	ENV_RECOVERY_BOOT_ARGS		"rdinit=/init " \
					"ubi.mtd="__stringify(ENV_NAND_ROOT_PART)" " \
					"ubi.mtd="__stringify(ENV_NAND_BOOT_PART)" " \
					"ubi.mtd="__stringify(ENV_NAND_MISC_PART)" "
#define	ENV_RECOVERY_POSTCOMMAND	"bootz ${kernel_addr} ${initrd_addr} ${fdt_addr}"
#define ENV_INIRTD_IMAGE		"recovery.uinitrd"

/* For Environments */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_rootfs\0" \
	"bootdelay="__stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_rootfs=" \
		ENV_BOOT_PRECOMMAND \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_args;" \
		"run mem_resv;" \
		ENV_BOOT_POSTCOMMAND "\0" \
	"console="CONFIG_DEFAULT_CONSOLE \
	"fdt_addr="__stringify(ENV_FDT_ADDR) "\0" \
	"fdt_file="__stringify(ENV_KERNEL_DTB) "\0" \
	"load_args=setenv bootargs \"" \
		ENV_BOOT_ARGS \
		"${console} ${log_msg} ${opts}" \
		"\"\0" \
	ENV_LOAD_KERNEL_FILE "\0"\
	ENV_LOAD_KERNEL_FDT "\0"\
	"log_msg="ENV_LOG_MSG "\0" \
	"opts="ENV_OPTS "\0" \
	"kernel_addr="__stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file="ENV_KERNEL_FILE" \0" \
	"splashfile="SPLASH_STORAGE_FILE "\0" \
	"splashimage="__stringify(SPLASH_STORAGE_LOAD) "\0" \
	"fb_addr=\0" \
	"root_rw=rw\0" \
	"mem_resv="ENV_SPLASH_MEM_RESERVE "\0" \
	ENV_EXTRA_SETTINGS \
	"recovery-boot=" \
		ENV_BOOT_PRECOMMAND \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_initrd;" \
		"run load_recovery_args;" \
		"run mem_resv;" \
		ENV_RECOVERY_POSTCOMMAND "\0" \
	"initrd_addr="__stringify(ENV_INITRD_ADDR) "\0" \
	"initrd_file="ENV_INIRTD_IMAGE" \0" \
	"load_recovery_args=setenv bootargs \"" \
		ENV_RECOVERY_BOOT_ARGS \
		"${console} ${log_msg} quiet" "\0" \
	ENV_LOAD_INITRD_FILE "\0"\

#endif
