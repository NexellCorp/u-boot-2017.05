#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <mapmem.h>
#include <errno.h>
#include <mmc.h>
#include <artik_ota.h>

static int write_flag_partition(u32 dev, struct boot_header *hdr)
{
	struct mmc *mmc;
	char buf[FLAG_PART_BLOCK_SIZE * BLOCK_SIZE] = {0,  };
	int part_num_bkp;
	int ret;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", 1);
		return -ENODEV;
	}
	mmc_init(mmc);

	part_num_bkp = mmc_get_blk_desc(mmc)->hwpart;
	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, 0, 1);
	if (ret) {
		printf("switch to partitions %d error\n", 1);
		return ret;
	}

	memcpy(buf, hdr, sizeof(struct boot_header));
	hdr->checksum =
		crc32(0, (unsigned char *)&hdr->blocks, sizeof(struct blocks));

	if (blk_dwrite(mmc_get_blk_desc(mmc), FLAG_PART_BLOCK_START,
			FLAG_PART_BLOCK_SIZE, hdr) < FLAG_PART_BLOCK_SIZE)
		printf("Cannot write flag information\n");

	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, 0, part_num_bkp);
	if (ret) {
		printf("switch to partitions %d error\n", part_num_bkp);
		return ret;
	}

	return 0;
}

static int read_flag_partition(u32 dev, struct boot_header *hdr)
{
	struct mmc *mmc;
	char buf[FLAG_PART_BLOCK_SIZE * BLOCK_SIZE] = {0,  };
	int part_num_bkp;
	int ret;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", 1);
		return -ENODEV;
	}
	mmc_init(mmc);

	part_num_bkp = mmc_get_blk_desc(mmc)->hwpart;
	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, 0, 1);
	if (ret) {
		printf("switch to partitions %d error\n", 1);
		return ret;
	}

	if (blk_dread(mmc_get_blk_desc(mmc), FLAG_PART_BLOCK_START,
			FLAG_PART_BLOCK_SIZE, buf) < FLAG_PART_BLOCK_SIZE)
		printf("Cannot read flag informatin\n");

	ret = blk_select_hwpart_devnum(IF_TYPE_MMC, 0, part_num_bkp);
	if (ret) {
		printf("switch to partitions %d error\n", part_num_bkp);
		return ret;
	}

	memcpy(hdr, buf, sizeof(struct boot_header));

	return 0;
}

static inline void update_partition_env(enum part_active active)
{
	if (active == PART_A) {
		env_set("mmc_boot_part", __stringify(MMC_BOOT_A_PART));
		env_set("mmc_modules_part", __stringify(MMC_MODULES_A_PART));
	} else {
		env_set("mmc_boot_part", __stringify(MMC_BOOT_B_PART));
		env_set("mmc_modules_part", __stringify(MMC_MODULES_B_PART));
	}
}

int check_ota_update(void)
{
	struct boot_header hdr;
	BLOCK *block;
	PART *part;
	char *bootdev;
	u32 boot_device;

	/* Check boot device (Support only eMMC boot)*/
	bootdev = env_get("mmc_boot_dev");
	boot_device = simple_strtoul(bootdev, NULL, 10);
	if (boot_device != 0)
		return 0;

	env_set("ota", "ota");
	/* Read & check flag information */
	if (read_flag_partition(boot_device, &hdr) < 0)
		return -1;

	if (strncmp(hdr.header_magic, HEADER_MAGIC, 32) != 0) {
		printf("Broken FLAG information\n");
		return -EINVAL;
	}

	if (hdr.checksum != crc32(0, (unsigned char *)&hdr.blocks,
				sizeof(struct blocks))) {
		printf("Tainted FLAG information\n");
		return -EINVAL;
	}

	/* Checks status */
	block = &hdr.blocks.block_boot;

	switch (block->b_state) {
	case SUCCESS:
		printf("OTA: Booting Partition: %s (Normal)\n",
			block->active == PART_A ? "PART_A" : "PART_B");
		env_set("rescue", "0");
		update_partition_env(block->active);
		break;
	case UPDATE:
		(block->active == PART_A) ?
			(part = &block->part_a) : (part = &block->part_b);
		if (part->retry > 0) { /* Success */
			printf("OTA: Booting Partition: %s (Updated)\n",
				block->active == PART_A ? "PART_A" : "PART_B");
			part->retry--;
			env_set("bootdelay", "0");
			env_set("rescue", "0");
			update_partition_env(block->active);
		} else if (part->retry == 0) { /* Fail */
			printf("OTA: Booting Partition: %s (Recovery)\n",
				block->active == PART_A ? "PART_B" : "PART_A");
			printf("OTA: Back to backup partiton\n");
			part->p_state = FAIL;
			env_set("rescue", "1");
			if (block->active == PART_A)
				update_partition_env(PART_B);
			else
				update_partition_env(PART_A);
		}

		write_flag_partition(boot_device, &hdr);
		break;
	case FAIL:
	default:
		printf("Booting Partition: %s (Failed)\n",
			block->active == PART_A ? "PART_A" : "PART_B");
		env_set("rescue", "1");
		update_partition_env(block->active);
		return -1;
	}

	return 0;
}
