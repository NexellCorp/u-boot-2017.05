/*
 * Copyright (c) 2016 Samsung Electronics
 * Author: Jaewon Kim <jaewon02.kim@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ARTIK_OTA_H
#define __ARTIK_OTA_H

#define HEADER_MAGIC		"ARTIK_OTA_V2"
#define VERSION_MAJOR		2
#define VERSION_MINOR		0

#define FLAG_OFFSET		(3712 * 1024)
#define CMD_LEN			1024
#define TAG_SIZE		128
#define NAME_SIZE		32
#define MB			(1024 * 1024)

#define FLAG_PART_BLOCK_START	0x1D00
#define FLAG_PART_BLOCK_SIZE	0x100
#define BLOCK_SIZE		512

enum part_active {
	PART_A	= 0,
	PART_B,
};

enum part_state {
	UNUSED = 0,
	FAIL,
	SUCCESS,
	UPDATE,
};

typedef struct part_info {
	enum part_state p_state;
	uint8_t retry;
	uint8_t part_n;
	char tag[TAG_SIZE];
	uint32_t checksum;
}PART;

typedef struct block_info {
	char block_name[NAME_SIZE];
	enum part_active active;
	enum part_state b_state;
	PART part_a;
	PART part_b;
}BLOCK;

struct blocks {
	BLOCK block_boot;	/* boot */
	BLOCK block_module;	/* modules */
	BLOCK block_res1;	/* firmware */
	BLOCK block_res2;	/* rootfs */
	BLOCK block_res3;	/* reserved */
	BLOCK block_res4;	/* reserved */
};

struct boot_header {
	char header_magic[32];
	uint8_t major;
	uint8_t minor;
	uint32_t checksum;
	struct blocks blocks;
};

int check_ota_update(void);
#endif
