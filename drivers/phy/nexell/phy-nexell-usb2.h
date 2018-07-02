/* SPDX-License-Identifier:	GPL-2.0+ */

#ifndef _PHY_NEXELL_USB2_H
#define _PHY_NEXELL_USB2_H

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device.h>
#include <generic-phy.h>
#include <linux/compat.h>

#define MAX_PHYS	4

struct nx_usb2_phy {
	struct phy *phy;
	u32 base;
	const char *label;
	const char *vbus_gpio;
	int port;
	int bus_width;
	struct nx_usb2_phy_pdata *pdata;
	int (*probe)(struct nx_usb2_phy *p);
	int (*power_on)(struct nx_usb2_phy *p);
	int (*power_off)(struct nx_usb2_phy *p);
};

struct nx_usb2_phy_config {
	struct nx_usb2_phy *phys;
	int num_phys;
};

struct nx_usb2_phy_pdata {
	u32 base;
	struct udevice *dev;
	int refcount;
	struct clk clk_ahb;
	struct clk clk_apb;
	const struct nx_usb2_phy_config *cfg;
	struct nx_usb2_phy *phys[MAX_PHYS];
};

extern const struct nx_usb2_phy_config nxp3220_usb2_phy_cfg;

#endif
