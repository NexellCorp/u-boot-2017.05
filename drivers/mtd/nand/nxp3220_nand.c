/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <dm.h>
#include <clk.h>
#include <errno.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>

#include "nxp3220_nand.h"

DECLARE_GLOBAL_DATA_PTR;

/*
 * nand interface
 */

static void nx_nandc_set_irq_enable(void __iomem *regs, int nr_int, int enable)
{
	const u32 IRQRDYEN_POS    = 0;
	const u32 IRQRDYEN_MASK   = (1UL << IRQRDYEN_POS);

	const u32 IRQDMAEN_POS    = 4;
	const u32 IRQDMAEN_MASK   = (1UL << IRQDMAEN_POS);

	u32 val;

	val = readl(regs + NFC_STATUS);
	if (nr_int == NX_NANDC_INT_RDY) {
		val &= ~IRQRDYEN_MASK;
		val |= (u32)(enable << IRQRDYEN_POS);
	} else {
		val &= ~IRQDMAEN_MASK;
		val |= (u32)(enable << IRQDMAEN_POS);
	}

	writel(val, regs + NFC_STATUS);
	dmb();
}

static void nx_nandc_set_irq_mask(void __iomem *regs, int nr_int, int enable)
{
	const u32 IRQRDYMS_POS    = 1;
	const u32 IRQRDYMS_MASK   = (1UL << IRQRDYMS_POS);

	const u32 IRQDMAMS_POS    = 5;
	const u32 IRQDMAMS_MASK   = (1UL << IRQDMAMS_POS);

	u32 val;
	int unmasked = !enable; /* 1: unmasked, 0: masked */

	val = readl(regs + NFC_STATUS);
	if (nr_int == 0) {
		val &= ~IRQRDYMS_MASK;
		val |= (u32)(unmasked << IRQRDYMS_POS);
	} else {
		val &= ~IRQDMAMS_MASK;
		val |= (u32)(unmasked << IRQDMAMS_POS);
	}

	writel(val, regs + NFC_STATUS);
	dmb();
}

static int nx_nandc_get_irq_pending(void __iomem *regs, int nr_int)
{
	const u32 IRQRDY_POS    = 2;
	const u32 IRQDMA_POS    = 6;
	const u32 IRQRDY_MASK   = (1UL << IRQRDY_POS);
	const u32 IRQDMA_MASK   = (1UL << IRQDMA_POS);

	u32 IRQPEND_POS, IRQPEND_MASK;

	if (nr_int == NX_NANDC_INT_RDY) {
		IRQPEND_POS  = IRQRDY_POS;
		IRQPEND_MASK = IRQRDY_MASK;
	} else {
		IRQPEND_POS  = IRQDMA_POS;
		IRQPEND_MASK = IRQDMA_MASK;
	}

	return  (int)((readl(regs + NFC_STATUS) & IRQPEND_MASK) >> IRQPEND_POS);
}

static void nx_nandc_clear_irq_pending(void __iomem *regs, int nr_int)
{
	const u32 IRQRDY_POS    = 2;
	const u32 IRQDMA_POS    = 6;
	const u32 IRQRDY_MASK   = (1UL << IRQRDY_POS);
	const u32 IRQDMA_MASK   = (1UL << IRQDMA_POS);

	u32 IRQPEND_POS, IRQPEND_MASK;

	u32 val;

	if (nr_int == NX_NANDC_INT_RDY) {
		IRQPEND_POS  = IRQRDY_POS;
		IRQPEND_MASK = IRQRDY_MASK;
	} else {
		IRQPEND_POS  = IRQDMA_POS;
		IRQPEND_MASK = IRQDMA_MASK;
	}

	val = readl(regs + NFC_STATUS);
	val &= ~IRQPEND_MASK;
	val |= (0x1 << IRQPEND_POS);
	writel(val, regs + NFC_STATUS);
	dmb();
}

static void nx_nandc_set_cmd_timing(void __iomem *regs,
				    int rhw, int setup, int width, int hold)
{
	u32 val;

	val = (rhw << 24 | setup << 16 | width << 8 | hold);
	writel(val, regs + NFC_CMD_TIME);
	dmb();
}

static void nx_nandc_set_data_timing(void __iomem *regs,
				     int whr, int setup, int width, int hold)
{
	u32 val;

	val = (whr << 24 | setup << 16 | width << 8 | hold);
	writel(val, regs + NFC_DATA_TIME);
	dmb();
}

static void nx_nandc_set_ddr_timing(void __iomem *regs,
				    int tcad, int tlast, int rd_dly,
				    int dq_dly, int clk_period)
{
	u32 val;

	val = (tcad << 28 | tlast << 24 | rd_dly << 16 | dq_dly << 8 |
			clk_period);
	writel(val, regs + NFC_DDR_TIME);
	dmb();
}

