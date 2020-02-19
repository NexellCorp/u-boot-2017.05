/*
 * (C) Copyright 2020 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <dm.h>
#include <fdtdec.h>
#include <clk.h>
#include <misc.h>
#include <linux/io.h>
#include <asm/gpio.h>
#include <power/regulator.h>
#include <mach/sec_io.h>

DECLARE_GLOBAL_DATA_PTR;

#define MAX_POWER_FUNCS         5

/* register offset */
#define EFUSE_EC2		0x04C
#define EFUSE_BLOW0		0x050
#define EFUSE_BLOW1		0x054
#define EFUSE_BLOW2		0x058
#define EFUSE_BLOW_D0		0x060

/* Bit position */
#define BLOW0_SENSE_INIT	16
#define BLOW0_BLOW_IDLE		12
#define	BLOW0_BLOW_START	8
#define BLOW0_BLOW_INIT_DONE	4
#define BLOW0_BLOW_INIT		0

#define BLOW1_CLK_CYC		16
#define BLOW1_BIT_CYC		0
#define BLOW2_FSR_CYC		16
#define BLOW2_PRW_CYC		0

#define EC2_SENSE_DONE		16

/* enable gpio for efuse power */
static char * const nx_efuse_dt_gpio[] = {
	"power-gpio",
	"power-gpio-fsource0",
	"power-gpio-fsource1",
	"power-gpio-fsource2",
	"power-gpio-fsource3",
};

/* enable regulator for efuse power */
static char * const nx_efuse_dt_supply[] = {
	"power-supply",
	"power-supply-fsource0",
	"power-supply-fsource1",
	"power-supply-fsource2",
	"power-supply-fsource3",
};

/* status check with gpio */
#define EFUSE_DT_STATUS_GPIO	"status-gpio"
#define	EFUSE_FLAG_READ_RAW	BIT(0)

enum nx_efuse_endian {
	BI, /* 10023004 50067008 -> 10023004 50067008 */
	LE, /* 10023004 50067008 -> 50067008 10023004 */
};

enum nx_efuse_prot {
	FWP = BIT(0),
	FRP = BIT(1),
	WP = BIT(2),
	RP = BIT(3),
	FWR = FWP | FRP,
	WR = WP | RP,
	NC = 0,
};

struct nx_efuse_cell {
	u32 offset;
	int id;
	int bits, min;
	int bank, fs;
	int bitcyc; /* blow bit cycle */
	enum nx_efuse_endian endian; /* reverse write (unit 32bit) */
	enum nx_efuse_prot prot;
	u32 mask;
	const char *name;
	struct list_head link;
};

/*
 * - Efuse Endian: unit 32bits
 *   BI: 128 bits
 *	----------------------------------------------------------------------
 *	- Value : 0x1002300450067008900AB00CD00EF001
 *	----------------------------------------------------------------------
 *	- REGS  : R0:10023004,R1:50067008,R2:900AB00C,R3:D00EF001
 *
 *   LE: For SJTAG 128 bits
 *	----------------------------------------------------------------------
 *	- Value : 0x1002300450067008900AB00CD00EF001 ...
 *	----------------------------------------------------------------------
 *	- REGS  : R0:D00EF001,R[1]:900AB00C,R[2]:50067008,R[3]:10023004
*
 * - Efuse ID:
 *	For efuse with the same ID, it must be a contiguous address.
 */
