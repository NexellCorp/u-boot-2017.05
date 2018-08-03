/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <clk.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include <generic-phy.h>

#include "./dwc2_udc_otg_priv.h"

DECLARE_GLOBAL_DATA_PTR;

static struct clk otg_clk;

void otg_phy_init(struct dwc2_udc *dev)
{
	struct dwc2_plat_otg_data *pdata = dev->pdata;
	struct phy *phy = pdata->priv;
	struct clk *clk = &otg_clk;

	if (clk->dev)
		clk_enable(clk);

	if (!phy)
		return;

	generic_phy_init(phy);
	generic_phy_power_on(phy);
}

void otg_phy_off(struct dwc2_udc *dev)
{
	struct dwc2_plat_otg_data *pdata = dev->pdata;
	struct phy *phy = pdata->priv;
	struct clk *clk = &otg_clk;

	if (clk->dev)
		clk_disable(clk);

	if (!phy)
		return;

	generic_phy_power_off(phy);
	generic_phy_exit(phy);
}

int board_usb_init(int index, enum usb_init_type init)
{
	struct dwc2_plat_otg_data *pdata;
	struct udevice *dev, *devp;
	int ret = 0;

	for (ret = uclass_find_first_device(UCLASS_USB, &dev); dev;
		ret = uclass_find_next_device(&dev)) {
		if (ret)
			continue;

		/* compare device node name: dwc2otg@xxxxxxxx */
		if (strncmp(dev->name, "dwc2otg", strlen("dwc2otg")))
			continue;

		pdata = dev_get_platdata(dev);
		if (!pdata)
			continue;

		clk_get_by_index(dev, 0, &otg_clk);

		debug("USB_udc_probe %s\n", dev->name);

		return uclass_get_device_tail(dev, 0, &devp);
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	return 0;
}
