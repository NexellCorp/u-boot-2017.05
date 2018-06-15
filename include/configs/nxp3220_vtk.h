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
#define CONFIG_SYS_SDRAM_SIZE	0x10000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

/* kernel load address */
#define CONFIG_SYS_LOAD_ADDR	0x48000000

/* environments */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"bootdelay=0\0"

/* flash */
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_BAR

#endif
