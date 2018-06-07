/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <linux/ctype.h>
#include "ecid.h"

static int nx_cpu_id_guid(u32 guid[4])
{
	if (nx_ecid_get_key_ready() < 0)
		return -EBUSY;
	nx_ecid_get_guid((struct nx_guid *)guid);
	return 0;
}

static int nx_cpu_id_ecid(u32 ecid[4])
{
	if (nx_ecid_get_key_ready() < 0)
		return -EBUSY;
	nx_ecid_get_ecid(ecid);
	return 0;
}

static int nx_cpu_id_string(u8 *chipname)
{
	if (nx_ecid_get_key_ready() < 0)
		return -EBUSY;
	nx_ecid_get_chip_name(chipname);
	return 0;
}

static int ecid_show(char *entry)
{
	char buf[128];
	char *s = buf;
	u32 uid[4] = { 0, };
	u8 chipname[CHIPNAME_LEN + 1] = { 0, };
	int string = 0;
	int ret = 0;

	if (!strcmp(entry, "ecid")) {
		ret = nx_cpu_id_ecid(uid);
	} else if (!strcmp(entry, "guid")) {
		ret = nx_cpu_id_guid(uid);
	} else if (!strcmp(entry, "name")) {
		ret = nx_cpu_id_string(chipname);
		string = 1;
	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		error("ECID module busy\n");
		return ret;
	}

	if (string) {
		if (isprint(chipname[0])) {
			s += sprintf(s, "%s\n", chipname);
		} else {
			#define _W	(12)	/* width */
			int i;

			for (i = 0; i < CHIPNAME_LEN; i++) {
				s += sprintf(s, "%02x", chipname[i]);
				if ((i + 1) % _W == 0)
					s += sprintf(s, " ");
			}
			s += sprintf(s, "\n");
		}
	} else {
		s += sprintf(s, "%08x:%08x:%08x:%08x\n",
			     uid[0], uid[1], uid[2], uid[3]);
	}

	if (s != buf)
		*(s-1) = '\n';

	printf("%s", buf);

	return s - buf;
}

static int do_efuse(cmd_tbl_t *cmdtp, int flag, int argc,
		char * const argv[])
{
	if (!strcmp(argv[1], "ecid") ||
	    !strcmp(argv[1], "guid") ||
	    !strcmp(argv[1], "name"))
		ecid_show(argv[1]);

	return 0;
}

U_BOOT_CMD(
	cpuid, 2, 0, do_efuse,
	"Nexell nxp3220 ECID Controller",
	"ecid | guid | name"
);
