/*
 * Pinctrl driver for Nexell SoCs
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __PINCTRL_NXP3220_H_
#define __PINCTRL_NXP3220_H_

#include <linux/types.h>
#include <asm/io.h>

#define GPIOX_ALTFN0	0x20
#define GPIOX_ALTFN1	0x24
#define GPIOX_DRV1	0x48
#define GPIOX_DRV0	0x50
#define GPIOX_PULLSEL	0x58
#define GPIOX_PULLENB	0x60
#define GPIOX_ALTFNEX	0x7c

#define GPIOX_SLEW_DISABLE_DEFAULT	0x44
#define GPIOX_DRV1_DISABLE_DEFAULT	0x4C
#define GPIOX_DRV0_DISABLE_DEFAULT	0x54
#define GPIOX_PULLSEL_DISABLE_DEFAULT	0x5C
#define GPIOX_PULLENB_DISABLE_DEFAULT	0x64
#define GPIOX_INPUTENB_DISABLE_DEFAULT	0x78

#define ALIVE_PWRGATE			0x0
#define ALIVE_PULLENB_RST		0x80
#define ALIVE_PULLENB_SET		0x84
#define ALIVE_PULLENB_READ		0x88

#define ALIVE_PULLSEL_RST		0x138
#define ALIVE_PULLSEL_SET		0x13C
#define ALIVE_PULLSEL_READ		0x140

#define ALIVE_DRV0_RST			0x144
#define ALIVE_DRV0_SET			0x148
#define ALIVE_DRV0_READ			0x14C
#define ALIVE_DRV1_RST			0x150
#define ALIVE_DRV1_SET			0x154
#define ALIVE_DRV1_READ			0x158

#define ALIVE_ALTFN_SEL_LOW		(0x10 - 0x800)

enum {
	nx_gpio_padfunc_0 = 0ul,
	nx_gpio_padfunc_1 = 1ul,
	nx_gpio_padfunc_2 = 2ul,
	nx_gpio_padfunc_3 = 3ul
};

enum {
	nx_gpio_drvstrength_0 = 0ul,
	nx_gpio_drvstrength_1 = 1ul,
	nx_gpio_drvstrength_2 = 2ul,
	nx_gpio_drvstrength_3 = 3ul
};

enum {
	nx_gpio_pull_down = 0ul,
	nx_gpio_pull_up = 1ul,
	nx_gpio_pull_off = 2ul
};

#endif /* __PINCTRL_NXP3220_H_ */