static struct nx_efuse_cell nxp3220_efuse_cells[] = {
	/*********************************************************************
	 * RESERVED: EDS / ASB
	 *********************************************************************/
	{ 0x100,  0, 128,  128, 0, 0, 128, BI, FWP, 0, "ECID" },
	/*********************************************************************
	 * SECURE BOOT
	 *********************************************************************/
	/* MAX 128 BITS */
	{ 0x110,  1, 128,  128, 1, 1, 128, BI, FWR, (u32)(-1), "SBOOT_KEY0_0~3" },
	/* MAX 128 BITS */
	{ 0x120,  2, 128,  128, 2, 1, 128, BI,  WR, (u32)(-1), "SBOOT_KEY1_0~3" },
	/* MAX 256 BITS */
	{ 0x150,  3, 128,  128, 3, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH0_0~3" },
	{ 0x160,  3, 128,  128, 4, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH0_4~7" },
	/* MAX 256 BITS */
	{ 0x190,  4, 128,  128, 5, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH1_0~3" },
	{ 0x1a0,  4, 128,  128, 6, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH1_4~7" },
	/* MAX 256 BITS */
	{ 0x1b0,  5, 128,  128, 7, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH2_0~3" },
	{ 0x1c0,  5, 128,  128, 8, 1, 128, BI,  WP, (u32)(-1), "SBOOT_HASH2_4~7" },
	/* MAX 128 BITS */
	{ 0x1d0,  6, 128,  128, 9, 0, 128, LE, FWP, (u32)(-1), "SJTAG0~3" },
	/*********************************************************************
	 * USER
	 *********************************************************************/
	/*
	 * Efuse 128 bits : 0x1e0 : ANTI_RB0~3
	 * Efuse 256 bits : 0x210 : BACKUP_ENC_KEY0~3
	 *		  : 0x220 : BACKUP_ENC_KEY0~7
	 * Efuse 256 bits : 0x230 : ROOT_ENC_KEY0~3
	 *		  : 0x240 : ROOT_ENC_KEY4~7
	 * Efuse 512 bits : 0x250 : CM0_SBOOT_KEY0~3
	 *		  : 0x260 : CM0_SBOOT_KEY4~7
	 *		  : 0x270 : CM0_SBOOT_KEY8~11
	 *		  : 0x280 : CM0_SBOOT_KEY12~15
	 * Efuse 512 bits : 0x290 : ROOT_PRIV_KEY0~3
	 *		  : 0x2a0 : ROOT_PRIV_KEY4~7
	 *		  : 0x2b0 : ROOT_PRIV_KEY8~11
	 *		  : 0x2c0 : ROOT_PRIV_KEY12~15
	 * Efuse  16 bits : 0x2d0 : ROOT_PRIV_KEY16
	 */
	/* MAX 128 BITS */
	{ 0x1e0,  7, 128,  32, 10, 0, 128, BI,  WP, (u32)(-1), "USER0:ANTI_RB0~3" },
	/* MAX 1536 BITS */
	{ 0x210,  8, 128,  32, 11, 2, 128, BI,  WR, (u32)(-1), "USER1:BACKUP_ENC_KEY0~3" },
	{ 0x220,  8, 128,  32, 12, 2, 128, BI,  WR, (u32)(-1), "USER2:BACKUP_ENC_KEY0~7" },
	{ 0x230,  8, 128,  32, 13, 2, 128, BI, FWR, (u32)(-1), "USER3:ROOT_ENC_KEY0~3" },
	{ 0x240,  8, 128,  32, 14, 2, 128, BI, FWR, (u32)(-1), "USER4:ROOT_ENC_KEY4~7" },
	{ 0x250,  8, 128,  32, 15, 2, 128, BI,  WR, (u32)(-1), "USER5:CM0_SBOOT_KEY0~3" },
	{ 0x260,  8, 128,  32, 16, 2, 128, BI,  WR, (u32)(-1), "USER6:CM0_SBOOT_KEY4~7" },
	{ 0x270,  8, 128,  32, 17, 2, 128, BI,  WR, (u32)(-1), "USER7:CM0_SBOOT_KEY8~11" },
	{ 0x280,  8, 128,  32, 18, 2, 128, BI,  WR, (u32)(-1), "USER8:CM0_SBOOT_KEY12~15" },
	{ 0x290,  8, 128,  32, 19, 3, 128, BI,  WR, (u32)(-1), "USER9:ROOT_PRIV_KEY0~3" },
	{ 0x2a0,  8, 128,  32, 20, 3, 128, BI,  WR, (u32)(-1), "USER10:ROOT_PRIV_KEY4~7" },
	{ 0x2b0,  8, 128,  32, 21, 3, 128, BI,  WR, (u32)(-1), "USER11:ROOT_PRIV_KEY8~11" },
	{ 0x2c0,  8, 128,  32, 22, 3, 128, BI,  WR, (u32)(-1), "USER12:ROOT_PRIV_KEY12~15" },
	/* MAX 32 BITS */
	{ 0x2d0,  9,  32,  32, 60, 3,  16, BI,  WR,  0x1ff, "USER13:ROOT_PRIV_KEY16" },
	/*********************************************************************
	 * CONTROL
	 *********************************************************************/
	/* MAX 32 BITS : SJTAG-Enable[6], Verifacaion-Enable[4:2], AES-Enable[1], CPU_CFG[0] */
	{ 0x1f0, 10,  32,  32, 62, 0,   8, BI,  WP, 0x5e, "EFUSE_CFG" },
	/* MAX 32 BITS : WP[3], RP[2], FWP[1], FRP[0] */
	{ 0x1f4, 11,  32,  32, 63, 0,   8, BI,  NC, 0xf, "EFUSE_PROT" },
	/* MAX 32 BITS : DFT_BOOT_DISABLE[31], XIP_BOOT_DISABLE[30], BOOT_CFG[29:0] */
	{ 0x200, 12,  32,  32, 59, 0,  32, BI,  WP, 0, "BOOT_CFG" },
	/*********************************************************************
	 * RESERVED: EDS / CHIPNAME
	 *********************************************************************/
	{ 0x000, 13, 384,   32, 0, 0, 128, BI,  NC, 0, "CHIPNAME" },
	/*********************************************************************
	 * RESERVED: EDS / GUID 
	 *********************************************************************/
	{ 0x034, 14, 128,   32, 0, 0, 128, BI,  NC, 0, "GUID" },
	/*********************************************************************
	 * RESERVED: HPM 
	 *********************************************************************/
	{ 0x530, 15, 128,  128, 0, 0, 128, BI,  NC, 0, "HPM" },
};

#define	CELL_SIZE ARRAY_SIZE(nxp3220_efuse_cells)

struct nx_efuse_gpio {
	const char *name;
	struct gpio_desc desc;
};

struct nx_efuse_supply {
	const char *name;
	struct udevice *regulator;
};

struct nx_efuse_priv {
	void __iomem *regs;
	void __iomem *test_reg;
	struct nx_efuse_gpio status;
	struct nx_efuse_gpio enable[MAX_POWER_FUNCS];
	struct nx_efuse_supply supply[MAX_POWER_FUNCS];
	struct nx_efuse_cell *cells;
	struct list_head cell_list;
	/* clock cycle */
	int clk, bit;
	int fsr, prw;
	unsigned int flag;
	bool test_mode;
};

#define BIT_BYTE(b)	(b / 8)
#define BIT_4BYTE(b)	(b / 32)

const char *print_prot(enum nx_efuse_prot prot)
{
	const char *type = "???";

	switch (prot) {
	case FWP:
		type = "FWP";
		break;
	case FRP:
		type = "FRP";
		break;
	case WP:
		type = " WP";
		break;
	case RP:
		type = " RP";
		break;
	case FWR:
		type = "FWR";
		break;
	case WR:
		type = " WR";
		break;
	case NC:
		type = " NC";
		break;
	default:
		break;
	}

	return type;
}

static inline u32 efuse_readl(struct nx_efuse_priv *priv, int offs)
{
	if (priv->test_mode)
		return readl(priv->test_reg + offs);

	return sec_readl(priv->regs + offs);
}

static inline u32 efuse_writel(struct nx_efuse_priv *priv,
			       int offs, unsigned long val)
{
	if (priv->test_mode) {
		u32 v = readl(priv->test_reg + offs);

		writel((val & ~v) | v, priv->test_reg + offs);
		return 0;
	}

	return sec_writel(priv->regs + offs, val);
}

static inline int efuse_wait_done(struct nx_efuse_priv *priv, int offs,
				  unsigned long mask)
{
	int count = 500000;

	if (priv->test_mode)
		return 0;

	while (count-- > 0) {
		if (efuse_readl(priv, offs) & mask)
			return 0;
		udelay(1);
	}

	return -EIO;
}

static int nx_efuse_get_cell(struct udevice *dev, int offset, int bits)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct nx_efuse_cell *cell = priv->cells;
	struct nx_efuse_cell *sp = NULL;
	int id = cell->id;
	int cellnr = 0, remainder_bits = bits, efuse_bits = 0;
	int i;

	for (i = 0; i < CELL_SIZE; i++, cell++) {
		/* different id */
		if (sp && id != cell->id)
			break;

		if (offset >= cell->offset &&
		    offset < cell->offset + BIT_BYTE(ALIGN(cell->bits, 32))) {
			sp = cell;
			id = cell->id;
			remainder_bits += (offset - cell->offset) * 8;
		}

		if (sp) {
			remainder_bits -= cell->bits;
			cellnr++;
		}

		if (remainder_bits <= 0)
			break;
	}

	if (!cellnr || remainder_bits > 0)
		return -EINVAL;

	if (efuse_bits)
		efuse_bits = 0;

	for (i = 0, cell = sp; i < cellnr; i++, cell++) {
		pr_debug("EFUSE: cell add id:%d, 0x%04x, bit:%d, min:%d [%s], %s\n",
			 cell->id, cell->offset, cell->bits, cell->bits,
			 cell->endian ? "LE" : "BI", cell->name);
		list_add_tail(&cell->link, &priv->cell_list);
		efuse_bits += cell->bits;
	}

	return efuse_bits;
}

static void nx_efuse_put_cell(struct udevice *dev)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct list_head *head = &priv->cell_list;
	struct list_head *entry, *tmp;
	struct nx_efuse_cell *cell;

	if (list_empty(head))
		return;

	list_for_each_safe(entry, tmp, head) {
		cell = list_entry(entry, struct nx_efuse_cell, link);
		pr_debug("EFUSE: cell del id:%d, 0x%04x:%s\n",
			 cell->id, cell->offset, cell->name);
		list_del(entry);
	}
}

static inline int nx_efuse_is_ready(struct udevice *dev)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct gpio_desc *desc = &priv->status.desc;
	int ret;

	if (priv->test_mode || !dm_gpio_is_valid(desc))
		return 0;

	ret = dm_gpio_get_value(desc);

	if (!ret)
		printf("EFUSE: not ready %s [%s.%d] Active %s\n",
		       priv->status.name, desc->dev->name, desc->offset,
		       desc->flags & GPIOD_ACTIVE_LOW ? "L" : "H");

	return !ret ? -EINVAL : 0;
}

static inline int nx_efuse_set_sense(struct udevice *dev)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);

