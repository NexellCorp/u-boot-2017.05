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

struct boot_mode {
	char *mode;
	enum boot_device dev;
	unsigned int mask;
};

#if defined(CONFIG_ARCH_NXP3220) || defined(CONFIG_ARCH_NXP3225)
#define	ALIVE_BASE	(0x20080000)
#define	BOOT_OPTION	(ALIVE_BASE + 0xC86C)

#define VDDPWR_BASE	(0x20080000 + 0xc800)
#define SCRATCH_BASE(x) (VDDPWR_BASE + 0x100 + (4 * (x)))
#define SCRATCH_OFFSET	7

struct nsih_sbi_header {
	u32 RESV[0x098 / 4];	/* 0x010 ~ 0x094 */
	u8  R_cal[16];		/* 0x098 ~ 0x0A7 */
	u8  W_cal[16];		/* 0x0A8 ~ 0x0B7 */
};

#define BL2_NSIH_DEV_BASE_ADDR		0x18000

static struct boot_mode boot_main_mode[] = {
	{ "EMMC", BOOT_DEV_EMMC, 0 },
	{ "USB" , BOOT_DEV_USB , BIT(0) },
	{ "SD"  , BOOT_DEV_SD  , BIT(0) | BIT(1) },
	{ "NAND", BOOT_DEV_NAND, BIT(2) },
	{ "SPI" , BOOT_DEV_SPI , BIT(0) | BIT(2) },
	{ "UART", BOOT_DEV_UART, BIT(1) | BIT(2) },
};

#else
#error "Not defined bootmode configs"
#endif

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

static int boot_ddr_cal_config(void)
{
#ifdef CONFIG_MMC
	struct blk_desc *dev_desc;
	struct nsih_sbi_header *psbi;
	struct boot_mode *bootmode = current_bootmode;
	lbaint_t start = BL2_NSIH_DEV_BASE_ADDR / 512, blkcnt = 1, blks;
	unsigned int mode = 0;
	void *buffer = NULL;
	u32 *p;
	char cmd[128];
	int port = 0, ret, i;

	boot_check_mode();

	if (bootmode->dev != BOOT_DEV_EMMC && bootmode->dev != BOOT_DEV_SD)
		return 0;

	if (readl(SCRATCH_BASE(SCRATCH_OFFSET - 1)) != 0x4d656d43) {
#ifndef CONFIG_QUICKBOOT_QUIET
		printf("MEM: DONE calibration\n");
#endif
		return 0;
	}

	mode = (readl(BOOT_OPTION) >> 3) & 0xFF;

	if (bootmode->dev == BOOT_DEV_SD) {
		port = abs(((mode & 0xc) >> 2) - 2);
	} else if (bootmode->dev == BOOT_DEV_EMMC) {
		port = (mode & 0xc) >> 2;
		sprintf(cmd, "mmc bootbus %d %d %d %d",
			port, (mode & 0x2) ? 1 : 2, 0, 0);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;

		/* R/W boot partition 1 */
		sprintf(cmd, "mmc partconf %d 0 1 1", port);
		ret = run_command(cmd, 0);
		if (ret < 0)
			goto exit;
	}

	debug("%s: %s.%d mode:0x%x\n",
	      __func__, bootmode->mode, port, mode);

	/* set mmc devicee */
	sprintf(cmd, "mmc dev %d", port);
	ret = blk_get_device_by_str("mmc", simple_itoa(port), &dev_desc);
	if (ret < 0) {
		ret = run_command(cmd, 0);
		if (ret < 0)
			goto exit;

		ret = run_command("mmc rescan", 0);
		if (ret < 0)
			goto exit;
	}

	buffer = calloc(1, 512);
	if (!buffer) {
		pr_err("%s: failed memory allocation 512bytes!!!\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	blks = blk_dread(dev_desc, start, blkcnt, buffer);
	if (blks != blkcnt) {
		pr_err("%s: failed read device\n", __func__);
		ret = -EIO;
		goto exit;
	}
	psbi = buffer;

	/* clear */
	writel(0, SCRATCH_BASE(SCRATCH_OFFSET - 1));

	debug("RC: ");
	for (p = (u32 *)psbi->R_cal, i = 0; i < 4; i++) {
		p[i] = readl(SCRATCH_BASE(i + SCRATCH_OFFSET));
		debug("%08x ", p[i]);
	}
	debug("\n");

	debug("WC: ");
	for (p = (u32 *)psbi->W_cal, i = 0; i < 4; i++) {
		p[i] = readl(SCRATCH_BASE(i + 4 + SCRATCH_OFFSET));
		debug("%08x ", p[i]);
	}
	debug("\n");

	blks = blk_dwrite(dev_desc, start, blkcnt, buffer);
	if (blks != blkcnt)
		pr_err("%s: failed write device\n", __func__);

exit:
	if (ret < 0)
		pr_err("%s: Failed memory calibration:%d\n", __func__, ret);
	else
#ifndef CONFIG_QUICKBOOT_QUIET
		printf("MEM: OK calibration\n");
#endif

	if (buffer)
		free(buffer);

	if (bootmode->dev == BOOT_DEV_EMMC) {
		/* No access to boot partition */
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
	int ret = boot_ddr_cal_config();

	if (ret)
		pr_err("%s: Failed drr config !!!\n", __func__);

	return 0;
}
#endif

