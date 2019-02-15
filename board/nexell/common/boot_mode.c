// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018 Nexell
 * Junghyun, Kim <jhkim@nexell.co.kr>
 *
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <linux/err.h>

#define GPIO_OUT        (0x0)
#define GPIO_OUTENB	(0x4)
#define GPIO_PAD        (0x18)

enum gpio_group {
	GPIO_A,	GPIO_B, GPIO_C, GPIO_D, GPIO_E,
};

struct boot_gpio {
	int group, pin, function;
};

struct boot_mode {
	char *mode;
	unsigned int mask;
};

#if defined(CONFIG_ARCH_NXP3220) || defined(CONFIG_ARCH_NXP3225)

#define	GPIO_BASE(x)	(0x20180000 + (x * 0x10000))

static struct boot_gpio boot_pin_main[] = {
	[0] = { GPIO_C, 13, },
	[1] = { GPIO_B, 17, },
	[2] = { GPIO_D, 25, },
};

static struct boot_mode boot_mode[] = {
	{ "EMMC", 0 },
	{ "USB" , BIT(0) },
	{ "SD"  , BIT(0) | BIT(1) },
	{ "NAND", BIT(2) },
	{ "SPI" , BIT(0) | BIT(2) },
	{ "UART", BIT(1) | BIT(2) },
};
#else
#error "Not defined bootmode configs"
#endif

static struct boot_mode *current_bootmode;

static int read_gpio(struct boot_gpio *desc)
{
	void __iomem *base = (void __iomem *)GPIO_BASE(desc->group);
	int pin = desc->pin;
	unsigned int mask = 1UL << pin;
	unsigned int val, out;

	out = readl(base + GPIO_OUTENB) & mask;

	if (out)
		val = (readl(base + GPIO_OUT) & mask) >> pin;
	else
		val = (readl(base + GPIO_PAD) & mask) >> pin;

	return val;
}

int boot_check_mode(void)
{
	unsigned int mode = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(boot_pin_main); i++)
		mode |= read_gpio(&boot_pin_main[i]) << i;

	debug("boot mode value:0x%x\n", mode);

	for (i = 0; i < ARRAY_SIZE(boot_mode); i++) {
		if (boot_mode[i].mask == mode) {
#ifndef CONFIG_QUICKBOOT_QUIET
			printf("BOOT: %s: 0x%x\n", boot_mode[i].mode, mode);
#endif
			current_bootmode = &boot_mode[i];
			break;
		}
	}

	return 0;
}

int run_fastboot_update(void)
{
#ifdef CONFIG_CMD_FASTBOOT
	if (current_bootmode) {
		if (!strcmp(current_bootmode->mode, "USB"))
			return run_command("fastboot 0 nexell", 0);
	}
#endif
	return 0;
}
