/*
 * Pinctrl driver for Nexell SoCs
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <dm/pinctrl.h>
#include <dm/root.h>
#include <fdtdec.h>
#include "pinctrl-nexell.h"
#include "pinctrl-nxp3220.h"

DECLARE_GLOBAL_DATA_PTR;

static void nx_gpio_set_bit(u32 *value, u32 bit, int enable)
{
	register u32 newvalue;
	newvalue = *value;
	newvalue &= ~(1ul << bit);
	newvalue |= (u32)enable << bit;
	writel(newvalue, value);
}

static void nx_gpio_set_bit2(u32 *value, u32 bit, u32 bit_value)
{
	register u32 newvalue = *value;
	newvalue = (u32)(newvalue & ~(3ul << (bit * 2)));
	newvalue = (u32)(newvalue | (bit_value << (bit * 2)));

	writel(newvalue, value);
}

static int nx_gpio_open_module(void *base)
{
	writel(0xFFFFFFFF, base + GPIOX_SLEW_DISABLE_DEFAULT);
	writel(0xFFFFFFFF, base + GPIOX_DRV1_DISABLE_DEFAULT);
	writel(0xFFFFFFFF, base + GPIOX_DRV0_DISABLE_DEFAULT);
	writel(0xFFFFFFFF, base + GPIOX_PULLSEL_DISABLE_DEFAULT);
	writel(0xFFFFFFFF, base + GPIOX_PULLENB_DISABLE_DEFAULT);
	writel(0xFFFFFFFF, base + GPIOX_INPUTENB_DISABLE_DEFAULT);
	return true;
}

static void nx_gpio_set_pad_function(void *base, u32 pin, u32 fn)
{
	u32 reg;
	u32 ex_bit = (fn >> 2) & 0x1;

	nx_gpio_set_bit(base + GPIOX_ALTFNEX, pin, ex_bit);

	reg = GPIOX_ALTFN0 + ((pin / 16) * 4);
	nx_gpio_set_bit2(base + reg, pin % 16, fn);
}

static void nx_gpio_set_drive_strength(void *base, u32 pin, u32 drv)
{
	nx_gpio_set_bit(base + GPIOX_DRV1, pin, (int)(((u32)drv >> 0) & 0x1));
	nx_gpio_set_bit(base + GPIOX_DRV0, pin, (int)(((u32)drv >> 1) & 0x1));
}

static void nx_gpio_set_pull_mode(void *base, u32 pin, u32 mode)
{
	if (mode == nx_gpio_pull_off) {
		nx_gpio_set_bit(base + GPIOX_PULLENB, pin, false);
		nx_gpio_set_bit(base + GPIOX_PULLSEL, pin, false);
	} else {
		nx_gpio_set_bit(base + GPIOX_PULLSEL,
				pin, (mode & 1 ? true : false));
		nx_gpio_set_bit(base + GPIOX_PULLENB, pin, true);
	}
}

static void nx_alive_set_pad_function(u32 pin, u32 fn)
{
	/* TODO request to secure os */
	error("%s called\n", __func__);
}

static void nx_alive_set_pullup(u32 pin, bool enable)
{
	/* TODO request to secure os */
	error("%s called\n", __func__);
}

static void nx_alive_set_drive_strength(u32 pin, u32 drv)
{
	/* TODO request to secure os */
	error("%s called\n", __func__);
}

static int nxp3220_pinctrl_gpio_init(struct udevice *dev)
{
	struct nexell_pinctrl_priv *priv = dev_get_priv(dev);
	const struct nexell_pin_ctrl *ctrl = priv->pin_ctrl;
	int i;

	for (i = 0; i < ctrl->nr_banks - 1; i++) /* except alive bank */
		nx_gpio_open_module((void *)(ctrl->pin_banks[i].addr));

	return 0;
}

static int nxp3220_pinctrl_alive_init(struct udevice *dev)
{
	struct nexell_pinctrl_priv *priv = dev_get_priv(dev);
	const struct nexell_pin_ctrl *ctrl = priv->pin_ctrl;
	void __iomem *reg = ctrl->pin_banks[ctrl->nr_banks - 1].addr;

	writel(1, reg + ALIVE_PWRGATE);
	return 0;
}