	efuse_writel(priv, EFUSE_BLOW0, 1 << BLOW0_SENSE_INIT);

	return efuse_wait_done(priv, EFUSE_EC2, (1 << EC2_SENSE_DONE));
}

static inline void nx_efuse_set_clk(struct udevice *dev)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);

	/*
	 * clock cycle, every 10 cycle need for clock
	 * bit cycle, efuse bit (max 128 bits)
	 */
	efuse_writel(priv, EFUSE_BLOW1, (priv->clk << BLOW1_CLK_CYC) |
				      (priv->bit << BLOW1_BIT_CYC));

	/*
	 * fsr cycle
	 * prw cycle 10000 / 41.667
	 */
	efuse_writel(priv, EFUSE_BLOW2, (priv->fsr << BLOW2_FSR_CYC) |
				      (priv->prw << BLOW2_PRW_CYC));
}

static inline void nx_efuse_set_bank(struct udevice *dev, int bank, int bitcyc)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);

	u32 val;

	/* select blow bank */
	efuse_writel(priv, EFUSE_EC2, bank);

	/*
	 * clock cycle, every 10 cycle need for clock
	 * bit cycle, efuse bit (max 128 bits)
	 */
	val = efuse_readl(priv, EFUSE_BLOW1) & ~(0xff << 0);
	efuse_writel(priv, EFUSE_BLOW1, val | ((bitcyc - 1)  << BLOW1_BIT_CYC));
}