static void nx_nandc_set_cs_enable(void __iomem *regs, u32 chipsel,
				   int enable)
{
	const u32 BIT_SIZE  = 3;
	const u32 BIT_POS   = 4;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_CTRL);
	val &= ~(BIT_MASK | 0x1);
	val |= (chipsel << BIT_POS) | enable;
	writel(val, regs + NFC_CTRL);
	dmb();
}

static u32 nx_nandc_get_ready(void __iomem *regs)
{
	const u32 BIT_SIZE  = 1;
	const u32 BIT_POS   = 28;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	return readl(regs + NFC_STATUS) & BIT_MASK;
}

static void nx_nandc_set_bchmode(void __iomem *regs, u32 bchmode)
{
	const u32 BIT_SIZE  = 4;
	const u32 BIT_POS   = 0;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_BCH_MODE);
	val &= ~BIT_MASK;
	val |= (bchmode << BIT_POS);
	writel(val, regs + NFC_BCH_MODE);
	dmb();
}

static void nx_nandc_set_encmode(void __iomem *regs, int enable)
{
	const u32 BIT_SIZE  = 1;
	const u32 BIT_POS   = 4;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_BCH_MODE);
	val &= ~BIT_MASK;
	val |= (u32)enable << BIT_POS;
	writel(val, regs + NFC_BCH_MODE);
	dmb();
}

static void nx_nandc_set_ddrmode(void __iomem *regs, int enable)
{
	const u32 BIT_POS  = 0;
	const u32 BIT_MASK = (1UL << BIT_POS);
	u32 val;

	val = readl(regs + NFC_DDR_CTRL);
	val &= ~BIT_MASK;
	val |= (enable << BIT_POS) & BIT_MASK;
	writel(val, regs + NFC_DDR_CTRL);
	dmb();
}

static void nx_nandc_set_ddrclock_enable(void __iomem *regs, int enable)
{
	const u32 BIT_POS  = 1;
	const u32 BIT_MASK = (1UL << BIT_POS);
	u32 val;

	val = readl(regs + NFC_DDR_CTRL);
	val &= ~BIT_MASK;
	val |= (enable << BIT_POS) & BIT_MASK;
	writel(val, regs + NFC_DDR_CTRL);
	dmb();
}

static void nx_nandc_set_dmamode(void __iomem *regs, u32 mode)
{
	const u32 BIT_POS  = 0;
	const u32 BIT_MASK = (1UL << BIT_POS);
	u32 val;

	val = readl(regs + NFC_DMA_CTRL);
	val &= ~BIT_MASK;
	val |= (mode << BIT_POS) & BIT_MASK;
	writel(val, regs + NFC_DMA_CTRL);
	dmb();
}

static void nx_nandc_set_eccsize(void __iomem *regs, int eccsize)
{
	const u32 BIT_SIZE  = 8;
	const u32 BIT_POS   = 16;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_DMA_SIZE);
	val &= ~BIT_MASK;
	val |= (u32)eccsize << BIT_POS;
	writel(val, regs + NFC_DMA_SIZE);
	dmb();
}

static void nx_nandc_set_dmasize(void __iomem *regs, int dmasize)
{
	const u32 BIT_SIZE  = 16;
	const u32 BIT_POS   = 0;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_DMA_SIZE);
	val &= ~BIT_MASK;
	val |= dmasize << BIT_POS;
	writel(val, regs + NFC_DMA_SIZE);
	dmb();
}

static void nx_nandc_sel_subpage(void __iomem *regs, int sel)
{
	const u32 BIT_POS   = 0;
	u32 val;

	val = (u32)sel << BIT_POS;
	writel(val, regs + NFC_SUBPAGE);
	dmb();
}

static int nx_nandc_get_errinfo(void __iomem *regs)
{
	const u32 BIT_SIZE  = 14;
	const u32 BIT_POS   = 0;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	return (readl(regs + NFC_ERR_INFO) & BIT_MASK) >> BIT_POS;
}

static void nx_nandc_set_subpage(void __iomem *regs, int subpage)
{
	const u32 BIT_SIZE  = 4;
	const u32 BIT_POS   = 16;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_DMA_SUBP);
	val &= ~BIT_MASK;
	val |= (u32)subpage << BIT_POS;
	writel(val, regs + NFC_DMA_SUBP);
	dmb();
}

static void nx_nandc_set_subpage_size(void __iomem *regs, int subsize)
{
	const u32 BIT_SIZE  = 11;
	const u32 BIT_POS   = 0;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;

	u32 val;

	val = readl(regs + NFC_DMA_SUBP);
	val &= ~BIT_MASK;
	val |= subsize << BIT_POS;
	writel(val, regs + NFC_DMA_SUBP);
	dmb();
}

