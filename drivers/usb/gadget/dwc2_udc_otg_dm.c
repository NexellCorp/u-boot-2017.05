/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include <linux/io.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct dm_usb_ops dwc2_udc_ops = {                             
}; 

static int dwc2_udc_ofdata_to_platdata(struct udevice *dev)
{
	struct dwc2_plat_otg_data *pdata = dev_get_platdata(dev);
	fdt_addr_t base;

	debug("%s: %p\n", __func__, pdata);

	base = dev_get_addr_index(dev, 0);
	if (base == FDT_ADDR_T_NONE) {
		error("'phy' resoure not found");
		return -EINVAL;
	}
	pdata->regs_phy = (unsigned int)devm_ioremap(dev, base, SZ_32);

	base = dev_get_addr_index(dev, 1);
	if (base == FDT_ADDR_T_NONE) {
		error("'otg' resoure not found");
		return -EINVAL;
	}
	pdata->regs_otg = (unsigned int)devm_ioremap(dev, base, SZ_32);

	debug("%s: phy 0x%x otg 0x%x\n", __func__,
	      pdata->regs_phy, pdata->regs_otg);

	return 0;
}

static int dwc2_udc_dm_probe(struct udevice *dev)
{
	struct dwc2_plat_otg_data *pdata = dev_get_platdata(dev);

	return dwc2_udc_probe(pdata);
}

static int dwc2_udc_dm_remove(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id dwc2_udc_ids[] = {
	{ .compatible = "nexell,dwc2-udc-otg" },
	{ }
};

U_BOOT_DRIVER(dwc2_udc) = {
	.name = "dwc2_udc",
	.id = UCLASS_USB,
	.of_match = dwc2_udc_ids,
	.ofdata_to_platdata = dwc2_udc_ofdata_to_platdata,
	.probe = dwc2_udc_dm_probe,
	.remove = dwc2_udc_dm_remove,
	.ops = &dwc2_udc_ops,
	.platdata_auto_alloc_size = sizeof(struct dwc2_plat_otg_data),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};

