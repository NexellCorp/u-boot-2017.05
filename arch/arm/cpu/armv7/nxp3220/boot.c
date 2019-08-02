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
	char *name;
	enum boot_device dev;
	int port;
	unsigned int mask;
	u32 mode;
};

struct sbi_header {
	u32 RESV[0x098 / 4];	/* 0x010 ~ 0x094 */
	u8  R_cal[16];		/* 0x098 ~ 0x0A7 */
	u8  W_cal[16];		/* 0x0A8 ~ 0x0B7 */
};

static struct boot_mode boot_main_mode[] = {
	{ "EMMC", BOOT_DEV_EMMC, 0, 0 },
	{ "USB" , BOOT_DEV_USB , 0, BIT(0) },
	{ "SD"  , BOOT_DEV_SD  , 0, BIT(0) | BIT(1) },
	{ "NAND", BOOT_DEV_NAND, 0, BIT(2) },
	{ "SPI" , BOOT_DEV_SPI , 0, BIT(0) | BIT(2) },
	{ "UART", BOOT_DEV_UART, 0, BIT(1) | BIT(2) },
};

static struct boot_mode *current_bootmode;

int boot_check_mode(void)
{
	u32 mode, main, sub;
	struct boot_mode *bmode = NULL;
	int i;

	if (current_bootmode)
		return 0;

	mode = readl(BOOT_OPTION) & 0x7;
	main = mode & 0x7;
	sub = (mode >> 3) & 0xFF;

	debug("Boot mode option:0x%x\n", mode);

	for (i = 0; i < ARRAY_SIZE(boot_main_mode); i++) {
		if (boot_main_mode[i].mask == main) {
			bmode = &boot_main_mode[i];
			bmode->mode = mode;
			break;
		}
	}

	if (!bmode) {
		pr_err("%s: Unknown Boot mode option:0x%x\n", __func__, mode);
		return -ENODEV;
	}

	if (bmode->dev == BOOT_DEV_SD)
		bmode->port = abs(((sub & 0xc) >> 2) - 2);

	if (bmode->dev == BOOT_DEV_EMMC ||
	    bmode->dev == BOOT_DEV_SPI ||
	    bmode->dev == BOOT_DEV_UART)
		bmode->port = (sub & 0xc) >> 2;

#ifndef CONFIG_QUICKBOOT_QUIET
	printf("BOOT: %s.%d\n", bmode->name, bmode->port);
#endif

	current_bootmode = bmode;

	return 0;
}

enum boot_device boot_current_device(void)
{
	if (!current_bootmode)
		boot_check_mode();

	return current_bootmode->dev;
}

