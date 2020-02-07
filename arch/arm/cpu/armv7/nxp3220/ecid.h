/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _NEXELL_ECID_H
#define _NEXELL_ECID_H

#define CHIPNAME_LEN	48

struct nx_guid {
	u32 guid0;
	u16 guid1;
	u16 guid2;
	u8 guid3[8];
};

int nx_ecid_get_chip_name(u8 *chip_name);
int nx_ecid_get_ecid(u32 ecid[4]);
int nx_ecid_get_guid(struct nx_guid *guid);

#endif /* _NEXELL_ECID_H */
