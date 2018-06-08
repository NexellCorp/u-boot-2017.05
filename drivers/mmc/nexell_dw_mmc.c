/*
 * (C) Copyright 2018 Nexell
 * Youngbok, Park <park@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <dwmmc.h>
#include <reset.h>
#include <errno.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <dm.h>
#include <dt-structs.h>
#include <clk.h>
#include <mmc.h>

DECLARE_GLOBAL_DATA_PTR;

#define	DWMCI_NAME "NEXELL DWMMC"

#ifdef CONFIG_ARCH_NXP3220

#define DWMCI_MODE			0x400
#define DWMCI_SRAM			0x404
#define DWMCI_DRV_PHASE			0x408
#define DWMCI_SMP_PHASE			0x40c

#define MODE_HS400			(1<<2)

#elif defined CONFIG_ARCH_S5P4418
#define DWMCI_CLKSEL			0x09C
#define DWMCI_SHIFT_0			0x0
#define DWMCI_SHIFT_1			0x1
#define DWMCI_SHIFT_2			0x2
#define DWMCI_SHIFT_3			0x3
#define DWMCI_SET_SAMPLE_CLK(x)	(x)
#define DWMCI_SET_DRV_CLK(x)	((x) << 16)
#define DWMCI_SET_DIV_RATIO(x)	((x) << 24)
#define DWMCI_CLKCTRL			0x114

#define NX_MMC_CLK_DELAY(x, y, a, b)	(((x & 0xFF) << 0)	|\
					((y & 0x03) << 16)	|\
					((a & 0xFF) << 8)	|\
					((b & 0x03) << 24))
#endif


#define	DWMMC_MAX_FREQ			200000000
#define	DWMMC_MIN_FREQ			400000

#define DEV_NAME_SDHC			"nx-sdhc"

struct nx_dwmci_dat {
#ifdef CONFIG_DM_MMC
	struct dwmci_host host;
#endif
	int dev;
	int frequency;
	int buswidth;
	int d_delay;
	int d_shift;
	int s_delay;
	int s_shift;
	int ddr;
	char name[50];
	struct clk clk;
	struct clk ahbclk;
};

struct nx_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

/* FIXME : This func will be remove after support pinctrl.
 * set mmc pad alternative func.
 */

static unsigned int nx_dw_mmc_get_clk(struct dwmci_host *host, uint freq)
{
	struct nx_dwmci_dat *priv = host->priv;
	struct clk *clk;

	clk = &priv->clk;

	return clk_get_rate(clk) / 2;
}

static unsigned long nx_dw_mmc_set_clk(struct dwmci_host *host,
				       unsigned int rate)
{
	struct nx_dwmci_dat *priv = host->priv;
	struct clk *clk;
	struct clk *ahbclk;
	unsigned int freq = rate;

	ahbclk = &priv->ahbclk;
	if (ahbclk->dev) {
		clk_disable(ahbclk);
		rate = clk_set_rate(ahbclk, 100000000 );
		clk_enable(ahbclk);
	}

	clk = &priv->clk;
	if (clk->dev) {
		clk_disable(clk);
		rate = clk_set_rate(clk, freq );
		clk_enable(clk);
	}

	return rate;
}

static void nx_dw_mmc_set_mode(struct dwmci_host *host)
{
#ifdef CONFIG_ARCH_NXP3220
	struct nx_dwmci_dat *priv = host->priv;
	int mode = 0;

	switch (priv->buswidth)	{
	case 1:
		mode = 0;
		break;
	case 4:
		mode = 1;
		break;
	case 8:
		mode = 2;
		break;
	}
	if (priv->ddr && priv->frequency == 200000000)
		mode |= MODE_HS400;
	writel(mode, (host->ioaddr + DWMCI_MODE));
	
#endif
}

static void nx_dw_mmc_clk_delay(struct dwmci_host *host)
{
	struct nx_dwmci_dat *priv = host->priv;

#ifdef CONFIG_ARCH_NXP3220
	writel(priv->s_shift, (host->ioaddr + DWMCI_SMP_PHASE));
	writel(priv->d_shift, (host->ioaddr + DWMCI_DRV_PHASE));
#elif defined CONFIG_ARCH_S5P4418
	unsigned int delay;

	delay = NX_MMC_CLK_DELAY(priv->d_delay,
				 priv->d_shift, priv->s_delay, priv->s_shift);

	writel(delay, (host->ioaddr + DWMCI_CLKCTRL));
#endif
}

static int nx_dw_mmc_of_platdata(const void *blob, int node,
		struct udevice *dev, struct dwmci_host *host)
{
	struct nx_dwmci_dat *priv;
	int fifo_size = 0x20;
	int index, bus_w;
	int ddr, err;
	unsigned long base;
	struct reset_ctl rst;