static void nx_efuse_sel_fs(struct udevice *dev, int fs, bool on)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct nx_efuse_supply *supply = priv->supply;
	struct nx_efuse_gpio *enable = priv->enable;
	char *p;
	int index, i;

	/* Power control with supply */
	for (i = 0; i < MAX_POWER_FUNCS; i++) {
		if (!supply[i].regulator)
			continue;

		pr_debug("EFUSE: :%s %s\n", supply[i].name, on ? "on" : "off");

		/* Main power */
		if (!strcmp(supply[i].name, "power-supply")) {
			regulator_set_enable(supply[i].regulator, on);
			break;
		}

		p = strstr(supply[i].name, "power-supply-fsource");
		if (!p)
			continue;

		index = simple_strtoul(p, NULL, 10);
		if (index != fs)
			continue;

		regulator_set_enable(supply[i].regulator, on);
	}

	/* Power control with gpio */
	for (i = 0; i < MAX_POWER_FUNCS; i++) {
		struct gpio_desc *desc = &enable[i].desc;

		if (!dm_gpio_is_valid(desc))
			continue;

		pr_debug("EFUSE:  %s [%d] %s\n",
			 enable[i].name, desc->offset, on ? "on" : "off");

		/* Main gpio */
		if (!strcmp(enable[i].name, "power-gpio")) {
			dm_gpio_set_value(desc, on ? 1 : 0);
			break;
		}

		p = strstr(enable[i].name, "power-gpio-fsource");
		if (!p)
			continue;

		index = simple_strtoul(p, NULL, 10);
		if (index != fs)
			continue;

		dm_gpio_set_value(desc, on ? 1 : 0);
	}
}