static void nx_nandc_set_dma_base(void __iomem *regs, u32 base)
{
	writel(base, regs + NFC_DMA_ADDR);
	dmb();
}

static void nx_nandc_set_sramsleep(void __iomem *regs, int enable)
{
	const u32 BIT_SIZE  = 1;
	const u32 BIT_POS   = 16;
	const u32 BIT_MASK  = ((1 << BIT_SIZE) - 1) << BIT_POS;
	u32 val;

	val = readl(regs + NFC_CTRL);
	val &= ~BIT_MASK;
	val |= (!enable << BIT_POS);
	writel(val, regs + NFC_CTRL);
	dmb();
}

static int nx_nandc_run_dma(struct nxp3220_nfc *nfc)
{
	void __iomem *regs = nfc->regs;

	/* clear DMA interrupt pending */
	nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_DMA);
	/* dma interrupt mask enable */
	nx_nandc_set_irq_mask(regs, NX_NANDC_INT_DMA, 1);
	/* ready interrupt enable */
	nx_nandc_set_irq_enable(regs, NX_NANDC_INT_DMA, 1);

	/* DMA run */
	nx_nandc_set_dmamode(regs, NX_NANDC_DMA_MODE);

	while (!nx_nandc_get_irq_pending(regs, NX_NANDC_INT_DMA))
		;
	/* clear interrupt pending */
	nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_DMA);

	nx_nandc_set_dmamode(regs, NX_NANDC_CPU_MODE);

	return 0;
}

/*
 * interface functions
 */
static void nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;

	pr_debug("%s, chipnr=%d\n", __func__, chipnr);

	if (chipnr > 4) {
		pr_err("not support nand chip index %d\n", chipnr);
		return;
	}

	if (chipnr == -1)
		nx_nandc_set_cs_enable(regs, 7, 1);
	else
		nx_nandc_set_cs_enable(regs, chipnr, 1);
}

static void ioread8_rep(void *addr, u8 *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		buf[i] = readb(addr);
}

static u16 nand_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	u16 tmp;

	ioread8_rep(chip->IO_ADDR_R, (u_char *)&tmp, sizeof(tmp));

	return tmp;
}

static void nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CTRL_CHANGE) {
		nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_RDY);
		nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_DMA);
	}

	if (ctrl & NAND_CLE) {
		dev_dbg(nfc->dev, " command: %02x\n", (unsigned char)cmd);
		writeb(cmd, regs + NFC_CMD);
	} else if (ctrl & NAND_ALE) {
		dev_dbg(nfc->dev, " address: %02x\n", (unsigned char)cmd);
		writeb(cmd, regs + NFC_ADDR);
	}
}

static int nand_dev_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;
	int ret = 0;

	ret = nx_nandc_get_ready(regs);
	pr_debug(" nfc RnB %s\n", ret ? "READY" : "BUSY");

	return ret;
}

/* convert to nano-seconds to nfc clock cycles */
#define ns2cycle(ns, clk)	(int)(((ns) * (clk / 1000000)) / 1000)
#define ns2cycle_r(ns, clk)	(int)(((ns) * (clk / 1000000) + 999) / 1000)

