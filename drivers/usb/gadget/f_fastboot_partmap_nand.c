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

extern int get_nand_chip_datasize(struct nand_chip *chip);
extern int get_nand_chip_eccbyte(struct nand_chip *chip);

static struct list_head *fd_dev_head;

static char *print_part_type(int fb_part_type)
{
	char *s;

	if (fb_part_type == FASTBOOT_PART_BOOT)
		s = "boot";
	else if (fb_part_type == FASTBOOT_PART_RAW)
		s = "raw";
	else if (fb_part_type == PART_TYPE_PARTITION)
		s = "ubi";
	else
		s = "unknown";

	return s;
}

static uint32_t get_nand_bld_block_size(struct mtd_info *mtd,
					struct fb_part_par *f_part,
					loff_t bytes)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int dmasize = get_nand_chip_datasize(chip);
	int eccbyte = get_nand_chip_eccbyte(chip);
	int count;

	count = lldiv(bytes, dmasize);

	if (bytes > ((loff_t)count * (loff_t)dmasize))
		count += 1;

	return roundup((count * (dmasize + eccbyte)), mtd->erasesize);
}

static loff_t get_nand_bld_start_offset(struct mtd_info *mtd,
					struct fb_part_par *f_part,
					loff_t bytes)
{
	loff_t erasesize = mtd->erasesize;
	loff_t start = f_part->start;
	loff_t offset;
	int count, size, offs;
	int total, i;

	count = lldiv(start, erasesize);
	size = nand_size() * 1024;
	total = lldiv(size, erasesize);

	if (start > ((loff_t)count * (loff_t)erasesize)) {
		printf("Error: %s start 0x%llx not alinged erase size 0x%llx\n",
		       f_part->name, start, erasesize);
		return -1;
	}

	for (i = 0, offs = -1; i < total ; i++) {
		if (!nand_block_isbad(mtd, (loff_t)erasesize * (loff_t)i))
			offs++;

		if (offs == count)
			break;
	}

	offset = (loff_t)erasesize * (loff_t)i;
	if ((offset + bytes) > size) {
		printf("Error: %s start 0x%llx -> 0x%llx, size 0x%llx over chip size 0x%x\n",
		       f_part->name, start, offset, bytes, size);
		return -1;
	}

	return offset;
}

static int fb_nand_write(struct fb_part_par *f_part, void *buffer, u64 bytes)
{
	struct mtd_info *mtd;
	char cmd[128];
	loff_t start = f_part->start;
	loff_t erasesize;
	int l, p;

	mtd = get_nand_dev_by_index(0);

	if (f_part->length == 0)
		f_part->length = round_down(nand_size() * 1024 - f_part->start,
					    mtd->erasesize);

	if (f_part->type & FASTBOOT_PART_BOOT) {
		start = get_nand_bld_start_offset(mtd, f_part, bytes);
		erasesize = get_nand_bld_block_size(mtd, f_part, bytes);
	} else {
		erasesize = f_part->length;
	}

	if (start == -1) {
		printf("Error %s: invalid start offset:0x%llx\n",
		       f_part->name, f_part->start);
		return -EINVAL;
	}

	if (erasesize > f_part->length) {
		printf("Error %s: size 0x%llx require erase size 0x%llx over part size 0x%llx\n",
		       f_part->name, bytes, erasesize, f_part->length);
		return -EINVAL;
	}

	debug("part name:%s [0x%x]\n", f_part->name, f_part->type);
	debug("** nand.%d partition %s (%s)**\n", f_part->dev_no, f_part->name,
	      print_part_type(f_part->type));
	debug("** part start: 0x%llx -> 0x%llx **\n", f_part->start, start);
	debug("** part size : 0x%llx **\n", f_part->length);
	debug("** file size : 0x%llx **\n", bytes);
	debug("** erase size: 0x%llx **\n", erasesize);

	/* set command */
	p = sprintf(cmd, "nand ");
	l = sprintf(&cmd[p], "%s", "erase");
	p += l;
	l = sprintf(&cmd[p], " 0x%llx 0x%llx", start, erasesize);
	p += l;
	cmd[p] = 0;

	printf("** cmd %s: %s **\n", f_part->name, cmd);
	run_command(cmd, 0);

	/* nand standalone (bl1, bl2, bl32, u-boot)
	 *	bootloader image : FASTBOOT_PART_BOOT (bootsector)
	 *	"nand bwrite <addr> <offset> <size>
	 *
	 * normal
	 *	u-boot env image : FASTBOOT_PART_RAW (raw)
	 *	"nand write <addr> <offset> <size>
	 *
	 * partition
	 *	ubi image : PART_TYPE_PARTITION (boot.img/rootfs.img ...)
	 *	"nand write.trimffs <addr> <offset> <size>
	 */
	l = 0;
	p = sprintf(cmd, "nand ");

	if (f_part->type & PART_TYPE_PARTITION)
		l = sprintf(&cmd[p], "%s", "write.trimffs");
	else if (f_part->type & FASTBOOT_PART_BOOT)
		l = sprintf(&cmd[p], "%s", "bwrite");
	else
		l = sprintf(&cmd[p], "%s", "write");

	p += l;
	l = sprintf(&cmd[p], " 0x%x 0x%llx 0x%llx",
		    (unsigned int)buffer, start, bytes);
	p += l;
	cmd[p] = 0;

	printf("** cmd %s: %s, erase:0x%08llx **\n",
	       f_part->name, cmd, erasesize);
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
		FASTBOOT_PART_FS,
	.ops = &fb_partmap_ops_nand,
};

void fb_partmap_add_dev_nand(struct list_head *head)
{
	struct fb_part_dev *fd = &fb_partmap_dev_nand;

	INIT_LIST_HEAD(&fd->list);
	INIT_LIST_HEAD(&fd->part_list);

	list_add_tail(&fd->list, head);

	fd_dev_head = head;
}
