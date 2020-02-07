/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <linux/io.h>
#include "ecid.h"

#define PHY_BASEADDR_ECID		0x20060000

struct nx_ecid_regs {
	u8 chipname[CHIPNAME_LEN]; /* 0x00 */
	u32 __reserved_0x30;
	u32 guid0; /* 0x34 */
	u16 guid1; /* 0x38 */
	u16 guid2; /* 0x3a */
	u8 guid3[8]; /* 0x3c */
	u32 ec0; /* 0x44 */
	u32 __reserved_0x48;
	u32 ec2; /* 0x4c */
	u32 __reserved_0x50[(0x100 - 0x50) / 4];
	u32 ecid[4]; /* 0x100 */
};

static struct nx_ecid_regs *reg = (struct nx_ecid_regs *)PHY_BASEADDR_ECID;

static int nx_ecid_key_ready(void)
{
	const u32 ready_pos = 16; /* sense done */
	const u32 ready_mask = 1ul << ready_pos;

	u32 regval = readl(&reg->ec2);

	return (int)((regval & ready_mask) >> ready_pos);
}

int nx_ecid_get_chip_name(u8 *chip_name)
{
	int i;
	u8 *c = chip_name;

	if (nx_ecid_key_ready() < 0)
		return -EBUSY;

	for (i = 0; i < CHIPNAME_LEN; i++)
		c[i] = readb(&reg->chipname[i]);

	for (i = CHIPNAME_LEN - 1; i >= 0; i--) {
		if (c[i] != '-')
			break;
		c[i] = 0;
	}

	return 0;
}

int nx_ecid_get_ecid(u32 ecid[4])
{
	if (nx_ecid_key_ready() < 0)
		return -EBUSY;

	ecid[0] = readl(&reg->ecid[0]);
	ecid[1] = readl(&reg->ecid[1]);
	ecid[2] = readl(&reg->ecid[2]);
	ecid[3] = readl(&reg->ecid[3]);

	return 0;
}

int nx_ecid_get_guid(struct nx_guid *guid)
{
	if (nx_ecid_key_ready() < 0)
		return -EBUSY;

	guid->guid0 = readl(&reg->guid0);
	guid->guid1 = readw(&reg->guid1);
	guid->guid2 = readw(&reg->guid2);
	guid->guid3[0] = readb(&reg->guid3[0]);
	guid->guid3[1] = readb(&reg->guid3[1]);
	guid->guid3[2] = readb(&reg->guid3[2]);
	guid->guid3[3] = readb(&reg->guid3[3]);
	guid->guid3[4] = readb(&reg->guid3[4]);
	guid->guid3[5] = readb(&reg->guid3[5]);
	guid->guid3[6] = readb(&reg->guid3[6]);
	guid->guid3[7] = readb(&reg->guid3[7]);

	return 0;
}
