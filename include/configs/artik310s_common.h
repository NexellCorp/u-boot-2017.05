/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __ARTIK310S_COMMON_H__
#define __ARTIK310S_COMMON_H__

#include <linux/sizes.h>

/* dram 1 bank num */
#define CONFIG_NR_DRAM_BANKS	1

/* System memory Configuration */
#define	CONFIG_SYS_TEXT_BASE	0x43C00000
#define	CONFIG_SYS_INIT_SP_ADDR	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_BASE	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_SDRAM_BASE	0x40000000
/* 512MB - 16MB(Reserved for SecureOS) */
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

/* Console I/O Buffer Size  */
#define CONFIG_SYS_CBSIZE	1024
/* max number of command args   */
#define CONFIG_SYS_MAXARGS	16

#define CONFIG_ENV_OVERWRITE

#define CONFIG_BOOTCOMMAND		"run autoboot;fastboot 0"
#define CONFIG_DEFAULT_CONSOLE		"console=ttyS0,115200n8\0"

/* Factory info(unit: sector) */
#define FACTORY_INFO_START	0x1c00
#define FACTORY_INFO_SIZE	0x100

#define CONFIG_HAS_ETH1

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER
#define CONFIG_SUPPORT_EMMC_BOOT

#define MMC_BOOT_DEV		0
#define MMC_ROOT_DEV		0
#define MMC_BOOT_PART		2
#define MMC_MODULES_PART	4
#define MMC_FIRMWARE_PART	6
#define MMC_ROOTFS_PART		8
#define MMC_SEC_PART		10
#define MMC_DATA_PART		11

#define MMC_BOOT_PART_TYPE	"ext4"
#define MMC_MODULES_PART_TYPE	"ext4"
#define MMC_FIRMWARE_PART_TYPE	"ext4"
#define MMC_ROOTFS_PART_TYPE	"ext4"
#define MMC_SEC_PART_TYPE	"ext4"
#define MMC_DATA_PART_TYPE	"ext4"

/* Partition Size(Unit: MiB) */
#define BOOT_PART_ALIGN		8
#define BOOT_PART_SIZE		16
#define MODULES_PART_SIZE	20
#define FIRMWARE_PART_SIZE	8
#define ROOTFS_PART_SIZE	512
#define SEC_PART_SIZE		128

#define CONFIG_REVISION_TAG

/*
 * Default environment organization
 */
#if !defined(CONFIG_ENV_IS_IN_MMC) && !defined(CONFIG_ENV_IS_IN_NAND) && \
	!defined(CONFIG_ENV_IS_IN_FLASH) && !defined(CONFIG_ENV_IS_IN_EEPROM)
	#define CONFIG_ENV_OFFSET	1024
	#define CONFIG_ENV_SIZE		(16 * 1024) /* env size */
	/* imls - list all images found in flash, default enable so disable */
	#undef CONFIG_CMD_IMLS
#endif

#define CONFIG_MISC_INIT_R
#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

/* USB Device Firmware Update support */
#define DFU_ALT_INFO_RAM \
	"dfu_alt_info_ram=" \
	"setenv dfu_alt_info " \
	"kernel ram 0x48000000 0xD80000\\\\;" \
	"fdt ram 0x49000000 0x80000\\\\;" \
	"ramdisk ram 0x49100000 0x4000000\0" \
	"dfu_ram=run dfu_alt_info_ram && dfu 0 ram 0\0" \
	"thor_ram=run dfu_ram_info && thordown 0 ram 0\0"

/* eMMC DFU info(Use eMMC Boot Partition) */
#define DFU_ALT_INFO \
	"dfu_alt_info=" \
	"bl-singleimage.bin.raw raw 0x0 0x1800 mmcpart 1;" \
	"zImage ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"Image.itb ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"uInitrd ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"sip-s31nx-artik310s-trike-rev00.dtb ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"boot.img part 0 " __stringify(MMC_BOOT_PART) ";" \
	"modules.img part 0 " __stringify(MMC_MODULES_PART) ";" \
	"firmware.img part 0 " __stringify(MMC_FIRMWARE_PART) ";" \
	"rootfs.img part 0 " __stringify(MMC_ROOTFS_PART) ";" \
	"sec.img part 0 " __stringify(MMC_SEC_PART) ";" \
	"data.img part 0 " __stringify(MMC_DATA_PART) "\0"

#define PARTS_DEFAULT \
	"uuid_disk=${uuid_gpt_disk};" \
	"name=flag,start=4MiB,size=128KiB,uuid=${uuid_gpt_flag};" \
	"name=boot_a,start=" __stringify(BOOT_PART_ALIGN) "MiB,size=" \
		__stringify(BOOT_PART_SIZE) "MiB,uuid=${uuid_gpt_boot_a};" \
	"name=boot_b,size=" __stringify(BOOT_PART_SIZE) "MiB,uuid=${uuid_gpt_boot_b};" \
	"name=modules_a,size=" __stringify(MODULES_PART_SIZE) "MiB,uuid=${uuid_gpt_modules_a};" \
	"name=modules_b,size=" __stringify(MODULES_PART_SIZE) "MiB,uuid=${uuid_gpt_modules_b};" \
	"name=firmware_a,size=" __stringify(FIRMWARE_PART_SIZE) "MiB,uuid=${uuid_gpt_firmware_a};" \
	"name=firmware_b,size=" __stringify(FIRMWARE_PART_SIZE) "MiB,uuid=${uuid_gpt_firmware_b};" \
	"name=rootfs_a,size=" __stringify(ROOTFS_PART_SIZE) "MiB,uuid=${uuid_gpt_rootfs_a};" \
	"name=rootfs_b,size=" __stringify(ROOTFS_PART_SIZE) "MiB,uuid=${uuid_gpt_rootfs_b};" \
	"name=sec,size=" __stringify(SEC_PART_SIZE) "MiB,uuid=${uuid_gpt_sec};" \
	"name=data,size=-,uuid=${uuid_gpt_data}\0"

#endif
