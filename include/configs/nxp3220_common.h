/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_COMMON_H__
#define __NXP3220_COMMON_H__

#include <linux/sizes.h>

/* System memory Configuration */
#define	CONFIG_SYS_TEXT_BASE	0x43C00000
#define	CONFIG_SYS_INIT_SP_ADDR	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_BASE	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_SDRAM_BASE	0x40000000

/* Memory Test (up to 0x3C00000:60MB) */
#define MEMTEST_SIZE			(32 * SZ_1M)
#define CONFIG_SYS_MEMTEST_START	(CONFIG_SYS_SDRAM_BASE + 0)
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + MEMTEST_SIZE)

/* dram 1 bank num */
#define CONFIG_NR_DRAM_BANKS	1

/* Console I/O Buffer Size  */
#define CONFIG_SYS_CBSIZE	1024
/* max number of command args   */
#define CONFIG_SYS_MAXARGS	16

/* load address */
#define CONFIG_SYS_LOAD_ADDR	0x40008000
#define FDT_ADDR		0x43000000
#define INITRD_ADDR		0x43100000

#define CONFIG_BOOTCOMMAND		"run autoboot"
#define CONFIG_DEFAULT_CONSOLE		"console=ttyS2,115200n8\0"

/* environments */
#if defined(CONFIG_ENV_IS_IN_MMC) || defined(CONFIG_ENV_IS_IN_SPI_FLASH) || \
    defined(CONFIG_ENV_IS_IN_NAND)
#define CONFIG_ENV_OFFSET		(0x330000)
#define CONFIG_ENV_SIZE			(0x4000) /* env size */
#endif

/*
 * Default environment organization
 */
#if !defined(CONFIG_ENV_IS_IN_MMC) && !defined(CONFIG_ENV_IS_IN_NAND) && \
	!defined(CONFIG_ENV_IS_IN_FLASH) && !defined(CONFIG_ENV_IS_IN_EEPROM) && \
	!defined(CONFIG_ENV_IS_IN_SPI_FLASH)
	#define CONFIG_ENV_OFFSET	1024
	#define CONFIG_ENV_SIZE		(16 * 1024) /* env size */
	/* imls - list all images found in flash, default enable so disable */
	#undef CONFIG_CMD_IMLS
#endif

#define CONFIG_ENV_OVERWRITE
#define CONFIG_MISC_INIT_R
#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER

/* support bootsector for emmc */
#define CONFIG_SUPPORT_EMMC_BOOT

/* For NAND */
#ifdef CONFIG_CMD_NAND
#define CONFIG_CMD_NAND_LOCK_UNLOCK
#define CONFIG_SYS_MAX_NAND_DEVICE     1
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_MTD_PARTITIONS
#define CONFIG_MTD_DEVICE
#endif

#endif