static inline int nx_efuse_fuse(struct udevice *dev, struct nx_efuse_cell *cell)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	int ret = -EIO;

	/* blow init */
	efuse_writel(priv, EFUSE_BLOW0, 1 << BLOW0_BLOW_INIT);
	ret = efuse_wait_done(priv, EFUSE_BLOW0, (1 << BLOW0_BLOW_INIT_DONE));
	if (ret)
		return ret;

	/* fsource enable */
	nx_efuse_sel_fs(dev, cell->fs, true);

	/* need 1us */
	udelay(100);

	/* select blow bank */
	efuse_writel(priv, EFUSE_BLOW0, 1 << BLOW0_BLOW_START);

	ret = efuse_wait_done(priv, EFUSE_BLOW0, (1 << BLOW0_BLOW_IDLE));
	/* need 1us */
	if (!ret)
		udelay(100);

	/* fsource disable */
	nx_efuse_sel_fs(dev, cell->fs, false);

	udelay(100); /* need 1us */

	return ret;
}

static int nx_efuse_read(struct udevice *dev, int offset, void *buf, int size)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct list_head *head = &priv->cell_list;
	struct nx_efuse_cell *cell;
	u32 *buffer = buf;
	int bits, counts;
	int i, ret = -EINVAL;

	if (!IS_ALIGNED(offset, 4) || !IS_ALIGNED(size, 32)) {
		printf("%s:%d: Error: not aligned offs 4byte or 32bits\n",
		       __func__, __LINE__);
		return -EINVAL;
	}

	bits = size;
	counts = BIT_4BYTE(bits);

	nx_efuse_get_cell(dev, offset, bits);
	if (list_empty(&priv->cell_list)) {
		printf("%s:%d: no cell: read 0x%x %d bits\n",
		       __func__, __LINE__, offset, bits);
		return -EINVAL;
	}

	cell = list_first_entry(head, struct nx_efuse_cell, link);
	if (bits < cell->min) {
		printf("%s:%d: Error: read %d less than min %d bits\n",
		       __func__, __LINE__, bits, cell->min);
		goto err_read;
	}

	ret = nx_efuse_set_sense(dev);
	if (ret)
		goto err_read;

	for (i = 0; i < counts; i++) {
		int pos = cell->endian == LE &&
				!(priv->flag & EFUSE_FLAG_READ_RAW) ?
				(counts - i - 1) : i;

		buffer[pos] = efuse_readl(priv, offset + (i * 4));
	}
	ret = size;