static int ddr_cal_bind(struct boot_mode *bmode)
{
	char cmd[128];
	int ret = -EINVAL;

	if (bmode->dev == BOOT_DEV_SD) {
		sprintf(cmd, "mmc dev %d", bmode->port);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;

	} else if (bmode->dev == BOOT_DEV_EMMC) {
		sprintf(cmd, "mmc bootbus %d %d %d %d",
			bmode->port, (bmode->mode & 0x2) ? 1 : 2, 0, 0);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;

		/* switch to boot partition */
		sprintf(cmd, "mmc partconf %d 0 1 1", bmode->port);
		ret = run_command(cmd, 0);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static void ddr_cal_unbind(struct boot_mode *bmode)
{
	char cmd[128];

	if (bmode->dev != BOOT_DEV_EMMC)
		return;

	/* switch data partition */
	sprintf(cmd, "mmc partconf %d 0 1 0", bmode->port);
	run_command(cmd, 0);
}

static int ddr_cal_update(const char *dev, int port, struct sbi_header *psh)
{
	struct blk_desc *dev_desc;
	struct sbi_header *pheader = NULL;
	lbaint_t start = BL2_NSIH_DEV_BASE_ADDR / 512, blkcnt = 1, blks;
	u32 *p;
	int i, ret;

	ret = blk_get_device_by_str(dev, simple_itoa(port), &dev_desc);
	if (ret < 0)
		goto exit;

	pheader = calloc(1, 512);
	if (!pheader) {
		pr_err("%s: failed memory allocation 512bytes!!!\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	blks = blk_dread(dev_desc, start, blkcnt, pheader);
	if (blks != blkcnt) {
		pr_err("%s: failed read device\n", __func__);
		ret = -EIO;
		goto exit;
	}

	memcpy(pheader->R_cal, psh->R_cal, sizeof(psh->R_cal));
	memcpy(pheader->W_cal, psh->W_cal, sizeof(psh->W_cal));

	printf("MEM: rc [ ");
	for (p = (u32 *)pheader->R_cal, i = 0; i < 4; i++)
		printf("%08x ", p[i]);
	printf("]\n");

	printf("MEM: wc [ ");
	for (p = (u32 *)pheader->W_cal, i = 0; i < 4; i++)
		printf("%08x ", p[i]);
	printf("]\n");

	blks = blk_dwrite(dev_desc, start, blkcnt, pheader);
	if (blks != blkcnt)
		pr_err("%s: failed write device\n", __func__);

exit:
	if (pheader)
		free(pheader);

	return 0;
}

static int ddr_cal_save(void)
{
#ifdef CONFIG_MMC
	struct sbi_header *psh;
	struct boot_mode *bmode = current_bootmode;
	void *env;
	u32 *p;
	int ret, i;

	boot_check_mode();

	env = env_get("ignore_cal");
	if (env) {
#ifndef CONFIG_QUICKBOOT_QUIET
		printf("MEM: ignore calibration: %s.%d, Env: 'ignore_cal'\n",
		       bmode->name, bmode->port);
#endif
		return 0;
	}

	if (bmode->dev != BOOT_DEV_EMMC && bmode->dev != BOOT_DEV_SD)
		return 0;

	if (readl(SCRATCH_BASE(SCRATCH_OFFSET - 1)) != 0x4d656d43) {
#ifndef CONFIG_QUICKBOOT_QUIET
		printf("MEM: DONE calibration: %s.%d\n",
		       bmode->name, bmode->port);
#endif
		return 0;
	}

	psh = calloc(1, 512);
	if (!psh) {
		pr_err("%s: failed memory allocation 512bytes!!!\n", __func__);
		return -ENOMEM;
	}

	ret = ddr_cal_bind(bmode);
	if (ret < 0)
		goto exit;

	debug("%s: %s.%d\n", __func__, bmode->name, bmode->port);

	for (p = (u32 *)psh->R_cal, i = 0; i < 4; i++)
		p[i] = readl(SCRATCH_BASE(i + SCRATCH_OFFSET));

	for (p = (u32 *)psh->W_cal, i = 0; i < 4; i++)
		p[i] = readl(SCRATCH_BASE(i + 4 + SCRATCH_OFFSET));

	/* update */
	ret = ddr_cal_update("mmc", bmode->port, psh);

	/* clear scratch signature */
	if (!ret)
		writel(0, SCRATCH_BASE(SCRATCH_OFFSET - 1));

exit:
	if (psh)
		free(psh);

	ddr_cal_unbind(bmode);

	if (ret < 0)
		pr_err("%s: Failed memory calibration:%d\n", __func__, ret);
	else
		printf("MEM: OK calibration: %s.%d\n",
		       bmode->name, bmode->port);

	return ret;
#else
	return 0;
#endif
}

static int ddr_cal_clear(void)
{
#ifdef CONFIG_MMC
	struct sbi_header *psh;
	struct boot_mode *bmode = current_bootmode;
	int ret;

	boot_check_mode();

	if (bmode->dev != BOOT_DEV_EMMC && bmode->dev != BOOT_DEV_SD)
		return 0;

	psh = calloc(1, 512);
	if (!psh) {
		pr_err("%s: failed memory allocation 512bytes!!!\n", __func__);
		return -ENOMEM;
	}

	ret = ddr_cal_bind(bmode);
	if (ret < 0)
		goto exit;

	debug("%s: %s.%d\n", __func__, bmode->name, bmode->port);

	memset(psh->R_cal, 0, sizeof(psh->R_cal));
	memset(psh->W_cal, 0, sizeof(psh->W_cal));

	ret = ddr_cal_update("mmc", bmode->port, psh);

exit:
	if (psh)
		free(psh);

	ddr_cal_unbind(bmode);

	return ret;
#else
	return 0;
#endif
}

static int do_cal(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;

	if (strcmp(argv[1], "clear") == 0) {
		ret = ddr_cal_clear();
		if (ret < 0)
			pr_err("%s: Failed memory calibration clear !!!\n",
			       __func__);

		return ret;
	}

	printf("Please see usage\n");
	return 1;
}

#if defined(CONFIG_ARCH_MISC_INIT)
int arch_misc_init(void)
{
	int ret = ddr_cal_save();

	if (ret < 0)
		pr_err("%s: Failed memory calibration !!!\n", __func__);

	return 0;
}
#endif

U_BOOT_CMD(
	cal, CONFIG_SYS_MAXARGS, 1, do_cal,
	"clear saved calibration value for DDR\n"
	"- set environment 'ignore_cal' to ignore ddr calibration save\n"
	"- Ex> setenv ignore_cal 1",
	"clear"
);

