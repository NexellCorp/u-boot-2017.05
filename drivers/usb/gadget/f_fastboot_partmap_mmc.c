/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <fastboot.h>
#include <image-sparse.h>
#include <div64.h>
#include <linux/err.h>

#include "f_fastboot_partmap.h"

#define MB	(1024 * 1024)

#ifdef CONFIG_FASTBOOT_FLASH
static int mmc_make_mbr_parts(int dev, char **names, u64 (*parts)[2], int count)
{
	char cmd[2048];
	int i, l, p;

	l = sprintf(cmd, "mbr mmc %d %d:", dev, count);
	p = l;
	for (i = 0; i < count; i++) {
		l = sprintf(&cmd[p], " 0x%llx:0x%llx",
			    parts[i][0], parts[i][1]);
		p += l;
	}

	if (sizeof(cmd) <= p) {
		printf("** %s: cmd stack overflow : stack %zu, cmd %d **\n",
		       __func__, sizeof(cmd), p);
		return -EINVAL;
	}

	cmd[p] = 0;
	printf("%s\n", cmd);

	/* "mbr "mmc" <dev no> [counts] <start:length> <start:length> ..\n" */
	return run_command(cmd, 0);
}

static int mmc_make_gpt_parts(int dev, char **names, u64 (*parts)[2], int count)
{
	char const *gpt_head[] = {
		"uuid_disk=${uuid_gpt_disk};"
	};
	char cmd[2048];
	int i, l, p;

	for (p = 0, i = 0; i < ARRAY_SIZE(gpt_head); i++) {
		strcpy(&cmd[p], gpt_head[i]);
		p += strlen(gpt_head[i]);
	}

	for (i = 0; i < count; i++) {
		if (parts[i][1])
			l = sprintf(&cmd[p],
				    "name=%s,start=%lldMiB,size=%lldMiB,uuid=${uuid_gpt_%s};",
				    names[i], parts[i][0] / MB, parts[i][1] / MB, names[i]);
		else
			l = sprintf(&cmd[p],
				    "name=%s,start=%lldMiB,size=-,uuid=${uuid_gpt_%s};",
				    names[i], parts[i][0] / MB, names[i]);
		p += l;
	}

	if (sizeof(cmd) <= p) {
		printf("** %s: cmd stack overflow : stack %zu, cmd %d **\n",
		       __func__, sizeof(cmd), p);
		return -EINVAL;
	}

	cmd[p] = 0;
	env_set("partitions", cmd);

	sprintf(cmd, "gpt write mmc %d $partitions", dev);

	return run_command(cmd, 0);
}

static int mmc_check_part_table(struct blk_desc *dev_desc,
				struct fb_part_par *f_part)
{
	struct part_driver *first_drv =
		ll_entry_start(struct part_driver, part_driver);
	const int n_drvs = ll_entry_count(struct part_driver, part_driver);
	struct part_driver *part_drv;
	disk_partition_t info;

	for (part_drv = first_drv; part_drv != first_drv + n_drvs; part_drv++) {
		int i, ret;

		for (i = 1; i < part_drv->max_entries; i++) {
			ret = part_drv->get_info(dev_desc, i, &info);
			if (ret)
				continue;

			if (info.start * info.blksz != f_part->start)
				continue;

			/*
			 * when last partition set value is zero,
			 * set available length
			 */
			if (f_part->length == 0)
				f_part->length =
					(u64)info.size * (u64)info.blksz;

			if ((u64)info.size * (u64)info.blksz == f_part->length)
				return 0;
		}
	}

	return -EINVAL;
}

static lbaint_t mmc_sparse_write(struct sparse_storage *info,
				 lbaint_t blk, lbaint_t blkcnt,
				 const void *buffer)
{
	struct blk_desc *dev_desc = info->priv;

	return blk_dwrite(dev_desc, blk, blkcnt, buffer);
}

static lbaint_t mmc_sparse_reserve(struct sparse_storage *info,
				   lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}

/* refer to image-sparse.c */
static void mmc_write_block(struct blk_desc *dev_desc,
			    disk_partition_t *info, const char *part_name,
			    void *buffer, u64 sz)
{
	if (is_sparse_image(buffer)) {
		struct sparse_storage sparse;

		sparse.blksz = info->blksz;
		sparse.start = info->start;
		sparse.size = info->size;
		sparse.write = mmc_sparse_write;
		sparse.reserve = mmc_sparse_reserve;
		sparse.priv = dev_desc;

		printf("Flashing sparse image at offset 0x%x/0x%x\n",
		       (unsigned int)sparse.start, (unsigned int)sparse.size);

		write_sparse_image(&sparse, part_name, buffer, sz);
	} else {
		lbaint_t blkcnt, blks;

		/* determine number of blocks to write */
		blkcnt = ((sz + (info->blksz - 1)) & ~(info->blksz - 1));
		blkcnt = lldiv(blkcnt, info->blksz);

		if (info->size < blkcnt) {
			printf("** Fail too large for partition: '%s'\n",
			       part_name);
			return;
		}

		puts("Flashing Raw Image\n");

		blks = blk_dwrite(dev_desc, info->start, blkcnt, buffer);
		if (blks != blkcnt) {
			pr_err("failed writing to device %d\n",
			       dev_desc->devnum);
			fastboot_fail("failed writing to device");
			return;
		}

		printf("........ wrote " LBAFU " bytes to '%s'\n",
		       blkcnt * info->blksz, part_name);
	}
	fastboot_okay("");
}