static int nxp3220_pinctrl_init(struct udevice *dev)
{
	nxp3220_pinctrl_gpio_init(dev);
	nxp3220_pinctrl_alive_init(dev);

	return 0;
}

static int is_pin_alive(const char *name)
{
	return !strncmp(name, "alive", 5);
}

/**
 * nxp3220_pinctrl_set_state: configure a pin state.
 * dev: the pinctrl device to be configured.
 * config: the state to be configured.
 */
static int nxp3220_pinctrl_set_state(struct udevice *dev,
				     struct udevice *config)
{
	const void *fdt = gd->fdt_blob;
	int node = dev_of_offset(config);
	unsigned int count, idx, pin = -1;
	unsigned int pinfunc, pinpud, pindrv;
	void __iomem *reg = NULL;
	const char *name;

	/*
	 * refer to the following document for the pinctrl bindings
	 * linux/Documentation/devicetree/bindings/pinctrl/nexell-pinctrl.txt
	 */
	count = fdt_stringlist_count(fdt, node, "nexell,pins");
	if (count <= 0)
		return -EINVAL;

	pinfunc = fdtdec_get_int(fdt, node, "nexell,pin-function", -1);
	pinpud = fdtdec_get_int(fdt, node, "nexell,pin-pull", -1);
	pindrv = fdtdec_get_int(fdt, node, "nexell,pin-strength", -1);

	for (idx = 0; idx < count; idx++) {
		name = fdt_stringlist_get(fdt, node, "nexell,pins", idx, NULL);
		if (!name)
			continue;

		if (is_pin_alive(name)) {
			if (pinfunc != -1)
				nx_alive_set_pad_function(pin, pinfunc);

			/* pin pull up/down */
			if (pinpud != -1)
				nx_alive_set_pullup(pin, pinpud & 1);

			if (pindrv != -1)
				nx_alive_set_drive_strength(pin, pindrv);
			continue;
		}

		reg = pin_to_bank_base(dev, name, &pin);
		if (!reg || pin == -1) {
			printf("invalid pin configurations of pin %s\n", name);
			continue;
		}

		/* pin function */
		if (pinfunc != -1)
			nx_gpio_set_pad_function((void *)reg, pin, pinfunc);

		/* pin pull up/down/off */
		if (pinpud != -1)
			nx_gpio_set_pull_mode((void *)reg, pin, pinpud);

		/* pin drive strength */
		if (pindrv != -1)
			nx_gpio_set_drive_strength((void *)reg, pin, pindrv);
	}

	return 0;
}

int nxp3220_pinctrl_probe(struct udevice *dev)
{
	int ret;

	ret = nexell_pinctrl_probe(dev);
	if (ret < 0)
		return ret;
	nxp3220_pinctrl_init(dev);

	return 0;
}

static struct pinctrl_ops nxp3220_pinctrl_ops = {
	.set_state	= nxp3220_pinctrl_set_state,
};

/* pin banks of nxp3220 pin-controller */
static struct nexell_pin_bank_data nxp3220_pin_banks[] = {
	NEXELL_PIN_BANK(32, "gpioa"),
	NEXELL_PIN_BANK(32, "gpiob"),
	NEXELL_PIN_BANK(32, "gpioc"),
	NEXELL_PIN_BANK(32, "gpiod"),
	NEXELL_PIN_BANK(32, "gpioe"),
	NEXELL_PIN_BANK(6,  "alive"),
};

struct nexell_pin_ctrl nxp3220_pin_ctrl[] = {
	{
		/* pin-controller data */
		.pin_banks	= nxp3220_pin_banks,
		.nr_banks	= ARRAY_SIZE(nxp3220_pin_banks),
	},
};

static const struct udevice_id nxp3220_pinctrl_ids[] = {
	{ .compatible = "nexell,nxp3220-pinctrl",
		.data = (ulong)nxp3220_pin_ctrl },
	{ }
};

U_BOOT_DRIVER(pinctrl_nxp3220) = {
	.name		= "pinctrl_nxp3220",
	.id		= UCLASS_PINCTRL,
	.of_match	= nxp3220_pinctrl_ids,
	.priv_auto_alloc_size = sizeof(struct nexell_pinctrl_priv),
	.ops		= &nxp3220_pinctrl_ops,
	.probe		= nxp3220_pinctrl_probe,
	.flags		= DM_FLAG_PRE_RELOC
};
