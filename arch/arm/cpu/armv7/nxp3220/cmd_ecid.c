/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <linux/ctype.h>
#include <mach/usb.h>
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

	if (!strncmp(entry, "ecid", 4)) {
		ret = nx_cpu_id_ecid(uid);
	} else if (!strncmp(entry, "guid", 4)) {
		ret = nx_cpu_id_guid(uid);
	} else if (!strncmp(entry, "name", 4)) {
		ret = nx_cpu_id_string(chipname);
		string = 1;
	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		pr_err("ECID module busy\n");
		return ret;
	}

	if (string) {
		if (isprint(chipname[0])) {
			s += snprintf(s, sizeof(chipname), "%s\n", chipname);
		} else {
			#define _W	(12)	/* width */
			int i;

			for (i = 0; i < CHIPNAME_LEN; i++) {
				s += snprintf(s, 2, "%02x", chipname[i]);
				if ((i + 1) % _W == 0)
					s += snprintf(s, 1, " ");
			}
			s += snprintf(s, 1, "\n");
		}
	} else {
		s += snprintf(s, 36, "%08x:%08x:%08x:%08x\n",
			     uid[0], uid[1], uid[2], uid[3]);
	}

	if (s != buf)
		*(s - 1) = '\n';

	printf("%s", buf);

	return s - buf;
}

static int do_efuse(cmd_tbl_t *cmdtp, int flag, int argc,
		    char * const argv[])
{
	if (!strncmp(argv[1], "ecid", 4) ||
	    !strncmp(argv[1], "guid", 4) ||
	    !strncmp(argv[1], "name", 4))
		ecid_show(argv[1]);

	return 0;
}

U_BOOT_CMD(
	cpuid, 2, 0, do_efuse,
	"Nexell nxp3220 ECID Controller",
	"ecid | guid | name\n"
	"  ecid - display 128-bit ECID\n"
	"  guid - display guid\n"
	"  name - display chipname\n"
);

int nx_cpu_id_usbid(u16 *vid, u16 *pid)
{
	u32 id, uid[4] = { 0, };
	int ret;

	ret = nx_cpu_id_ecid(uid);
	if (ret)
		return ret;

	id = uid[3];
	if (!id) {
		/* ecid is not burned */
		*vid = VENDORID;
		*pid = PRODUCTID;
	} else {
		*vid = (id >> 16) & 0xFFFF;
		*pid = id & 0xFFFF;
	}

	return 0;
}

static int do_usbid(cmd_tbl_t *cmdtp, int flag, int argc,
		    char * const argv[])
{
	u16 vid, pid;
	int ret;

	ret = nx_cpu_id_usbid(&vid, &pid);
	if (ret < 0) {
		pr_err("ECID module busy\n");
		return ret;
	}

	if (argc < 2) {
		printf("VID %4x, PID %4x\n", vid, pid);
		return 0;
	}

	if (!strcmp("vid", argv[1]))
		printf("VID  : %4x\n", vid);
	else if (!strcmp("pid", argv[1]))
		printf("PID  : %4x\n", pid);

	return 0;
}

U_BOOT_CMD(
	usbid, 2, 1, do_usbid,
	"Nexell USB ID from ECID",
	"<vid | pid>\n"
);