/* sdr timing calculation */
static void nxp3220_calc_sdr_timings(struct nxp3220_nfc *nfc,
				     const struct nand_sdr_timings *t)
{
	u32 tCLH = DIV_ROUND_UP(t->tCLH_min, 1000);
	u32 tCH = DIV_ROUND_UP(t->tCH_min, 1000);
	u32 tWP = DIV_ROUND_UP(t->tWP_min, 1000);
	u32 tWH = DIV_ROUND_UP(t->tWH_min, 1000);
	u32 tCLS = DIV_ROUND_UP(t->tCLS_min, 1000);
	u32 tWC = DIV_ROUND_UP(t->tWC_min, 1000);
	u32 tALS = DIV_ROUND_UP(t->tALS_min, 1000);
	u32 tRHW = DIV_ROUND_UP(t->tRHW_min, 1000);
	u32 tWHR = DIV_ROUND_UP(t->tWHR_min, 1000);

	u32 tDH = DIV_ROUND_UP(t->tDH_min, 1000);
	u32 tRC = DIV_ROUND_UP(t->tRC_min, 1000);
	u32 tRP = DIV_ROUND_UP(t->tRP_min, 1000);
	u32 tREH = DIV_ROUND_UP(t->tREH_min, 1000);
	u32 tREA = DIV_ROUND_UP(t->tREA_max, 1000);
	u32 clk_period;

	unsigned long clk_hz;
	int cmd_setup, cmd_width, cmd_hold, rhw;
	int write_setup, write_width, write_hold, whr;
	int read_setup, read_width, read_hold;

	clk_hz = clk_get_rate(&nfc->core_clk);
	rhw = ns2cycle_r(tRHW, clk_hz);
	whr = ns2cycle_r(tWHR, clk_hz);

	clk_period = (u32)(1000000000UL / clk_hz);

	/* command write */
	cmd_setup = (tCLS-tWP) > tCLH ?
		DIV_ROUND_UP((tCLS-tWP),clk_period) - 1 :
		DIV_ROUND_UP(tCLH,clk_period) -1;
	cmd_width = DIV_ROUND_UP(tWP,clk_period) - 1;
	cmd_hold = tCLH > tDH ? 0 : DIV_ROUND_UP(tDH,clk_period) - 1;

	/* data write */
	write_setup = 0;
	write_width = DIV_ROUND_UP(tWP,clk_period) - 1;
	write_hold = (tWC-tWP) > tWH ?
		(((tWC-tWP) > tDH) ? DIV_ROUND_UP((tWC-tWP),clk_period) - 1 :
			DIV_ROUND_UP((tWC-tWP),clk_period)) - 1:
		((tWH > tDH) ? DIV_ROUND_UP(tWH,clk_period) - 1 :
			DIV_ROUND_UP(tDH,clk_period)) - 1;

	/* data read */
	read_setup = 0;
	read_width = (tRP > (tREA+clk_period)) ?
		DIV_ROUND_UP(tRP,clk_period) - 1 :
		DIV_ROUND_UP((tREA+clk_period),clk_period) - 1;
	read_hold = (tRC-tRP) > tREH ?
		DIV_ROUND_UP((tRC-tRP),clk_period) - 1 :
		DIV_ROUND_UP(tREH,clk_period) - 1;

	nfc->time.cmd_setup = cmd_setup;
	nfc->time.cmd_width = cmd_width;
	nfc->time.cmd_hold = cmd_hold;
	nfc->time.rhw = rhw;

	nfc->time.rd_setup = read_setup;
	nfc->time.rd_width = read_width;
	nfc->time.rd_hold = read_hold;

	nfc->time.wr_setup = write_setup;
	nfc->time.wr_width = write_width;
	nfc->time.wr_hold = write_hold;
	nfc->time.whr = whr;

	pr_debug(" clk_hz %ld cmd setup:%d width:%d hold:%d, rhw:%d\n",
		 clk_hz, cmd_setup, cmd_width, cmd_hold, rhw);
	pr_debug(" read setup:%d width:%d hold:%d, whr:%d\n",
		 read_setup, read_width, read_hold, whr);
	pr_debug(" write setup:%d width:%d hold:%d, whr:%d\n",
		 write_setup, write_width, write_hold, whr);
}

/* sdr timing settings */
static void nxp3220_set_sdr_timings(struct nxp3220_nfc *nfc,
				    int cmd, int read, int write)
{
	void __iomem *regs = nfc->regs;

	if (cmd) {
		u32 rhw = nfc->time.rhw;
		u32 setup = nfc->time.cmd_setup;
		u32 width = nfc->time.cmd_width;
		u32 hold = nfc->time.cmd_hold;

		nx_nandc_set_cmd_timing(regs, rhw, setup, width, hold);
	}

	if (read || write) {
		u32 whr = nfc->time.whr;
		u32 setup = (read) ? nfc->time.rd_setup : nfc->time.wr_setup;
		u32 width = (read) ? nfc->time.rd_width : nfc->time.wr_width;
		u32 hold = (read) ? nfc->time.rd_hold : nfc->time.wr_hold;

		nx_nandc_set_data_timing(regs, whr, setup, width, hold);
	}
}

static int nand_hw_init_timings(struct nand_chip *chip)
{
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	const struct nand_sdr_timings *timings;
	int mode;

	mode = onfi_get_async_timing_mode(chip);
	if (mode == ONFI_TIMING_MODE_UNKNOWN)
		mode = chip->onfi_timing_mode_default;
	else
		mode = fls(mode) - 1;
	if (mode < 0)
		mode = 0;

	timings = onfi_async_timing_mode_to_sdr_timings(mode);
	if (IS_ERR(timings))
		return PTR_ERR(timings);

	nxp3220_calc_sdr_timings(nfc, timings);
	nxp3220_set_sdr_timings(nfc, 1, 1, 0);

	return 0;
}

