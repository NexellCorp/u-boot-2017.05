// SPDX-License-Identifier: GPL-2.0+
/*
 * Nexell USB2 PHY driver
 * Copyright (c) 2018 JungHyun Kim <jhkim@nexell.co.kr>
 */

#include <common.h>
#include <dm.h>
#include <dm/device.h>
#include <fdtdec.h>
#include <clk.h>
#include <errno.h>
#include <asm/gpio.h>
#include "phy-nexell-usb2.h"

static int nx_usb2_phy_power_on(struct phy *phy)
{
	struct nx_usb2_phy_pdata *pdata = dev_get_priv(phy->dev);
	struct nx_usb2_phy *p = pdata->phys[phy->id];
	int ret = 0;

	dev_dbg(p->pdata->dev, "Request power on '%s'\n", p->label);

	if (pdata->refcount == 0) {
		if (!IS_ERR(&pdata->clk_ahb)) {
			ret = clk_enable(&pdata->clk_ahb);
			if (ret)
				return ret;
		}
		if (!IS_ERR(&pdata->clk_apb)) {
			ret = clk_enable(&pdata->clk_apb);
			if (ret)
				return ret;
		}
	}

	if (p->power_on)
		ret = p->power_on(p);

	if (!ret)
		pdata->refcount++;

	return ret;
}

static int nx_usb2_phy_power_off(struct phy *phy)
{
	struct nx_usb2_phy_pdata *pdata = dev_get_priv(phy->dev);
	struct nx_usb2_phy *p = pdata->phys[phy->id];
	int ret = 0;

	dev_dbg(p->pdata->dev, "Request power off '%s'\n", p->label);

	if (p->power_off) {
		ret = p->power_off(p);
		if (ret)
			return ret;
	}

	if (!ret) {
		pdata->refcount--;
		if (pdata->refcount == 0) {
			if (!IS_ERR(&pdata->clk_ahb))
				clk_disable(&pdata->clk_ahb);

			if (!IS_ERR(&pdata->clk_apb))
				clk_disable(&pdata->clk_apb);
		}
	}

	return ret;
}

static struct phy_ops nx_usb2_phy_ops = {
	.power_on = nx_usb2_phy_power_on,
	.power_off = nx_usb2_phy_power_off,
};

static const struct udevice_id nx_usb2_phy_of_match[] = {
	{
		.compatible = "nexell,nxp3220-usb2-phy",
		.data = (ulong)&nxp3220_usb2_phy_cfg,
	},
	{ },
};

static void nx_usb2_phy_vbus_gpio(struct udevice *dev, struct nx_usb2_phy *p)
{
	struct gpio_desc desc;
	int ret;

	ret = gpio_request_by_name(dev, p->vbus_gpio, 0, &desc,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret)
		dev_dbg(dev, "cannot request vbus_gpio\n");
}

static int nx_usb2_phy_probe(struct udevice *dev)
{
	struct nx_usb2_phy_pdata *pdata = dev_get_priv(dev);
	const struct nx_usb2_phy_config *cfg;
	int i, ret;

	cfg = (const struct nx_usb2_phy_config *)dev_get_driver_data(dev);
	if (!cfg) {
		dev_err(dev, "Missing usb phy match config ...\n");
		return -EINVAL;
	}

	pdata->cfg = cfg;
	pdata->dev = dev;

	pdata->base = dev_read_addr(dev);
	if (pdata->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_name(dev, "ahb", &pdata->clk_ahb);
	if (!ret) {
		ret = clk_enable(&pdata->clk_ahb);
		if (ret)
			return ret;
	}

	ret = clk_get_by_name(dev, "apb", &pdata->clk_apb);
	if (!ret) {
		ret = clk_enable(&pdata->clk_apb);
		if (ret)
			return ret;
	}

	for (i = 0; i < cfg->num_phys; i++) {
		struct nx_usb2_phy *p = &cfg->phys[i];

		if (i > (MAX_PHYS - 1)) {
			dev_err(dev, "Over max phy.%d num %d\n", i, MAX_PHYS);
			break;
		}

		p->pdata = pdata;

		nx_usb2_phy_vbus_gpio(dev, p);

		if (p->probe) {
			ret = p->probe(p);
			if (ret)
				continue;
		}

		pdata->phys[i] = p;

		dev_dbg(dev, "phy %s bus %d\n", p->label, p->bus_width);
	}

	return 0;
}

U_BOOT_DRIVER(nexell_usb_phy) = {
	.name	= "nexell_usb_phy",
	.id	= UCLASS_PHY,
	.of_match = nx_usb2_phy_of_match,
	.ops = &nx_usb2_phy_ops,
	.probe = nx_usb2_phy_probe,
	.priv_auto_alloc_size = sizeof(struct nx_usb2_phy_pdata),
};