	index = fdtdec_get_int(blob, node, "index", 0);
	bus_w = fdtdec_get_int(blob, node, "nexell,bus-width", 0);
	if (0 >= bus_w) {
		printf("failed to bus width %d for dwmmc.%d\n", bus_w, index);
		return -EINVAL;
	}

	base = fdtdec_get_uint(blob, node, "reg", 0);
	if (!base) {
		printf("failed to invalud base for dwmmc.%d\n", index);
		return -EINVAL;
	}
	ddr = fdtdec_get_int(blob, node, "nexell,ddr", 0);

	priv = malloc(sizeof(struct nx_dwmci_dat));
	if (!priv) {
		printf("failed to private malloc for dwmmc.%d\n", index);
		return -ENOMEM;
	}

	err = clk_get_by_index(dev, 0, &priv->clk);
	if (err) {
		printf("warning! : not found sdmmc core clock\n");
		return -EINVAL;
	}

	err = clk_get_by_index(dev, 1, &priv->ahbclk);
	if (err)
		printf("warning! : not found sdmmc ahb clock\n");

	err = reset_get_by_name(dev, "nexell-dwmmc", &rst);
	if (!err) {
		reset_assert(&rst);
		reset_deassert(&rst);
	}

	priv->d_delay = fdtdec_get_int(blob, node, "nexell,drive_dly", 0);
	priv->d_shift = fdtdec_get_int(blob, node, "nexell,drive_shift", 0);
	priv->s_delay = fdtdec_get_int(blob, node, "nexell,sample_dly", 0);
	priv->s_shift = fdtdec_get_int(blob, node, "nexell,sample_shift", 0);
	priv->frequency = fdtdec_get_int(blob, node, "frequency", 0);

	priv->buswidth = bus_w;
	priv->ddr = ddr;

	host->priv = priv;

	host->fifo_mode = fdtdec_get_bool(blob, node, "fifo-mode");
	host->ioaddr = (void *)base;
	host->dev_index = index;
	host->buswidth = bus_w;
	host->name = DWMCI_NAME;
	host->dev_id = host->dev_index;
	host->get_mmc_clk = nx_dw_mmc_get_clk;
	host->fifoth_val =
	    MSIZE(0x2) | RX_WMARK(fifo_size / 2 - 1) | TX_WMARK(fifo_size / 2);

	if (ddr)
		host->caps |= MMC_MODE_DDR_52MHz;

	return 0;
}

static int nx_dw_mmc_setup(const void *blob, struct udevice *dev,
		struct dwmci_host *host)
{
	struct nx_dwmci_dat *priv;
	int  err;

	err = nx_dw_mmc_of_platdata(blob, dev_of_offset(dev), dev, host);
	if (err) {
		printf("failed to decode dwmmc dev\n");
		return err;
	}

	nx_dw_mmc_set_mode(host);

	priv = (struct nx_dwmci_dat *)host->priv;

	nx_dw_mmc_set_clk(host, priv->frequency * 4);
	nx_dw_mmc_clk_delay(host);

	return 0;
}

static int nexell_dwmmc_probe(struct udevice *dev)
{
	struct nx_mmc_plat *plat = dev_get_platdata(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct nx_dwmci_dat *priv = dev_get_priv(dev);
	struct dwmci_host *host = &priv->host;
	int err;

	err =  nx_dw_mmc_setup(gd->fdt_blob, dev, host);
	if (err)
		return err;

	err = clk_get_by_index(dev, 0 , &priv->clk);
	dwmci_setup_cfg(&plat->cfg, host, DWMMC_MAX_FREQ, DWMMC_MIN_FREQ);
	host->mmc = &plat->mmc;
	host->mmc->priv = &priv->host;
	upriv->mmc = host->mmc;

	err = dwmci_probe(dev);
	writel(0x03 , (host->ioaddr + DWMCI_SRAM));
	return err;

}

static int nexell_dwmmc_bind(struct udevice *dev)
{
	struct nx_mmc_plat *plat = dev_get_platdata(dev);

	return dwmci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id nexell_dwmmc_ids[] = {
	{ .compatible = "nexell,nexell-dwmmc" },
	{ }
};

U_BOOT_DRIVER(nexell_dwmmc_drv) = {
	.name		= "nexell_dwmmc",
	.id		= UCLASS_MMC,
	.of_match	= nexell_dwmmc_ids,
	.bind		= nexell_dwmmc_bind,
	.ops		= &dm_dwmci_ops,
	.probe		= nexell_dwmmc_probe,
	.priv_auto_alloc_size = sizeof(struct nx_dwmci_dat),
	.platdata_auto_alloc_size = sizeof(struct nx_mmc_plat),
};