static void nand_dev_init(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;

	nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_RDY);
	nx_nandc_clear_irq_pending(regs, NX_NANDC_INT_DMA);

	/* sram sleep awake */
	nx_nandc_set_sramsleep(regs, 0);
	nx_nandc_set_cs_enable(regs, 0x7, 1);
	/* set ddr mode off */
	nx_nandc_set_ddrmode(regs, 0);
	/* ddr clock disable */
	nx_nandc_set_ddrclock_enable(regs, 0);
	/* ready interrupt enable */
	nx_nandc_set_irq_enable(regs, NX_NANDC_INT_RDY, 1);
	/* dma interrupt enable */
	nx_nandc_set_irq_enable(regs, NX_NANDC_INT_DMA, 1);
	/* ready interrupt mask enable */
	nx_nandc_set_irq_mask(regs, NX_NANDC_INT_RDY, 1);
	/* dma interrupt mask disable */
	nx_nandc_set_irq_mask(regs, NX_NANDC_INT_DMA, 0);

	nx_nandc_set_cmd_timing(regs, 45, 6, 20, 8);
	nx_nandc_set_data_timing(regs, 45, 5, 20, 8);
	nx_nandc_set_ddr_timing(regs, 3, 4, 0, 0, 3);

	nx_nandc_set_dmamode(regs, NX_NANDC_CPU_MODE);
}

/*
 * u-boot nand hw ecc interface
 */

/* ECC related defines */
static struct nand_ecclayout nand_ecc_oob = {
	.eccbytes =   0,
	.eccpos   = { 0, },
	.oobfree  = { {.offset = 0, .length = 0} }
};

/**
 * nand_hw_ecc_read_oob - OOB data read function for HW ECC
 *			    with syndromes
 * @mtd: mtd info structure
 * @chip: nand chip info structure
 * @page: page number to read
 */
static int nand_hw_ecc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	int length = mtd->oobsize;
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size;
	u8 *bufpoi = chip->oob_poi;
	int i, toread, sndrnd = 0, pos;
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);

	/* setup read timing */
	nxp3220_set_sdr_timings(nfc, 0, 1, 0);

	chip->cmdfunc(mtd, NAND_CMD_READ0, chip->ecc.size, page);
	for (i = 0; i < chip->ecc.steps; i++) {
		if (sndrnd) {
			pos = eccsize + i * (eccsize + chunk);
			if (mtd->writesize > 512)
				chip->cmdfunc(mtd, NAND_CMD_RNDOUT, pos, -1);
			else
				chip->cmdfunc(mtd, NAND_CMD_READ0, pos, page);
		} else {
			sndrnd = 1;
		}
		toread = min_t(int, length, chunk);
		chip->read_buf(mtd, bufpoi, toread);
		bufpoi += toread;
		length -= toread;
	}
	if (length > 0)
		chip->read_buf(mtd, bufpoi, length);

	return 0;
}

/**
 * nand_hw_ecc_write_oob - OOB data write function for HW ECC with syndrome
 * @mtd: mtd info structure
 * @chip: nand chip info structure
 * @page: page number to write
 */