static int mmc_write(struct fb_part_par *f_part, void *buffer,
		     u64 bytes)
{
	struct blk_desc *dev_desc;
	int dev = f_part->dev_no;
	disk_partition_t info;
	int blksz = 512;
	char cmd[128];
	int ret = 0;

	sprintf(cmd, "mmc bootbus %d 2 0 0", dev);
	if (run_command(cmd, 0) < 0)
		return -EINVAL;

	if (f_part->type & FASTBOOT_PART_BOOT)
		sprintf(cmd, "mmc partconf %d 0 1 1", dev);
	else
		sprintf(cmd, "mmc partconf %d 0 1 0", dev);

	if (run_command(cmd, 0) < 0)
		return -EINVAL;

	sprintf(cmd, "mmc dev %d", dev);

	debug("** mmc.%d partition %s (%s)**\n", dev, f_part->name,
	      f_part->type & PART_TYPE_PARTITION ? "partition" : "data");

	/* set mmc devicee */
	ret = blk_get_device_by_str("mmc", simple_itoa(dev), &dev_desc);
	if (ret < 0) {
		ret = run_command(cmd, 0);
		if (ret < 0)
			return -EINVAL;

		ret = run_command("mmc rescan", 0);
		if (ret < 0)
			return -EINVAL;
	}

	info.start = f_part->start / blksz;
	info.size = (bytes / blksz) + ((bytes & (blksz - 1)) ? 1 : 0);
	info.blksz = blksz;

	if (f_part->type & PART_TYPE_PARTITION) {
		ret = mmc_check_part_table(dev_desc, f_part);
		if (ret < 0)
			return -EINVAL;

		/* set to partition length */
		info.size = (f_part->length / blksz) +
				((f_part->length & (blksz - 1)) ? 1 : 0);
	}

	debug("** part start: 0x%llx -> 0x%lx **\n", f_part->start, info.start);
	debug("** part size : 0x%llx -> 0x%lx **\n", f_part->length, info.size);
	debug("** bytes     : 0x%llx (0x%lx)  **\n", bytes, info.blksz);

	mmc_write_block(dev_desc, &info, f_part->name, buffer, bytes);

	return 0;
}

static int mmc_capacity(int dev, u64 *length)
{
	struct blk_desc *dev_desc;
	char cmd[32];
	int ret;

	debug("** mmc.%d capacity **\n", dev);

	/* set mmc devicee */
	ret = blk_get_device_by_str("mmc", simple_itoa(dev), &dev_desc);
	if (ret < 0) {
		sprintf(cmd, "mmc dev %d", dev);

		ret = run_command(cmd, 0);
		if (ret < 0)
			return -EINVAL;

		ret = run_command("mmc rescan", 0);
		if (ret < 0)
			return -EINVAL;
	}

	ret = blk_get_device_by_str("mmc", simple_itoa(dev), &dev_desc);
	if (ret < 0)
		return -EINVAL;

	*length = (u64)dev_desc->lba * (u64)dev_desc->blksz;

	debug("%u*%u = %llu\n",
	      (uint)dev_desc->lba, (uint)dev_desc->blksz, *length);

	return 0;
}

static int mmc_create_part(int dev, char **names, u64 (*parts)[2],
			   int count, enum fb_part_type type)
{
	if (type == FASTBOOT_PART_GPT)
		return mmc_make_gpt_parts(dev, names, parts, count);
	else
		return mmc_make_mbr_parts(dev, names, parts, count);
}

static struct fb_part_ops fb_partmap_ops_mmc = {
	.write = mmc_write,
	.capacity = mmc_capacity,
	.create_part = mmc_create_part,
};

static struct fb_part_dev fb_partmap_dev_mmc = {
	.device	= "mmc",
	.dev_max = 3,
	.part_support = FASTBOOT_PART_BOOT | FASTBOOT_PART_RAW |
		FASTBOOT_PART_FS | FASTBOOT_PART_GPT | FASTBOOT_PART_MBR,
	.ops = &fb_partmap_ops_mmc,
};

void fb_partmap_add_dev_mmc(struct list_head *head)
{
	struct fb_part_dev *fd = &fb_partmap_dev_mmc;

#if defined CONFIG_FASTBOOT_PARTMAP_MBR
	printf("FASTBOOT PARTMAP: MBR\n");
#endif
#if defined CONFIG_FASTBOOT_PARTMAP_GPT
	printf("FASTBOOT PARTMAP: GPT\n");
#endif
	INIT_LIST_HEAD(&fd->list);
	INIT_LIST_HEAD(&fd->part_list);

	list_add_tail(&fd->list, head);
}
#endif
