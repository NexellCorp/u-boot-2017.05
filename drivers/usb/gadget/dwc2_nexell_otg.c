/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include <mach/usb.h>

DECLARE_GLOBAL_DATA_PTR;

int board_usb_init(int index, enum usb_init_type init)
{
	struct dwc2_plat_otg_data *pdata;
	struct nx_otg_phy *phy;
	struct udevice *dev, *devp;
	int ret = 0;

	phy = calloc(1, sizeof(*phy));
	if (!phy)
		return -ENOMEM;

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

		phy->dev = dev;
		pdata->priv = phy;

		debug("USB_udc_probe %s\n", dev->name);

		return uclass_get_device_tail(dev, 0, &devp);
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	return 0;
}
