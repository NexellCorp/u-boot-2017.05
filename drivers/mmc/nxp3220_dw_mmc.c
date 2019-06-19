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
#include <dm.h>
#include <dt-structs.h>
#include <clk.h>
#include <mmc.h>

DECLARE_GLOBAL_DATA_PTR;

#define	DWMCI_NAME "NXP3220 DWMMC"

#define DWMCI_MODE			0x400
#define DWMCI_SRAM			0x404
#define DWMCI_DRV_PHASE			0x408
#define DWMCI_SMP_PHASE			0x40c

#define SRAM_AWAKE			0x1
#define SRAM_SLEEP			0x0

#define MODE_HS400			(1<<2)

#define	DWMMC_MAX_FREQ			200000000
#define	DWMMC_MIN_FREQ			400000

#define DEV_NAME_SDHC			"nx-sdhc"
#define DWMMC_PRE_DIV			4

struct nx_dwmci_dat {
	struct dwmci_host host;
	int dev;
	int frequency;
	int buswidth;
	int d_shift;
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

static unsigned int nx_dw_mmc_get_clk(struct dwmci_host *host, uint freq)
{
	struct nx_dwmci_dat *priv = host->priv;
	struct clk *clk;

	clk = &priv->clk;

	return clk_get_rate(clk) / DWMMC_PRE_DIV;
}

static unsigned long nx_dw_mmc_set_clk(struct dwmci_host *host,
				       unsigned int rate)
{
	struct nx_dwmci_dat *priv = host->priv;
	struct clk *clk;
	unsigned int freq = rate;

	clk = &priv->clk;
	if (clk->dev) {
		clk_disable(clk);
		rate = clk_set_rate(clk, freq);
		clk_enable(clk);
	}
	return rate;
}

static void nx_dw_mmc_set_mode(struct dwmci_host *host)
{
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
}

static void nx_dw_mmc_clk_delay(struct dwmci_host *host)
{
	struct nx_dwmci_dat *priv = host->priv;

	writel(priv->s_shift, (host->ioaddr + DWMCI_SMP_PHASE));
	writel(priv->d_shift, (host->ioaddr + DWMCI_DRV_PHASE));
}

static int nx_dw_mmc_of_platdata(const void *blob, int node,
		struct udevice *dev, struct dwmci_host *host)
{
	struct nx_dwmci_dat *priv;
	int fifo_size = 0x20;
	int index, bus_w;
	int ddr, err;
	unsigned long base;

	index = fdtdec_get_int(blob, node, "index", 0);
	bus_w = fdtdec_get_int(blob, node, "nexell,bus-width", 0);
	if (0 >= bus_w) {
		printf("failed to bus width %d for dwmmc.%d\n", bus_w, index);
		return -EINVAL;
	}

	base = devfdt_get_addr(dev);
	if (!base) {
		printf("failed to invalud base for dwmmc.%d\n", index);
		return -EINVAL;
	}

	if ((dev_read_bool(dev, "mmc-ddr-1_8v")) ||
			(dev_read_bool(dev, "mmc-ddr-1_2v")))
		ddr = true;

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

	priv->d_shift = fdtdec_get_int(blob, node, "nexell,drive_shift", 0);
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

	nx_dw_mmc_set_clk(host, priv->frequency);
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

	err = clk_get_by_index(dev, 0 , &priv->clk);
	if (err)
		return err;

	err = nx_dw_mmc_setup(gd->fdt_blob, dev, host);
	if (err)
		return err;

	dwmci_setup_cfg(&plat->cfg, host, DWMMC_MAX_FREQ, DWMMC_MIN_FREQ);
	host->mmc = &plat->mmc;
	host->mmc->priv = &priv->host;
	upriv->mmc = host->mmc;

	err = dwmci_probe(dev);
	writel(SRAM_AWAKE , (host->ioaddr + DWMCI_SRAM));
	return err;
}

static int nexell_dwmmc_bind(struct udevice *dev)
{
	struct nx_mmc_plat *plat = dev_get_platdata(dev);

	return dwmci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id nexell_dwmmc_ids[] = {
	{ .compatible = "nexell,nxp3220-dwmmc" },
	{ }
};

U_BOOT_DRIVER(nexell_dwmmc_drv) = {
	.name		= "nxp3220_dwmmc",
	.id		= UCLASS_MMC,
	.of_match	= nexell_dwmmc_ids,
	.bind		= nexell_dwmmc_bind,
	.ops		= &dm_dwmci_ops,
	.probe		= nexell_dwmmc_probe,
	.priv_auto_alloc_size = sizeof(struct nx_dwmci_dat),
	.platdata_auto_alloc_size = sizeof(struct nx_mmc_plat),
};