err_read:
	nx_efuse_put_cell(dev);

	return ret;
}

static int nx_efuse_write(struct udevice *dev, int offset,
			  const void *buf, int size)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct nx_efuse_cell *cell;
	struct list_head *head = &priv->cell_list;
	struct list_head *entry, *tmp;
	const u32 *data = buf;
	int reg, offs = 0, cell_offs;
	int bits, counts;
	int val, pos = 0, i;
	int ret = -EINVAL;

	if (!IS_ALIGNED(offset, 4) || !IS_ALIGNED(size, 32)) {
		printf("%s:%d: Error: not aligned offs 4byte or 32bits\n",
		       __func__, __LINE__);
		return -EINVAL;
	}

	ret = nx_efuse_is_ready(dev);
	if (ret)
		return ret;

	bits = size;
	counts = BIT_4BYTE(bits);

	nx_efuse_get_cell(dev, offset, size);
	if (list_empty(&priv->cell_list)) {
		printf("%s:%d: no cell: write 0x%x %d bits\n",
		       __func__, __LINE__, offset, size);
		return -EINVAL;
	}

	cell = list_first_entry(head, struct nx_efuse_cell, link);
	if (bits < cell->min) {
		printf("%s:%d: Error: write %d less than min %d bits\n",
		       __func__, __LINE__, bits, cell->min);
		goto err_write;
	}

	ret = nx_efuse_set_sense(dev);
	if (ret)
		goto err_write;

	nx_efuse_set_clk(dev);

	list_for_each_safe(entry, tmp, head) {
		cell = list_entry(entry, struct nx_efuse_cell, link);

		printf("EFUSE: update id:%d, 0x%04x, bank:%2d, fs:%d, mask:0x%08x: %s [%s]\n",
		       cell->id, cell->offset, cell->bank, cell->fs, cell->mask,
		       cell->name, cell->endian ? "LE" : "BI");

		nx_efuse_set_bank(dev, cell->bank, cell->bitcyc);

		for (cell_offs = cell->offset, val = 0, i = 0;
		     i < BIT_4BYTE(cell->bits); cell_offs += 4, val = 0, i++) {
			/* start efuse offset */
			if (offs == 0 && offset == cell_offs)
				offs = cell_offs;

			if (offs && pos < counts) {
				val = data[cell->endian == LE ? counts - pos - 1 : pos];
				offs += 4, pos++;
			}

			if (priv->test_mode)
				reg = cell_offs;
			else
				reg = EFUSE_BLOW_D0 + (i * 4);

			printf("[0x%03x:0x%03x] 0x%08x\n", reg, cell_offs, val);

			ret = efuse_writel(priv, reg, val & cell->mask);
			if (ret) {
				printf("%s:%d: Error efuse write 0x%3x !!!\n",
				       __func__, __LINE__, offs);
				break;
			}
		}

		ret = nx_efuse_fuse(dev, cell);
		if (ret)
			break;
	}

	nx_efuse_set_sense(dev);

	ret = size;

err_write:
	nx_efuse_put_cell(dev);

	return ret;
}

