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

/* USB Device Firmware Update support */
#define DFU_ALT_INFO_RAM \
	"dfu_alt_info_ram=" \
	"setenv dfu_alt_info " \
	"kernel ram 0x48000000 0xD80000\\\\;" \
	"fdt ram 0x49000000 0x80000\\\\;" \
	"ramdisk ram 0x49100000 0x4000000\0" \
	"dfu_ram=run dfu_alt_info_ram && dfu 0 ram 0\0" \
	"thor_ram=run dfu_ram_info && thordown 0 ram 0\0"

/* environments */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"bootdelay=0\0"	\
	DFU_ALT_INFO_RAM

#endif
