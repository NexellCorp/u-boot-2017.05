/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <linux/io.h>
#include <mach/usb.h>
#include "../gadget/dwc2_udc_otg_priv.h"

void nx_otg_phy_init(struct nx_otg_phy *phy)
{
	void __iomem *base = phy->addr;

	/* USB PHY0 Enable */
	debug("USB PHY0 Enable :%p\n", base);

	/*
	 * Must be enabled 'adb400 blk usb cfg' and 'data adb'
	 * 'adb400 blk usb cfg' and 'data adb' is shared EHCI
	 */

	/*
	 * PHY POR release
	 * SYSREG_USB_USB20PHY_OTG0_i_POR/SYSREG_USB_USB20PHY_OTG0_i_POR_ENB
	 */
	clrbits_le32(base + 0x80, (1 << 4));
	clrsetbits_le32(base + 0x80, (1 << 3), (1 << 3));

	udelay(50);

	/*
	 * PHY reset release in OTG LINK
	 * SYSREG_USB_OTG_i_nUtmiResetSync
	 */
	clrsetbits_le32(base + 0x60, (1 << 8), (1 << 8));

	udelay(1);

	/*
	 * BUS reset release in OTG LINK
	 * SYSREG_USB_OTG_i_nResetSync
	 */
	clrsetbits_le32(base + 0x60, (1 << 7), (1 << 7));

	udelay(1);
}

void nx_otg_phy_off(struct nx_otg_phy *phy)
{
	void __iomem *base = phy->addr;

	/* USB PHY0 Disable */
	debug("USB PHY0 Disable :%p\n", base);

	/*
	 * PHY reset in OTG LINK
	 * SYSREG_USB_OTG_i_nUtmiResetSync
	 */
	clrbits_le32(base + 0x60, (1 << 8));

	/*
	 * BUS reset in OTG LINK
	 * SYSREG_USB_OTG_i_nResetSync
	 */
	clrbits_le32(base + 0x60, (1 << 7));

	/*
	 * PHY POR
	 * SYSREG_USB_USB20PHY_OTG0_i_POR/SYSREG_USB_USB20PHY_OTG0_i_POR_ENB
	 */
	clrsetbits_le32(base + 0x80, (1 << 4), (1 << 4));

	/*
	 * Don't disable 'adb400 blk usb cfg' and 'data adb'
	 * 'adb400 blk usb cfg' and 'data adb' is shared EHCI
	 */
}

void otg_phy_init(struct dwc2_udc *dev)
{
	struct dwc2_plat_otg_data *pdata = dev->pdata;
	struct nx_otg_phy *phy;

	if (!pdata->priv)
		return;

	phy = pdata->priv;
	phy->addr = (void __iomem *)pdata->regs_phy;

	nx_otg_phy_init(phy);
}

void otg_phy_off(struct dwc2_udc *dev)
{
	struct dwc2_plat_otg_data *pdata = dev->pdata;
	struct nx_otg_phy *phy;

	if (!pdata->priv)
		return;

	phy = pdata->priv;
	phy->addr = (void __iomem *)pdata->regs_phy;

	nx_otg_phy_off(phy);
}
