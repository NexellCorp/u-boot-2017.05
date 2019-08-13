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

/*
 * MMC_BOOT_BUS_WIDTH
 * 0x0 : x1 (sdr) or x4 (ddr) bus width in boot operation mode (default)
 * 0x1 : x4 (sdr/ddr) bus width in boot operation mode
 * 0x2 : x8 (sdr/ddr) bus width in boot operation mode
 */
#ifndef MMC_BOOT_BUS_WIDTH
#define	MMC_BOOT_BUS_WIDTH		2
#endif

/*
 * MMC_BOOT_RESET_BUS_WIDTH
 * 0x0 : Reset bus width to x1, single data rate and backward compatible
 *       timings after boot operation
 * 0x1 : Retain BOOT_BUS_WIDTH and BOOT_MODE values after boot operation.
 *       This is relevant to Push-pull mode operation only
 */
#ifndef MMC_BOOT_RESET_BUS_WIDTH
#define	MMC_BOOT_RESET_BUS_WIDTH	0
#endif
/*
 * MMC_BOOT_MODE
 * 0x0 : Use single data rate + backward compatible timings
 *       in boot operation (default)
 * 0x1 : Use single data rate + High Speed timings in boot operation mode
 * 0x2 : Use dual data rate in boot operation
 */
#ifndef MMC_BOOT_MODE
#define	MMC_BOOT_MODE			0
#endif

#define MB	(1024 * 1024)

static int fb_mmc_make_dos_parts(int dev, char **names, u64 (*parts)[2],
				 int count)
{
	char cmd[2048];
	int i, l, p;

	l = sprintf(cmd, "dospart mmc %d %d:", dev, count);
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

	/* "dospart "mmc" <dev no> [counts] <start:length> <start:length> ..\n" */
	return run_command(cmd, 0);
}

static int fb_mmc_make_gpt_parts(int dev, char **names, u64 (*parts)[2],
				 int count)
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

static int fb_mmc_check_part_table(struct blk_desc *dev_desc,
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

			if ((u64)info.start * (u64)info.blksz != f_part->start)
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

static lbaint_t fb_mmc_sparse_write(struct sparse_storage *info,
				    lbaint_t blk, lbaint_t blkcnt,
				    const void *buffer)
{
	struct blk_desc *dev_desc = info->priv;

	return blk_dwrite(dev_desc, blk, blkcnt, buffer);
}

static lbaint_t fb_mmc_sparse_reserve(struct sparse_storage *info,
				      lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}

/* refer to image-sparse.c */
static void fb_mmc_write_block(struct blk_desc *dev_desc,
			    disk_partition_t *info, const char *part_name,
			    void *buffer, u64 sz)
{
	if (is_sparse_image(buffer)) {
		struct sparse_storage sparse;

		sparse.blksz = info->blksz;
		sparse.start = info->start;
		sparse.size = info->size;
		sparse.write = fb_mmc_sparse_write;
		sparse.reserve = fb_mmc_sparse_reserve;
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

static int fb_mmc_write(struct fb_part_par *f_part, void *buffer,
			u64 bytes)
{
	struct blk_desc *dev_desc;
	int dev = f_part->dev_no;
	disk_partition_t info;
	int blksz = 512;
	char cmd[128];
	int ret = 0;

	debug("part name:%s [0x%x]\n", f_part->name, f_part->type);

	/* set bootsector */
	if (f_part->type & FASTBOOT_PART_BOOT) {
		sprintf(cmd, "mmc bootbus %d %d %d %d", dev,
			MMC_BOOT_BUS_WIDTH,
			MMC_BOOT_RESET_BUS_WIDTH,
			MMC_BOOT_MODE);
		if (run_command(cmd, 0) < 0)
			return -EINVAL;

		#ifdef MMC_BOOT_RESET_FUNCTION
		sprintf(cmd, "mmc rst-function %d 1", dev);
		if (run_command(cmd, 0) < 0)
			return -EINVAL;
		#endif

		/* R/W boot partition 1 */
		sprintf(cmd, "mmc partconf %d 0 1 1", dev);
		if (run_command(cmd, 0) < 0)
			return -EINVAL;
	}

	debug("** mmc.%d partition %s (%s)**\n", dev, f_part->name,
	      f_part->type & PART_TYPE_PARTITION ? "partition" : "data");

	/* set mmc devicee */
	sprintf(cmd, "mmc dev %d", dev);

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
		ret = fb_mmc_check_part_table(dev_desc, f_part);
		if (ret < 0)
			return -EINVAL;

		/* set to partition length */
		info.size = (f_part->length / blksz) +
				((f_part->length & (blksz - 1)) ? 1 : 0);
	}

	debug("** part start: 0x%llx -> 0x%lx **\n", f_part->start, info.start);
	debug("** part size : 0x%llx -> 0x%lx **\n", f_part->length, info.size);
	debug("** bytes     : 0x%llx (0x%lx)  **\n", bytes, info.blksz);

	fb_mmc_write_block(dev_desc, &info, f_part->name, buffer, bytes);

	/* No access to boot partition */
	if (f_part->type & FASTBOOT_PART_BOOT) {
		sprintf(cmd, "mmc partconf %d 0 1 0", dev);
		return run_command(cmd, 0);
	}

	return 0;
}

static int fb_mmc_capacity(int dev, u64 *length)
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

static int fb_mmc_create_part(int dev, char **names, u64 (*parts)[2],
			   int count, enum fb_part_type type)
{
	if (type == FASTBOOT_PART_GPT)
		return fb_mmc_make_gpt_parts(dev, names, parts, count);
	else
		return fb_mmc_make_dos_parts(dev, names, parts, count);
}

static struct fb_part_ops fb_partmap_ops_mmc = {
	.write = fb_mmc_write,
	.capacity = fb_mmc_capacity,
	.create_part = fb_mmc_create_part,
};

static struct fb_part_dev fb_partmap_dev_mmc = {
	.device	= "mmc",
	.dev_max = 4,
	.part_support = FASTBOOT_PART_BOOT | FASTBOOT_PART_RAW |
		FASTBOOT_PART_FS | FASTBOOT_PART_GPT | FASTBOOT_PART_DOS,
	.ops = &fb_partmap_ops_mmc,
};

void fb_partmap_add_dev_mmc(struct list_head *head)
{
	struct fb_part_dev *fd = &fb_partmap_dev_mmc;

	INIT_LIST_HEAD(&fd->list);
	INIT_LIST_HEAD(&fd->part_list);

	list_add_tail(&fd->list, head);
}
