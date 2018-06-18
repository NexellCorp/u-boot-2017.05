/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __NEXELL_SECURE_IO__
#define __NEXELL_SECURE_IO__

#include <asm/io.h>

unsigned long sec_writel(void __iomem *reg, unsigned long val);
unsigned long sec_readl(void __iomem *reg);

#endif