static int nand_hw_ecc_write_oob(struct mtd_info *mtd,
				 struct nand_chip *chip, int page)
{
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size, length = mtd->oobsize;
	int i, len, pos, status = 0, sndcmd = 0, steps = chip->ecc.steps;
	const u8 *bufpoi = chip->oob_poi;
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);

	/* setup write timing */
	nxp3220_set_sdr_timings(nfc, 0, 0, 1);

	/*
	 * data-ecc-data-ecc ... ecc-oob
	 * or
	 * data-pad-ecc-pad-data-pad .... ecc-pad-oob
	 */
	pos = eccsize;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, pos, page);
	for (i = 0; i < steps; i++) {
		if (sndcmd) {
			if (mtd->writesize <= 512) {
				u32 fill = 0xFFFFFFFF;

				len = eccsize;
				while (len > 0) {
					int num = min_t(int, len, 4);

					chip->write_buf(mtd, (u8 *)&fill,
							num);
					len -= num;
				}
			} else {
				pos = eccsize + i * (eccsize + chunk);
				chip->cmdfunc(mtd, NAND_CMD_RNDIN, pos, -1);
			}
		} else {
			sndcmd = 1;
		}
		len = min_t(int, length, chunk);
		chip->write_buf(mtd, bufpoi, len);
		bufpoi += len;
		length -= len;
	}

	if (length > 0)
		chip->write_buf(mtd, bufpoi, length);

	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int nand_hw_ecc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
				 u8 *buf, int oob_required, int page)
{
	int ret = 0;

	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;

	u32 dmabase = (u32)nfc->databuf;
	int sectsize = nfc->sectsize;

	int eccsteps = chip->ecc.steps;
	int eccbytes = chip->ecc.bytes;
	int eccsize = chip->ecc.size;

	u8 *p = nfc->databuf;
	int spare, subp;
	int uncorr = 0;

	/* ecc setting */
	nx_nandc_set_bchmode(regs, nfc->bchmode);
	nx_nandc_set_encmode(regs, 0);

	/* setup DMA */
	nx_nandc_set_dma_base(regs, dmabase);
	nx_nandc_set_eccsize(regs, eccbytes - 1);
	nx_nandc_set_dmasize(regs, sectsize * eccsteps - 1);
	nx_nandc_set_subpage(regs, eccsteps - 1);
	nx_nandc_set_subpage_size(regs, sectsize - 1);

	/* setup read timing */
	nxp3220_set_sdr_timings(nfc, 0, 1, 0);

	ret = nx_nandc_run_dma(nfc);
	if (ret < 0) {
		ret = -EIO;
		goto out;
	}

	spare = (eccsize * 2 - (eccsize + eccbytes)) * 8; /* data + meta */
	/* correction */
	for (subp = 0; subp < eccsteps; subp++, p += sectsize) {
		int errcnt;

		nx_nandc_sel_subpage(regs, subp);
		errcnt = nx_nandc_get_errinfo(regs);

		if (errcnt == 0x3f) {
			pr_debug("page %d uncorrectable. check erased\n", page);
			uncorr = 1;
			break;
		} else if (errcnt) {
			int i;

			for (i = 0; i < errcnt; i++) {
				void __iomem *r = regs + NFC_SRAM +
					(i * sizeof(u32));
				int elp = readl(r) & 0x3fff;

				elp -= spare;

				/* correct data range only */
				if (eccsize * 8 <= elp)
					continue;

				/* correct bit */
				p[elp >> 3] ^= 1 << (7 - (elp & 0x7));
			}
		}

		pr_debug("read step=%d/%d page=%d, page size=%d\n",
			 subp + 1, eccsteps, page, mtd->writesize);
	}

	p = nfc->databuf;
	if (uncorr) {
		/* data-ecc-data-ecc-...-ecc-oob */
		int stat = 0;
		unsigned int max_bitflips = 0;
		int prepad = chip->ecc.prepad;

		for (subp = 0; subp < eccsteps; subp++, p += sectsize) {
			stat = nand_check_erased_ecc_chunk(p, eccsize,
					p + prepad + eccsize, eccbytes,
					NULL, 0,
					chip->ecc.strength);
			if (stat < 0) {
				pr_err("ecc uncorrectable - step %d of %d\n",
						(subp + 1), eccsteps);
				mtd->ecc_stats.failed++;
				break;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips =
					max_t(unsigned int, max_bitflips, stat);
				ret = max_bitflips;

				/* bitflip cleaning */
				memset(buf, 0xff, mtd->writesize);
			}
		}
	} else {
		for (subp = 0; subp < eccsteps; subp++) {
			memcpy(buf, p, eccsize);
			p += sectsize;
			buf += eccsize;
		}
	}

	pr_debug("hw ecc read page return %d\n", ret);
out:
	return ret;
}

static int nand_hw_ecc_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				  const u8 *buf, int oob_required, int page)
{
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);
	void __iomem *regs = nfc->regs;
	int ret;

	u32 dmabase;
	int sectsize = nfc->sectsize;

	int eccsteps = chip->ecc.steps;
	int eccbytes = chip->ecc.bytes;
	int eccsize = chip->ecc.size;

	u8 *p = nfc->databuf;
	int subp;

	memset(p, 0xff, nfc->databuf_size);
	dmabase = (u32)p;

	for (subp = 0; subp < eccsteps; subp++) {
		memcpy(p, buf, eccsize);
		p += sectsize;
		buf += eccsize;
	}

	/* ecc setting */
	nx_nandc_set_bchmode(regs, nfc->bchmode);
	nx_nandc_set_encmode(regs, 1);

	/* setup DMA */
	nx_nandc_set_dma_base(regs, dmabase);
	nx_nandc_set_eccsize(regs, eccbytes - 1);
	nx_nandc_set_dmasize(regs, sectsize * eccsteps - 1);
	nx_nandc_set_subpage(regs, eccsteps - 1);
	nx_nandc_set_subpage_size(regs, sectsize - 1);

	/* setup write timing */
	nxp3220_set_sdr_timings(nfc, 0, 0, 1);

	ret = nx_nandc_run_dma(nfc);
	if (ret < 0)
		ret = -EIO;

	return ret;
}

static int verify_config(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	int ecctotal = chip->ecc.total;
	int oobsize = mtd->oobsize;

	if (ecctotal > oobsize) {
		pr_err("==================================================\n");
		pr_err("error: %d bit hw ecc mode requires ecc %d byte\n",
		       chip->ecc.strength, ecctotal);
		pr_err("       it's over the oob %d byte for page %d byte\n",
		       oobsize, mtd->writesize);
		pr_err("==================================================\n");

		return -EINVAL;
	}

	return 0;
}