static int nx_efuse_ioctl(struct udevice *dev, unsigned long request, void *buf)
{
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct nx_efuse_cell *cell = priv->cells;
	int id, i, total_bits;

	switch (request) {
	case 0:
		id = cell->id;
		total_bits = cell->bits;
		printf("EFUSE cell:%dEa\n", CELL_SIZE);
		printf("cell.%d\n", id);
		for (i = 0; i < CELL_SIZE; i++, cell++) {
			if (id != cell->id) {
				id = cell->id;
				printf("\ttotal: %d bit\n", total_bits);
				printf("cell.%d\n", id);
				total_bits = 0;
			}
			printf("\t0x%03x, bit:%3d, min:%3d, mask:0x%08x, bank:%2d, fs:%d, %s:%s: %s\n",
			       cell->offset, cell->bits, cell->min, cell->mask,
			       cell->bank, cell->fs, print_prot(cell->prot),
			       cell->endian ? "LE" : "BI", cell->name);

			total_bits += cell->bits;
		}
		break;
	case 1:
		priv->flag = *(unsigned int *)buf;
		break;
	case 2:
		priv->test_mode = *(bool *)buf ? true : false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct misc_ops nx_efuse_ops = {
	.read = nx_efuse_read,
	.write = nx_efuse_write,
	.ioctl = nx_efuse_ioctl,
};

static const struct udevice_id nx_efuse_ids[] = {
	{ .compatible = "nexell,nxp3220-efuse", },
	{ }
};

static int nx_efuse_probe(struct udevice *dev)
{
	ofnode node = dev_ofnode(dev);
	struct nx_efuse_priv *priv = dev_get_priv(dev);
	struct nx_efuse_supply *supply = priv->supply;
	struct nx_efuse_gpio *enable = priv->enable;
	struct udevice *devp;
	fdt_addr_t addr;
	int i, n, ret;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE) {
		pr_err("Dev: %s - can't get address!", dev->name);
		return -EINVAL;
	}

	priv->regs = devm_ioremap(dev, addr, SZ_1K);
	if (!priv->regs)
		return -ENOMEM;

	for (i = 0, n = 0; i < ARRAY_SIZE(nx_efuse_dt_supply); i++) {
		ret = uclass_get_device_by_phandle(UCLASS_REGULATOR, dev,
						   nx_efuse_dt_supply[i],
						   &devp);
		if (ret)
			continue;

		supply[n].name = nx_efuse_dt_supply[i];
		supply[n].regulator = devp;
		pr_debug("EFUSE: %s\n", supply[n].name);
		n++;
	};

	for (i = 0, n = 0; i < ARRAY_SIZE(nx_efuse_dt_gpio); i++) {
		ret = gpio_request_by_name(dev, nx_efuse_dt_gpio[i], 0,
					   &priv->enable[n].desc, 0);
		if (ret)
			continue;

		enable[n].name = nx_efuse_dt_gpio[i];
		pr_debug("EFUSE: %s [%d]\n",
			 enable[n].name, enable[n].desc.offset);
		n++;
	};

	ret = gpio_request_by_name(dev, EFUSE_DT_STATUS_GPIO, 0,
				   &priv->status.desc, 0);
	if (!ret) {
		priv->status.name = EFUSE_DT_STATUS_GPIO;
		pr_debug("EFUSE: %s [%d]\n",
			 priv->status.name, priv->status.desc.offset);
	}

	/* clock cycle */
	priv->clk = ofnode_read_u32_default(node, "clk-cycle-clk", 9);
	priv->bit = ofnode_read_u32_default(node, "clk-cycle-bit", 127);
	priv->fsr = ofnode_read_u32_default(node, "clk-cycle-fsr", 25);
	priv->prw = ofnode_read_u32_default(node, "clk-cycle-prw", 240);

	INIT_LIST_HEAD(&priv->cell_list);
	priv->cells = nxp3220_efuse_cells;

	priv->test_reg = memalign(4, SZ_4K);
	if (!priv->test_reg) {
		printf("%s:%d: Error: not allocated memory !!!\n",
		       __func__, __LINE__);
		return -ENOMEM;
	}

	memset(priv->test_reg, 0, SZ_4K);

	return 0;
}

U_BOOT_DRIVER(nexell_efuse) = {
	.name = "nexell_efuse",
	.id = UCLASS_MISC,
	.of_match = nx_efuse_ids,
	.probe = nx_efuse_probe,
	.priv_auto_alloc_size = sizeof(struct nx_efuse_priv),
	.ops = &nx_efuse_ops,
};
