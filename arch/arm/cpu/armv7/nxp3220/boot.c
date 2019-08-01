/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <asm/io.h>
#include <mach/cpu.h>

DECLARE_GLOBAL_DATA_PTR;

#define	ALIVE_BASE	(0x20080000)
#define	BOOT_OPTION	(ALIVE_BASE + 0xC86C)
#define VDDPWR_BASE	(0x20080000 + 0xc800)
#define SCRATCH_BASE(x) (VDDPWR_BASE + 0x100 + (4 * (x)))
#define SCRATCH_OFFSET	7
#define BL2_NSIH_DEV_BASE_ADDR	0x18000

struct boot_mode {
	char *mode;
	enum boot_device dev;
	unsigned int mask;
};

struct nsih_sbi_header {
	u32 RESV[0x098 / 4];	/* 0x010 ~ 0x094 */
	u8  R_cal[16];		/* 0x098 ~ 0x0A7 */
	u8  W_cal[16];		/* 0x0A8 ~ 0x0B7 */
};

static struct boot_mode boot_main_mode[] = {
	{ "EMMC", BOOT_DEV_EMMC, 0 },
	{ "USB" , BOOT_DEV_USB , BIT(0) },
	{ "SD"  , BOOT_DEV_SD  , BIT(0) | BIT(1) },
	{ "NAND", BOOT_DEV_NAND, BIT(2) },
	{ "SPI" , BOOT_DEV_SPI , BIT(0) | BIT(2) },
	{ "UART", BOOT_DEV_UART, BIT(1) | BIT(2) },
};

static struct boot_mode *current_bootmode;

int boot_check_mode(void)
{
	unsigned int mode;
	int i;

	if (current_bootmode)
		return 0;

	mode = readl(BOOT_OPTION) & 0x7;

	debug("boot mode value:0x%x\n", mode);

	for (i = 0; i < ARRAY_SIZE(boot_main_mode); i++) {
		if (boot_main_mode[i].mask == mode) {
#ifndef CONFIG_QUICKBOOT_QUIET
			printf("BOOT: %s: 0x%x\n",
			       boot_main_mode[i].mode, mode);
#endif
			current_bootmode = &boot_main_mode[i];
			break;
		}
	}

	return 0;
}

enum boot_device boot_current_device(void)
{
	if (!current_bootmode)
		boot_check_mode();

	return current_bootmode->dev;
}

static int memory_calibration(void)
{
#ifdef CONFIG_MMC
	struct blk_desc *dev_desc;
	struct nsih_sbi_header *psbi;
	struct boot_mode *bootmode = current_bootmode;
	lbaint_t start = BL2_NSIH_DEV_BASE_ADDR / 512, blkcnt = 1, blks;
	unsigned int mode = 0;
	void *buffer = NULL;
	u32 *pr, *pw;
	char cmd[128];
	int port = 0, ret, i;

	boot_check_mode();

	if (bootmode->dev != BOOT_DEV_EMMC && bootmode->dev != BOOT_DEV_SD)
		return 0;

	if (readl(SCRATCH_BASE(SCRATCH_OFFSET - 1)) != 0x4d656d43) {
#ifndef CONFIG_QUICKBOOT_QUIET
		printf("MEM: DONE calibration -> %s\n", bootmode->mode);
#endif
		return 0;
	}

	mode = (readl(BOOT_OPTION) >> 3) & 0xFF;

	if (bootmode->dev == BOOT_DEV_SD) {
		port = abs(((mode & 0xc) >> 2) - 2);

		sprintf(cmd, "mmc dev %d", port);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;

	} else if (bootmode->dev == BOOT_DEV_EMMC) {
		port = (mode & 0xc) >> 2;
		sprintf(cmd, "mmc bootbus %d %d %d %d",
			port, (mode & 0x2) ? 1 : 2, 0, 0);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;

		/* switch to boot partition */
		sprintf(cmd, "mmc partconf %d 0 1 1", port);
		ret = run_command(cmd, 0);
		if (ret < 0)
			goto exit;
	}

	debug("%s: %s.%d mode:0x%x\n",
	      __func__, bootmode->mode, port, mode);

	ret = blk_get_device_by_str("mmc", simple_itoa(port), &dev_desc);
	if (ret < 0)
		goto exit;

	buffer = calloc(1, 512);
	if (!buffer) {
		pr_err("%s: failed memory allocation 512bytes!!!\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}
	psbi = buffer;

	blks = blk_dread(dev_desc, start, blkcnt, buffer);
	if (blks != blkcnt) {
		pr_err("%s: failed read device\n", __func__);
		ret = -EIO;
		goto exit;
	}

	/* clear scratch signature */
	writel(0, SCRATCH_BASE(SCRATCH_OFFSET - 1));

	printf("MEM: rc [ ");
	for (pr = (u32 *)psbi->R_cal, i = 0; i < 4; i++) {
		pr[i] = readl(SCRATCH_BASE(i + SCRATCH_OFFSET));
		printf("%08x ", pr[i]);
	}
	printf("]\n");

	printf("MEM: wc [ ");
	for (pw = (u32 *)psbi->W_cal, i = 0; i < 4; i++) {
		pw[i] = readl(SCRATCH_BASE(i + 4 + SCRATCH_OFFSET));
		printf("%08x ", pw[i]);
	}
	printf("]\n");

	blks = blk_dwrite(dev_desc, start, blkcnt, buffer);
	if (blks != blkcnt)
		pr_err("%s: failed write device\n", __func__);

exit:
	if (ret < 0)
		pr_err("%s: Failed memory calibration:%d\n", __func__, ret);
	else
		printf("MEM: OK calibration -> %s.%d\n", bootmode->mode, port);

	if (buffer)
		free(buffer);

	if (bootmode->dev == BOOT_DEV_EMMC) {
		/* switch data partition */
		sprintf(cmd, "mmc partconf %d 0 1 0", port);
		run_command(cmd, 0);
	}

	return ret;
#else
	return 0;
#endif
}

#if defined(CONFIG_ARCH_MISC_INIT)
int arch_misc_init(void)
{
	int ret = memory_calibration();

	if (ret < 0)
		pr_err("%s: Failed memory calibration !!!\n", __func__);

	return 0;
}
#endif

