/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/sizes.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <fdtdec.h>
#include <asm/io.h>
#include <asm/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

#define NEXELL_GPIO_OUT			(0x0)
#define NEXELL_GPIO_OUTENB		(0x4)
#define NEXELL_GPIO_DETMODE0		(0x8)
#define NEXELL_GPIO_DETMODE1		(0xc)
#define NEXELL_GPIO_INTENB		(0x10)
#define NEXELL_GPIO_DET			(0x14)
#define NEXELL_GPIO_PAD			(0x18)
#define NEXELL_GPIO_INPUTENB		(0x74)

#define NEXELL_ALIVE_OUTENB_RST		(0x74)
#define NEXELL_ALIVE_OUTENB_SET		(0x78)
#define NEXELL_ALIVE_OUTENB_READ	(0x7C)
#define NEXELL_ALIVE_OUT_RST		(0x8C)
#define NEXELL_ALIVE_OUT_SET		(0x90)
#define NEXELL_ALIVE_OUT_READ		(0x94)
#define NEXELL_ALIVE_INENB_RST		(0x15C)
#define NEXELL_ALIVE_INENB_SET		(0x160)
#define NEXELL_ALIVE_INENB_READ		(0x164)

struct nexell_gpio_platdata {
	void __iomem *regs;
	int gpio_count;
	const char *bank_name;
	bool is_alive;
};

static int nexell_alive_direction_input(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;
	u32 bit = 1 << pin;

	writel(bit, base + NEXELL_ALIVE_INENB_SET);
	writel(bit, base + NEXELL_ALIVE_OUTENB_RST);

	return 0;
}

static int nexell_alive_direction_output(struct udevice *dev, unsigned int pin,
					 int val)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;
	u32 bit = 1 << pin;

	writel(bit, base + NEXELL_ALIVE_OUT_SET);

	writel(bit, base + NEXELL_ALIVE_INENB_RST);
	writel(bit, base + NEXELL_ALIVE_OUTENB_SET);

	return 0;
}

static int nexell_alive_get_value(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	return (readl(base + NEXELL_ALIVE_OUT_READ) >> pin) & 1;
}


static int nexell_alive_set_value(struct udevice *dev, unsigned int pin,
				  int val)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	writel(1 << pin, base + NEXELL_ALIVE_OUT_SET);

	return 0;
}

static int nexell_alive_get_function(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	u32 mask = 1UL << pin;
	unsigned int output, input;

	output = readl(base + NEXELL_ALIVE_OUTENB_READ) & mask;
	input = readl(base + NEXELL_ALIVE_INENB_READ) & mask;

	if (output && !input)
		return GPIOF_OUTPUT;
	else if (!output && input)
		return GPIOF_INPUT;
	else
		return GPIOF_UNKNOWN;

	return 0;
}

static int nexell_gpio_direction_input(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	if (plat->is_alive)
		return nexell_alive_direction_input(dev, pin);

	clrbits_le32(base + NEXELL_GPIO_OUTENB, 1 << pin);
	if (device_is_compatible(dev, "nexell,nxp3220-gpio"))
		setbits_le32(base + NEXELL_GPIO_INPUTENB, 1 << pin);

	return 0;
}

static int nexell_gpio_direction_output(struct udevice *dev, unsigned int pin,
					int val)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	if (plat->is_alive)
		return nexell_alive_direction_output(dev, pin, val);

	if (val)
		setbits_le32(base + NEXELL_GPIO_OUT, 1 << pin);
	else
		clrbits_le32(base + NEXELL_GPIO_OUT, 1 << pin);

	setbits_le32(base + NEXELL_GPIO_OUTENB, 1 << pin);

	return 0;
}

static int nexell_gpio_get_value(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;
	unsigned int mask = 1UL << pin;
	unsigned int value;

	if (plat->is_alive)
		return nexell_alive_get_value(dev, pin);

	value = (readl(base + NEXELL_GPIO_PAD) & mask) >> pin;

	return value;
}


static int nexell_gpio_set_value(struct udevice *dev, unsigned int pin, int val)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;

	if (plat->is_alive)
		return nexell_alive_set_value(dev, pin, val);

	if (val)
		setbits_le32(base + NEXELL_GPIO_OUT, 1 << pin);
	else
		clrbits_le32(base + NEXELL_GPIO_OUT, 1 << pin);

	return 0;
}

static int nexell_gpio_get_function(struct udevice *dev, unsigned int pin)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	void __iomem *base = plat->regs;
	unsigned int mask = (1UL << pin);
	unsigned int output;

	if (plat->is_alive)
		return nexell_alive_get_function(dev, pin);

	output = readl(base + NEXELL_GPIO_OUTENB) & mask;

	if (output)
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static int nexell_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);

	uc_priv->gpio_count = plat->gpio_count;
	uc_priv->bank_name = plat->bank_name;

	return 0;
}

static int nexell_gpio_ofdata_to_platdata(struct udevice *dev)
{
	struct nexell_gpio_platdata *plat = dev_get_platdata(dev);
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->regs = devm_ioremap(dev, addr, SZ_1K);
	if (!plat->regs)
		return -ENOMEM;
	plat->gpio_count = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
			"nexell,gpio-bank-width", 32);
	plat->bank_name = fdt_getprop(gd->fdt_blob, dev_of_offset(dev),
			"gpio-bank-name", NULL);
	if (!plat->bank_name)
		return -EINVAL;

	if (!strncmp(plat->bank_name, "gpio_alv", strlen("gpio_alv")))
		plat->is_alive = 1;
	else
		plat->is_alive = 0;

	return 0;
}

static const struct dm_gpio_ops nexell_gpio_ops = {
	.direction_input	= nexell_gpio_direction_input,
	.direction_output	= nexell_gpio_direction_output,
	.get_value		= nexell_gpio_get_value,
	.set_value		= nexell_gpio_set_value,
	.get_function		= nexell_gpio_get_function,
};

static const struct udevice_id nexell_gpio_ids[] = {
	{ .compatible = "nexell,nxp3220-gpio" },
	{ }
};

U_BOOT_DRIVER(nexell_gpio) = {
	.name		= "nexell_gpio",
	.id		= UCLASS_GPIO,
	.of_match	= nexell_gpio_ids,
	.ops		= &nexell_gpio_ops,
	.ofdata_to_platdata = nexell_gpio_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct nexell_gpio_platdata),
	.probe		= nexell_gpio_probe,
};
