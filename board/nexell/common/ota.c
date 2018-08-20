#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <mapmem.h>
#include <errno.h>
#include <mmc.h>
#include <artik_ota.h>

static void write_flag_partition(u32 dev, char *buf)
{
	struct mmc *mmc;
	u32 n;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", 1);
		return;
	}

	n = blk_dwrite(mmc_get_blk_desc(mmc), FLAG_PART_BLOCK_START,
			FLAG_PART_BLOCK_SIZE, buf);
	if (n < FLAG_PART_BLOCK_SIZE)
		printf("Cannot write flag information\n");
}

static struct boot_info *read_flag_partition(u32 dev, char *buf)
{
	struct mmc *mmc;
	u32 n;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", 1);
		return NULL;
	}

	n = blk_dread(mmc_get_blk_desc(mmc), FLAG_PART_BLOCK_START,
			FLAG_PART_BLOCK_SIZE, buf);
	if (n < FLAG_PART_BLOCK_SIZE)
		printf("Cannot read flag informatin\n");

	return (struct boot_info *)buf;
}

static inline void update_partition_env(enum boot_part part_num)
{
	if (part_num == PART0) {
		env_set("mmc_boot_part", __stringify(MMC_BOOT_A_PART));
		env_set("mmc_modules_part", __stringify(MMC_MODULES_A_PART));
	} else {
		env_set("mmc_boot_part", __stringify(MMC_BOOT_B_PART));
		env_set("mmc_modules_part", __stringify(MMC_MODULES_B_PART));
	}
}

int check_ota_update(void)
{
	struct boot_info *boot;
	struct part_info *cur_part, *bak_part;
	char *bootdev;
	char flag[128 * 1024] = {0, };
	u32 boot_device;

	env_set("ota", "ota");
	/* Check booted device */
	bootdev = env_get("mmc_boot_dev");
	boot_device = simple_strtoul(bootdev, NULL, 10);

	/* Read flag information */
	boot = read_flag_partition(boot_device, flag);
	if ((boot != NULL) &&
			(strncmp(boot->header_magic, HEADER_MAGIC, 32) != 0)) {
		printf("Wrong FLAG information\n");
		return -1;
	}

	if (boot->part_num == PART0) {
		cur_part = &boot->part0;
		bak_part = &boot->part1;
	} else {
		cur_part = &boot->part1;
		bak_part = &boot->part0;
	}

	switch (boot->state) {
	case BOOT_SUCCESS:
		printf("Booting Partition: %s(Normal)\n",
				boot->part_num == PART0 ? "PART0" : "PART1");
		update_partition_env(boot->part_num);
		env_set("rescue", "0");
		break;
	case BOOT_UPDATED:
		if (cur_part->retry > 0) {
			printf("Booting Partition: %s (Updated)\n",
				boot->part_num == PART0 ? "PART0" : "PART1");
			cur_part->retry--;
			env_set("bootdelay", "0");
			env_set("rescue", "0");
		/* Handling Booting Fail */
		} else if (cur_part->retry == 0) {
			printf("Booting Partition: %s (Failed)\n",
				boot->part_num == PART0 ? "PART0" : "PART1");
			printf("OTA: Back to backup partiton\n");
			cur_part->state = BOOT_FAILED;
			if (bak_part->state == BOOT_SUCCESS) {
				if (boot->part_num == PART0)
					boot->part_num = PART1;
				else
					boot->part_num = PART0;
				boot->state = BOOT_SUCCESS;
				env_set("rescue", "1");
			} else {
				boot->state = BOOT_FAILED;
			}
		}

		update_partition_env(boot->part_num);
		write_flag_partition(boot_device, flag);
		break;
	case BOOT_FAILED:
	default:
		printf("Booting State = Abnormal\n");
		update_partition_env(PART0);
		return -1;
	}

	return 0;
}
