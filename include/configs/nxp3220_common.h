/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NXP3220_COMMON_H__
#define __NXP3220_COMMON_H__

#include <linux/sizes.h>

/* dram 1 bank num */
#define CONFIG_NR_DRAM_BANKS	1

/* Console I/O Buffer Size  */
#define CONFIG_SYS_CBSIZE	1024
/* max number of command args   */
#define CONFIG_SYS_MAXARGS	16

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER

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

#define CONFIG_ENV_OVERWRITE

#endif
