/*
 * (C) Copyright 2018 Nexell
 * SangJong, Han <hans@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __NEXELL_USB_H
#define __NEXELL_USB_H

/*
 * Vendor  ID = Nexell: 0x2375, Samsung: 0x04e8
 * Product ID = NXP3220: 0x3220, NXP3225: 0x3225, Samsung: 0x1234,
 */
#define VENDORID	0x2375
#define PRODUCTID	0x3220

int nx_usb_download(ulong buffer);
int nx_cpu_id_usbid(u16 *vid, u16 *pid);

#endif
