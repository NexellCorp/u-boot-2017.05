/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __NXP3220_NAND_H__
#define __NXP3220_NAND_H__

#include <linux/mtd/rawnand.h>
#include <clk.h>
#include <asm/gpio.h>

#define NFC_CTRL		(0x0)
#define NFC_BCH_MODE		(0x4)
#define NFC_DMA_CTRL		(0x8)
#define NFC_DMA_ADDR		(0xc)
#define NFC_DMA_SIZE		(0x10)
#define NFC_DMA_SUBP		(0x14)
#define NFC_DDR_CTRL		(0x18)
#define NFC_CMD_TIME		(0x1c)
#define NFC_DATA_TIME		(0x20)
#define NFC_DDR_TIME		(0x24)
#define NFC_RAND		(0x28)
#define NFC_STATUS		(0x2c)
#define NFC_SUBPAGE		(0x68)
#define NFC_ERR_INFO		(0x6c)
#define NFC_CMD			(0x70)
#define NFC_ADDR		(0x74)
#define NFC_DATA		(0x78)
#define NFC_DATA_BYPASS		(0x7c)
#define NFC_SRAM		(0x100)

struct nxp3220_nfc {
	void __iomem *regs;
	struct nand_chip chip;
	struct udevice *dev;
	u8 *databuf;
	u32 databuf_size;
	int sectsize;			/* data + meta + ecc + pad */
	int bchmode;
	int timing_mode;		/* onfi timing mode */
	struct clk core_clk;
	struct gpio_desc wp_gpio;
};

enum NX_NANDC_INT {
	NX_NANDC_INT_RDY = 0, /* NAND Flash Controller Ready */
	NX_NANDC_INT_DMA = 1 /* NAND Flash Controller DMA */
};

enum NX_NANDC_DMA {
	NX_NANDC_CPU_MODE = 0,
	NX_NANDC_DMA_MODE = 1
};

enum NX_NANDC_CFG_ECC {
	NX_NANDC_ECC_512_4 = 0,
	NX_NANDC_ECC_512_8 = 1,
	NX_NANDC_ECC_512_12 = 2,
	NX_NANDC_ECC_512_16 = 3,
	NX_NANDC_ECC_512_24 = 4,
	NX_NANDC_ECC_1024_24 = 5,
	NX_NANDC_ECC_1024_40 = 6,
	NX_NANDC_ECC_1024_60 = 7
};

enum NX_NANDC_ECC_SIZE {
	NX_NANDC_ECC_SZ_512_4 = 7,
	NX_NANDC_ECC_SZ_512_8 = 13,
	NX_NANDC_ECC_SZ_512_12 = 20,
	NX_NANDC_ECC_SZ_512_16 = 26,
	NX_NANDC_ECC_SZ_512_24 = 39,
	NX_NANDC_ECC_SZ_1024_24 = 42,
	NX_NANDC_ECC_SZ_1024_40 = 70,
	NX_NANDC_ECC_SZ_1024_60 = 105
};

enum NX_NANDC_BCH {
	NX_NANDC_BCH_512_4 = 0,
	NX_NANDC_BCH_512_8 = 1,
	NX_NANDC_BCH_512_12 = 2,
	NX_NANDC_BCH_512_16 = 3,
	NX_NANDC_BCH_512_24 = 4,
	NX_NANDC_BCH_1024_24 = 8,
	NX_NANDC_BCH_1024_40 = 9,
	NX_NANDC_BCH_1024_60 = 10
};

#endif /* __NXP3220_NAND_H__ */