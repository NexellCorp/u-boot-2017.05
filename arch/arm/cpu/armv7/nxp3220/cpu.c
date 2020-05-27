/*
 * (C) Copyright 2018 Nexell
 * JongKeun, Choi <jkchoi@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <asm/system.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

void s_init(void)
{
}

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	dcache_enable();
}
#endif

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	return 0;
}
#endif
