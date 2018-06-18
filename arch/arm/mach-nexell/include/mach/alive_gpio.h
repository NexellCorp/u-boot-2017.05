/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __NEXELL_ALIVE_GPIO__
#define __NEXELL_ALIVE_GPIO__

#include "./sec_io.h"

#if CONFIG_IS_ENABLED(ALIVE_WITH_SECURE)
#define ALIVE_WRITE(_reg, _val)		sec_writel((_reg), (_val))
#define ALIVE_READ(_reg)		sec_readl((_reg))
#else
#define ALIVE_WRITE(_reg, _val)		writel((_val), (_reg))
#define ALIVE_READ(_reg)		readl((_reg))
#endif

#endif /* __NEXELL_ALIVE_GPIO__ */
