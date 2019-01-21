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
	FASTBOOT_PART_RAW = (1 << 1),	/* raw write */
	FASTBOOT_PART_FS = (1 << 2),	/* set partition table: ex) ext4,fat,*/
};

#define	PART_TYPE_PARTITION	(FASTBOOT_PART_FS)
#define	FASTBOOT_PART_GPT	(FASTBOOT_PART_FS | (1 << 4))
#define	FASTBOOT_PART_MBR	(FASTBOOT_PART_FS | (1 << 5))

/* each device max partition max num */
#define	FASTBOOT_DEV_PART_MAX		(16)

struct fb_part_par {
	char name[32];	/* partition name: ex> boot, env, ... */
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
	unsigned int part_support;
	struct fb_part_ops *ops;
};

#endif
