/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <errno.h>
#include <dm.h>
#include <misc.h>
#include <fastboot.h>
#include <hexdump.h>
#include <linux/ctype.h>
#include <linux/err.h>

#include "f_fastboot_partmap.h"

static int fb_efuse_update(struct fb_part_par *f_part, void *buffer, u64 bytes)
{
	struct udevice *dev;
	char cmd[128] = { 0, };
	int offs, bits;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_GET_DRIVER(nexell_efuse), &dev);
	if (ret) {
		printf("%s: efuse-device not found !!!\n", __func__);
		return -EINVAL;
	}

	if (!f_part->length) {
		printf("%s: error not set bits length !!!\n", __func__);
		return -EINVAL;
	}

	sprintf((char *)cmd, "%llx", f_part->length);
	offs = f_part->start;
	bits = simple_strtol((char *)cmd, NULL, 10);

	debug("part name:%s [%s]\n",
	      f_part->name, print_part_type(f_part->type));
	debug("** efuse.%d **\n", f_part->dev_no);
	debug("** offs : 0x%x **\n", offs);
	debug("** bits : %d (load %lldbytes)**\n", bits, bytes);
	debug("** buff : %p **\n", buffer);

	/* set command */
	sprintf(cmd, "efuse write.hex 0x%x 0x%x %d", (u32)buffer, offs, bits);

	debug("** cmd %s: %s **\n", f_part->name, cmd);

	ret = run_command(cmd, 0);
	if (ret < 0) {
		printf("Error: cmd %s: %s, ret.%d\n", f_part->name, cmd, ret);
		return ret;
	}

	fastboot_okay("");

	return ret;
}

static struct fb_part_ops fb_partmap_ops_efuse = {
	.write = fb_efuse_update,
};

static struct fb_part_dev fb_partmap_dev_efuse = {
	.device	= "efuse",
	.dev_max = 1,
	.mask = FASTBOOT_PART_RAW,
	.ops = &fb_partmap_ops_efuse,
};

FB_PARTMAP_BIND_INIT(efuse, &fb_partmap_dev_efuse)