static int nand_ecc_layout_hwecc(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nand_ecclayout *layout = chip->ecc.layout;
	struct nand_oobfree *oobfree = chip->ecc.layout->oobfree;
	u32 *eccpos = chip->ecc.layout->eccpos;
	int ecctotal = chip->ecc.total;
	int oobsize = mtd->oobsize;
	int i = 0, n = 0;
	int ret = 0;
	int eccbits = chip->ecc.strength;

	if (mtd->writesize < 512) {
		pr_err("NAND ecc: page size %d not support hw ecc\n",
		       mtd->writesize);
		chip->ecc.mode = NAND_ECC_SOFT;
		chip->ecc.read_page = NULL;
		chip->ecc.read_subpage = NULL;
		chip->ecc.write_page = NULL;
		chip->ecc.layout = NULL;

		if (chip->buffers && !(chip->options & NAND_OWN_BUFFERS)) {
			kfree(chip->buffers);
			chip->buffers = NULL;
		}
		ret = nand_scan_tail(mtd);
		pr_info("NAND ecc: Software\n");
		return ret;
	}

	if (mtd->oobsize <= 16) {
		for (i = 0, n = 0; ecctotal > i; i++, n++) {
			if (n == 5)
				n += 1;	/* Bad marker */
			eccpos[i] = n;
		}
		oobfree->offset = n;
		oobfree->length = mtd->oobsize - ecctotal - 1;
		layout->oobavail = oobfree->length;

		mtd->oobavail = oobfree->length;
		pr_info("hw ecc %2d bit, oob %3d, bad '5', ecc 0~4,6~%d (%d), free %d~%d (%d) ",
			eccbits, oobsize, ecctotal, ecctotal, oobfree->offset,
			oobfree->offset + oobfree->length - 1, oobfree->length);
	} else {
		oobfree->offset = 2;
		oobfree->length = mtd->oobsize - ecctotal - 2;
		layout->oobavail = oobfree->length;

		n = oobfree->offset + oobfree->length;
		for (i = 0; i < ecctotal; i++, n++)
			eccpos[i] = n;

		mtd->oobavail = oobfree->length;
		pr_info("hw ecc %2d bit, oob %3d, bad '0,1', ecc %d~%d (%d), free 2~%d (%d) ",
			eccbits, oobsize, oobfree->offset + oobfree->length,
			n - 1, ecctotal, oobfree->length + 1, oobfree->length);
	}

	mtd->ecclayout = chip->ecc.layout;
	mtd->oobavail = chip->ecc.layout->oobavail;
	return ret;
}

static int nand_hw_ecc_init_device(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nxp3220_nfc *nfc = nand_get_controller_data(chip);

	int eccbyte = 0;
	int eccbits = chip->ecc.strength;
	int eccsize = chip->ecc.size;

	int sectsize;
	int bchmode;

	switch (eccbits) {
	case 4:
		eccbyte = NX_NANDC_ECC_SZ_512_4;
		bchmode = NX_NANDC_BCH_512_4;
		break;
	case 8:
		eccbyte = NX_NANDC_ECC_SZ_512_8;
		bchmode = NX_NANDC_BCH_512_8;
		break;
	case 12:
		eccbyte = NX_NANDC_ECC_SZ_512_12;
		bchmode = NX_NANDC_BCH_512_12;
		break;
	case 16:
		eccbyte = NX_NANDC_ECC_SZ_512_16;
		bchmode = NX_NANDC_BCH_512_16;
		break;
	case 24:
		if (eccsize == 512) {
			eccbyte = NX_NANDC_ECC_SZ_512_24;
			bchmode = NX_NANDC_BCH_512_24;
		} else {
			eccbyte = NX_NANDC_ECC_SZ_1024_24;
			bchmode = NX_NANDC_BCH_1024_24;
		}
		break;
	case 40:
		eccbyte = NX_NANDC_ECC_SZ_1024_40;
		bchmode = NX_NANDC_BCH_1024_40;
		break;
	case 60:
		eccbyte = NX_NANDC_ECC_SZ_1024_60;
		bchmode = NX_NANDC_BCH_1024_60;
		break;
	default:
		goto _ecc_fail;
	}

	sectsize = eccsize + eccbyte;
	if (sectsize % 2) {
		sectsize = sectsize + 1;
		chip->ecc.postpad = 1;
	}
	nfc->sectsize = sectsize;
	nfc->bchmode = bchmode;

	chip->ecc.mode = NAND_ECC_HW_SYNDROME;
	chip->ecc.bytes = eccbyte;
	chip->ecc.read_page = nand_hw_ecc_read_page;
	chip->ecc.write_page = nand_hw_ecc_write_page;
	chip->ecc.read_oob = nand_hw_ecc_read_oob;
	chip->ecc.write_oob = nand_hw_ecc_write_oob;

	return 0;

_ecc_fail:
	pr_err("Fail: %d-bit ecc for pagesize %d is not supported!\n",
	       eccbits, eccsize);
	return -EINVAL;
}

static const struct udevice_id nxp3220_nfc_ids[] = {
	{ .compatible = "nexell,nxp3220-nand", },
	{ /* sentinel */ }
};

