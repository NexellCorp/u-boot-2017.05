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
#define CONFIG_SYS_SDRAM_SIZE	0x70000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

/* kernel load address */
#define CONFIG_SYS_LOAD_ADDR	0x48000000

/* Console I/O Buffer Size  */
#define CONFIG_SYS_CBSIZE	1024
/* max number of command args   */
#define CONFIG_SYS_MAXARGS	16

/* Etc Command definition */
#define CONFIG_CMDLINE_EDITING	/* add command line history */

/* Support legacy image format */
#define CONFIG_IMAGE_FORMAT_LEGACY

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER

/*
 * Default environment organization
 */
#if !defined(CONFIG_ENV_IS_IN_MMC) && !defined(CONFIG_ENV_IS_IN_NAND) && \
	!defined(CONFIG_ENV_IS_IN_FLASH) && !defined(CONFIG_ENV_IS_IN_EEPROM)
	/* default: CONFIG_ENV_IS_NOWHERE */
	#define CONFIG_ENV_IS_NOWHERE
	#define CONFIG_ENV_OFFSET	1024
	#define CONFIG_ENV_SIZE		(4 * 1024) /* env size */
	/* imls - list all images found in flash, default enable so disable */
	#undef CONFIG_CMD_IMLS
#endif

#endif
