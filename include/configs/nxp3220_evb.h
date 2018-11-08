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

/* environments */
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_ENV_OFFSET		(0x2E0200)
#define CONFIG_ENV_SIZE			(0x4000) /* env size */
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	"bootdelay=" __stringify(CONFIG_BOOTDELAY) "\0" \

#endif