static int nxp3220_nfc_init(struct nxp3220_nfc *nfc)
{
	struct nand_chip *chip = &nfc->chip;
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ret = 0;

	nand_dev_init(mtd);

	chip->options |= NAND_NO_SUBPAGE_WRITE;

	chip->IO_ADDR_R = nfc->regs + NFC_DATA_BYPASS;
	chip->IO_ADDR_W = nfc->regs + NFC_DATA_BYPASS;

	/* nand callbacks */
	chip->read_word = nand_read_word;

	chip->cmd_ctrl = nand_cmd_ctrl;
	chip->dev_ready = nand_dev_ready;
	chip->select_chip = nand_select_chip;

	if (nand_scan_ident(mtd, 1, NULL)) {
		pr_err("nand scan ident failed\n");
		goto err;
	}

	if (chip->bbt_options & NAND_BBT_USE_FLASH)
		chip->bbt_options |= NAND_BBT_NO_OOB;

	chip->ecc.layout = &nand_ecc_oob;
	nand_hw_init_timings(chip);
	ret = nand_ecc_layout_hwecc(mtd);
	if (ret < 0)
		pr_err("NAND ecc: layout hwecc error\n");

	nfc->databuf_size = mtd->writesize + mtd->oobsize;
	nfc->databuf = devm_kzalloc(nfc->dev, nfc->databuf_size, GFP_KERNEL);
	if (!nfc->databuf) {
		ret = -ENOMEM;
		goto err;
	}

	ret = nand_hw_ecc_init_device(mtd);
	if (ret < 0)
		goto err;

	if (nand_scan_tail(mtd)) {
		pr_err("nand scan tail failed\n");
		goto err;
	}

	if (verify_config(mtd) < 0)
		goto err;

	if (nand_register(0, mtd)) {
		pr_err("Nand register failed\n");
		goto err;
	}

	return ret;
err:
	if (ret != -ENOMEM)
		kfree(nfc->databuf);
	kfree(nfc);

	return -1;
}

static int nxp3220_nfc_probe(struct udevice *dev)
{
	const void *blob = gd->fdt_blob;
	fdt_addr_t addr;
	int node = dev_of_offset(dev);
	struct nxp3220_nfc *nfc;
	struct nand_chip *chip;

	struct clk apb_clk;
	unsigned long rate;
	int ret = 0;

	nfc = dev_get_priv(dev);

	nfc->dev = dev;
	chip = &nfc->chip;
	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE) {
		pr_err("Dev: %s - can't get address!", dev->name);
		return -EINVAL;
	}

	nfc->regs = devm_ioremap(dev, addr, SZ_1K);
	if (!nfc->regs)
		return -ENOMEM;

	nand_set_controller_data(chip, nfc);

	/* APB clock */
	ret = clk_get_by_name(dev, "nand_apb", &apb_clk);
	if (ret)
		return ret;

	ret = clk_enable(&apb_clk);
	if (ret) {
		pr_err("nand cannot enable apb clock\n");
		return ret;
	}

	/* Core clock */
	ret = clk_get_by_name(dev, "nand_core", &nfc->core_clk);
	if (ret) {
		pr_err("nand cannot found core clock\n");
		return ret;
	}

	rate = fdtdec_get_int(blob, node, "core_freq", 0);
	if (rate == 0) {
		pr_err("nand cannot get core freq. settings\n");
		return -1;
	}

	ret = clk_set_rate(&nfc->core_clk, rate);
	if (IS_ERR_VALUE(ret)) {
		pr_err("nand cannot set core clock\n");
		return ret;
	}

	ret = clk_enable(&nfc->core_clk);
	if (ret) {
		pr_err("nand cannot enable core clock\n");
		return ret;
	}

	chip->flash_node = node;

	/* timing mode */
	ret = fdtdec_get_int(blob, node, "nand-timing-mode", -1);
	if (ret >= 0 && ret <= 5)
		chip->onfi_timing_mode_default = ret;

	ret = gpio_request_by_name(nfc->dev, "nexell,wp-gpio", 0,
			&nfc->wp_gpio, GPIOD_IS_OUT);
	if (ret) {
		pr_err("nand cannot get write-protect pin\n");
		return ret;
	}

	return nxp3220_nfc_init(nfc);
}

U_BOOT_DRIVER(nexell_nand) = {
	.name = "nxp3220_nfc",
	.id = UCLASS_MISC,
	.of_match = nxp3220_nfc_ids,
	.probe = nxp3220_nfc_probe,
	.priv_auto_alloc_size = sizeof(struct nxp3220_nfc),
};

void board_nand_init(void)
{
	struct udevice *nanddev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_GET_DRIVER(nexell_nand),
					  &nanddev);
	if (ret && ret != -ENODEV)
		pr_err("failed to nexell nand (%d)\n", ret);
}
