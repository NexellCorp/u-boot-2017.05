/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#ifndef _F_FASTBOOT_PARTMAP_H
#define _F_FASTBOOT_PARTMAP_H

enum fastboot_part_type {
	FASTBOOT_PART_BOOT = (1<<0),	/* bootable partition */
	FASTBOOT_PART_RAW  = (1<<1),	/* raw write */
	FASTBOOT_PART_PART = (1<<2),	/* partition write: ex) ext4 */
};

#define	FS_TYPE_PART_TABLE	(FASTBOOT_PART_PART)

/* each device max partition max num */
#define	FASTBOOT_DEV_PART_MAX		(16)

struct fb_part_par {
	char partition[32];	/* partition name: ex> boot, env, ... */
	int dev_no;
	uint64_t start;
	uint64_t length;
	enum fastboot_part_type type;
	struct fb_part_dev *fd;
	struct list_head link;
};

struct fb_part_ops {
	int (*write)(struct fb_part_par *f_part, void *buffer, uint64_t bytes);
	int (*capacity)(int dev, uint64_t *length);
	int (*create_part)(int dev, uint64_t (*parts)[2], int count);
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
