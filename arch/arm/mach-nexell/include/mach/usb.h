/*
 * (C) Copyright 2018 Nexell
 * SangJong, Han <hans@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __NEXELL_USB_H
#define __NEXELL_USB_H

struct nx_otg_phy {
	struct udevice *dev;
	void __iomem *addr;
};

void nx_otg_phy_init(struct nx_otg_phy *phy);
void nx_otg_phy_off(struct nx_otg_phy *phy);

int nx_usb_download(ulong buffer);

#endif
