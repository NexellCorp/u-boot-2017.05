/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <fastboot.h>
#include <div64.h>
#include <linux/err.h>
#include <nand.h>

#include "f_fastboot_partmap.h"

static char *print_part_type(int fb_part_type)
{
	char *s;

	if (fb_part_type == FASTBOOT_PART_BOOT)
		s = "boot";
	else if (fb_part_type == FASTBOOT_PART_RAW)
		s = "raw";
	else if (fb_part_type == FASTBOOT_PART_UBI)
		s = "ubi";
	else if (fb_part_type == FASTBOOT_PART_UBIFS)
		s = "ubifs";
	else
		s = "unknown";

	return s;
}

static int fb_nand_write(struct fb_part_par *f_part, void *buffer, u64 bytes)
{
	struct mtd_info *mtd;
	int dev = f_part->dev_no;
	char cmd[128];
	int l = 0, p = 0;

	mtd = get_nand_dev_by_index(0);

	/* nand standalone (bl1, bl2, bl32, u-boot)
	 *	bingened image			// FASTBOOT_PART_BOOT
	 *	"nand		write.rsv	0x48000000 0x0 0x20000"
	 *
	 * normal
	 *	u-boot env			// FASTBOOT_PART_RAW
	 *	"nand		write		0x48000000 0x400000 0x10000"
	 *
	 *	ubi image (kernel/dtb, rootfs, ...)	// FASTBOOT_PART_UBI
	 *	"nand		write.trimffs	0x48000000 0x2000000 0x2000000"
	 */

	if (f_part->length == 0)
		f_part->length = round_down(nand_size() * 1024 - f_part->start,
				mtd->erasesize);

	debug("part name:%s [0x%x]\n", f_part->name, f_part->type);

	debug("** nand.%d partition %s (%s)**\n", dev, f_part->name,
	      print_part_type(f_part->type));

	debug("** part start: 0x%llx **\n", f_part->start);
	debug("** part size : 0x%llx **\n", f_part->length);
	debug("** file size : 0x%llx **\n", bytes);
	debug("** write size: 0x%x **\n", mtd->writesize);
	debug("** erase size: 0x%x **\n", mtd->erasesize);

	/*
	 * erase block
	 */
	if (f_part->type & FASTBOOT_PART_BOOT)
		p = sprintf(cmd, "update_nand ");
	else
		p = sprintf(cmd, "nand ");

	l = sprintf(&cmd[p], "%s", "erase");
	p += l;
	l = sprintf(&cmd[p], " 0x%llx 0x%llx", f_part->start, f_part->length);
	p += l;
	cmd[p] = 0;

	debug("** run cmd: %s **\n", cmd);
	run_command(cmd, 0);

	/*
	 * write image data
	 */
	l = 0, p = 0;
	if (f_part->type & FASTBOOT_PART_BOOT)
		p = sprintf(cmd, "update_nand ");
	else
		p = sprintf(cmd, "nand ");

	if (f_part->type & FASTBOOT_PART_UBI)
		l = sprintf(&cmd[p], "%s", "write.trimffs");
	else
		l = sprintf(&cmd[p], "%s", "write");
	p += l;

	l = sprintf(&cmd[p], " 0x%x 0x%llx 0x%llx",
			(unsigned int)buffer, f_part->start, bytes);
	p += l;
	cmd[p] = 0;

	debug("** run cmd: %s **\n", cmd);
	run_command(cmd, 0);

	fastboot_okay("");

	return 0;
}

static int fb_nand_flash_capacity(int dev, u64 *length)
{
	debug("** nand capacity **\n");

	*length = nand_size() * 1024;

	debug("total size = %llu (%llu MB)\n", *length, *length / 1024 / 1024);

	return 0;
}

static struct fb_part_ops fb_partmap_ops_nand = {
	.write = fb_nand_write,
	.capacity = fb_nand_flash_capacity,
};

static struct fb_part_dev fb_partmap_dev_nand = {
	.device	= "nand",
	.dev_max = 1,
	.part_support = FASTBOOT_PART_BOOT | FASTBOOT_PART_RAW |
		FASTBOOT_PART_FS | FASTBOOT_PART_UBI | FASTBOOT_PART_UBIFS,
	.ops = &fb_partmap_ops_nand,
};

void fb_partmap_add_dev_nand(struct list_head *head)
{
	struct fb_part_dev *fd = &fb_partmap_dev_nand;

	INIT_LIST_HEAD(&fd->list);
	INIT_LIST_HEAD(&fd->part_list);

	list_add_tail(&fd->list, head);
}
