/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#ifndef _F_FASTBOOT_PARTMAP_H
#define _F_FASTBOOT_PARTMAP_H

enum fb_part_type {
	FASTBOOT_PART_BOOT = (1 << 0),	/* bootable partition */
	FASTBOOT_PART_RAW = (1 << 1),	/* raw binary data or 'ASCII text' */
	FASTBOOT_PART_FS = (1 << 2),	/* set partition table: ex) ext4,fat, ubi */
};

#define	PART_TYPE_PARTITION	(FASTBOOT_PART_FS)
#define	FASTBOOT_PART_GPT	(FASTBOOT_PART_FS | (1 << 4))
#define	FASTBOOT_PART_DOS	(FASTBOOT_PART_FS | (1 << 5))

/* each device max partition max num */
#define	FASTBOOT_DEV_PART_MAX		(16)

struct fb_part_par {
	char name[64];	/* partition name: ex> boot, env, ... */
	int dev_no;
	u64 start;
	u64 length;
	enum fb_part_type type;
	struct fb_part_dev *fd;
	struct list_head link;
};

struct fb_part_ops {
	int (*write)(struct fb_part_par *f_part, void *buffer, u64 bytes);
	int (*capacity)(int dev, u64 *length);
	int (*create_part)(int dev, char **names, u64 (*parts)[2], int count,
			   enum fb_part_type type);
};

struct fb_part_dev {
	char *device;
	int dev_max;
	struct list_head list;		/* next devices */
	struct list_head part_list;	/* partition lists */
	unsigned int mask;
	struct fb_part_ops *ops;
};

const char *print_part_type(int fb_part_type);

#define FB_PARTMAP_BIND_WEAK(name) \
	void __weak fb_partmap_bind_##name(struct list_head *head) { }

#define FB_PARTMAP_BIND_INIT(name, pdev) \
	void fb_partmap_bind_##name(struct list_head *head) { \
		struct fb_part_dev *fd = pdev; \
		INIT_LIST_HEAD(&fd->list); \
		INIT_LIST_HEAD(&fd->part_list); \
		list_add_tail(&fd->list, head); \
	}

#define FB_PARTMAP_BIND(name, list) fb_partmap_bind_##name(list)

#endif
