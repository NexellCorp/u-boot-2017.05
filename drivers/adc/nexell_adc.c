/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <common.h>
#include <linux/io.h>
#include <errno.h>
#include <dm.h>
#include <adc.h>
#include <asm/io.h>
#include <clk.h>

DECLARE_GLOBAL_DATA_PTR;

/* ADC Registers */
#define REG_ADCCON		(0x00)
#define REG_ADCDAT		(0x04)
#define REG_INTENB		(0x08)
#define REG_INTCLR		(0x0c)
#define REG_PRESCON		(0x10)
#define REG_ADCEN		(0x18)

/* Bit definitions for ADC */
#define ADC_CON_DATA_SEL(x)	(((x) & 0xf) << 10)
#define ADC_CON_CLK_CNT(x)	(((x) & 0xf) << 6)
#define ADC_CON_ASEL(x)		(((x) & 0x7) << 3)
#define ADC_CON_STBY		(1u << 2)
#define ADC_CON_ADEN		(1u << 0)
#define ADC_INTENB_ENB		(1u << 0)
#define ADC_INTCLR_CLR		(1u << 0)
#define ADC_PRESCON_APEN	(1u << 15)
#define ADC_PRESCON_PRES(x)	(((x) & 0x3ff) << 0)

#define ADC_DATA_SEL_VAL	(0)	/* 0:5clk, 1:4clk, 2:3clk, 3:2clk */
					/* 4:1clk: 5:not delayed, else: 4clk */
#define ADC_CLK_CNT_VAL		(6)	/* 28nm ADC */

/* channel clamp */
#define CLAMP_CHANNEL(ch)	((ch >= 6 && ch <= 9) ? ch - 2 : ch)

/* Time out */
#define ADC_CONV_TIMEOUT_US	50000
#define ADC_DAT_MASK		0xfff

/* max sampling rate */
#define ADC_SAMPLE_MAX		(1000000)
/* number of samples per clock cycle */
#define SAMPLING_RATIO		(6)
#define MAX_PRESCALER		(1024)

struct nexell_adc_priv {
	int sample_ch;
	int prescaler;
	int value;
	void __iomem *regs;
};

static int nexell_adc_channel_data(struct udevice *dev, int channel,
				   unsigned int *data)
{
	struct nexell_adc_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;

	channel = CLAMP_CHANNEL(channel);

	if (channel != priv->sample_ch) {
		error("Channel(%d) activate first.", channel);
		return -EINVAL;
	}

	while (!(readl(base + REG_INTCLR) & ADC_INTENB_ENB))
		;

	writel(ADC_INTENB_ENB, base + REG_INTCLR);
	*data = readl(base + REG_ADCDAT) & ADC_DAT_MASK;

	return 0;
}

static int nexell_adc_start_channel(struct udevice *dev, int channel)
{
	struct nexell_adc_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;
	unsigned int adccon;

	channel = CLAMP_CHANNEL(channel);

	/* channel */
	adccon = readl(base + REG_ADCCON) & ~ADC_CON_ASEL(7);
	adccon &= ~ADC_CON_ADEN;
	adccon |= ADC_CON_ASEL(channel);
	writel(adccon, base + REG_ADCCON);

	/* start */
	adccon |= ADC_CON_ADEN;
	writel(adccon, base + REG_ADCCON);

	priv->sample_ch = channel;

	return 0;
}

static int nexell_adc_stop(struct udevice *dev)
{
	struct nexell_adc_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;
	unsigned int adccon;

	/* stop (not really...) */
	adccon = readl(base + REG_ADCCON);
	adccon &= ~ADC_CON_ADEN;

	writel(adccon, base + REG_ADCCON);

	priv->sample_ch = -1;

	return 0;
}

static void setup_adc_con(struct nexell_adc_priv *priv)
{
	unsigned int adccon = 0;
	unsigned int pres = 0;
	void __iomem *base = priv->regs;

	adccon = ADC_CON_DATA_SEL(ADC_DATA_SEL_VAL) |
		ADC_CON_CLK_CNT(ADC_CLK_CNT_VAL);
	adccon &= ~ADC_CON_STBY;
	writel(adccon, base + REG_ADCCON);

	pres = ADC_PRESCON_PRES(priv->prescaler);
	writel(pres, base + REG_PRESCON);
	pres |= ADC_PRESCON_APEN;
	writel(pres, base + REG_PRESCON);

	priv->value = 0;

	writel(ADC_INTCLR_CLR, base + REG_INTCLR);
	writel(ADC_INTENB_ENB, base + REG_INTENB);
}

#define _ceil(x, y) \
	({ unsigned long __x = (x), __y = (y); (__x + __y - 1) / __y; })

static int nexell_adc_probe(struct udevice *dev)
{
	struct nexell_adc_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;

	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;
	struct clk clk;
	unsigned long pclk_freq;
	int sample_rate;
	int prescaler, min_prescaler;
	int ret;

	/* Interrupt Enable */
	writel(0x1, base + REG_INTENB);

	/* setup clock */
	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0)
		return ret;

	clk_enable(&clk);

	pclk_freq = clk_get_rate(&clk);
	min_prescaler = (int)_ceil(pclk_freq / ADC_SAMPLE_MAX, SAMPLING_RATIO);

	sample_rate = fdtdec_get_int(blob, node, "sample_rate", 0);
	if (sample_rate <= 0) {
		error("failed to get adc sampling rate\n");
		return -EINVAL;
	}

	prescaler = pclk_freq / (sample_rate * SAMPLING_RATIO);

	if (prescaler > MAX_PRESCALER || prescaler < min_prescaler) {
		error("cannot support sample rate\n");
		return -EINVAL;
	}

	priv->prescaler = prescaler - 1;
	debug("sample_rate %d, real prescaler %d, freq %lu\n",
	      sample_rate, prescaler, pclk_freq);

	/* setup adc controller */
	setup_adc_con(priv);

	priv->sample_ch = -1;

	return 0;
}

static int nexell_adc_ofdata_to_platdata(struct udevice *dev)
{
	struct adc_uclass_platdata *uc_pdata = dev_get_uclass_platdata(dev);
	struct nexell_adc_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	addr = dev_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE) {
		error("Dev: %s - can't get address!", dev->name);
		return -EINVAL;
	}

	priv->regs = devm_ioremap(dev, addr, SZ_1K);
	if (!priv->regs)
		return -ENOMEM;

	uc_pdata->data_mask = ADC_DAT_MASK;
	uc_pdata->data_format = ADC_DATA_FORMAT_BIN;
	uc_pdata->data_timeout_us = ADC_CONV_TIMEOUT_US;

	/* available channel bits: 0b1111001111 */
	uc_pdata->channel_mask = (0xf << 6) | 0xf;

	return 0;
}

static const struct adc_ops nexell_adc_ops = {
	.start_channel = nexell_adc_start_channel,
	.channel_data = nexell_adc_channel_data,
	.stop = nexell_adc_stop,
};

static const struct udevice_id nexell_adc_ids[] = {
	{ .compatible = "nexell,nxp3220-adc" },
	{ }
};

U_BOOT_DRIVER(nexell_adc) = {
	.name		= "nexell-adc",
	.id		= UCLASS_ADC,
	.of_match	= nexell_adc_ids,
	.ops		= &nexell_adc_ops,
	.probe		= nexell_adc_probe,
	.ofdata_to_platdata = nexell_adc_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct nexell_adc_priv),
};
