/*
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/io.h>
#include <dm.h>
#include <pwm.h>
#include <asm/io.h>
#include <clk.h>

DECLARE_GLOBAL_DATA_PTR;

#define PWM_MAX_CHANNEL			4

#define REG_TCFG0(ch)			(0x100 * (ch) + 0x00)
#define REG_TCFG1(ch)			(0x100 * (ch) + 0x04)
#define REG_TCON(ch)			(0x100 * (ch) + 0x08)
#define REG_TCNTB(ch)			(0x100 * (ch) + 0x0c)
#define REG_TCMPB(ch)			(0x100 * (ch) + 0x10)
#define REG_TCNTO(ch)			(0x100 * (ch) + 0x14)
#define REG_TINTR(ch)			(0x100 * (ch) + 0x18)

#define TCON_START			(1 << 0)
#define TCON_UPDATE			(1 << 1)
#define TCON_INVERTER			(1 << 2)
#define TCON_AUTO_RELOAD		(1 << 3)

#define TINT_ENABLE			(1 << 0)
#define TINT_STATUS			(1 << 5)

const char *pwm_tclk_name[] = {
	"pwm_tclk0", "pwm_tclk1", "pwm_tclk2", "pwm_tclk3"
};

struct nexell_pwm_priv {
	void __iomem *regs;
	unsigned long tclk_freqs[PWM_MAX_CHANNEL];
};

static int nexell_pwm_set_config(struct udevice *dev, uint channel,
				uint period_ns, uint duty_ns)
{
	struct nexell_pwm_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;
	unsigned int prescaler;
	uint div, rate, rate_ns;
	u32 val;
	u32 tcnt, tcmp, tcon;
	unsigned long tclk_freq;

	if (channel >= PWM_MAX_CHANNEL)
		return -EINVAL;

	debug("%s: Configure '%s' channel %u, period_ns %u, duty_ns %u\n",
	      __func__, dev->name, channel, period_ns, duty_ns);

	val = readl(base + REG_TCFG0(channel));
	prescaler = val & 0xff;
	div = readl(base + REG_TCFG1(channel)) & 0xf;

	/* pwm clock */
	tclk_freq = priv->tclk_freqs[channel];

	rate = tclk_freq / ((prescaler + 1) * (1 << div));
	debug("%s: pwm_ipclk %lu, rate %u\n", __func__, tclk_freq, rate);

	rate_ns = 1000000000 / rate;
	tcnt = period_ns / rate_ns;
	tcmp = duty_ns / rate_ns;
	debug("%s: tcnt %u, tcmp %u\n", __func__, tcnt, tcmp);
	writel(tcnt, base + REG_TCNTB(channel));
	writel(tcmp, base + REG_TCMPB(channel));

	tcon = readl(base + REG_TCON(channel));
	tcon |= TCON_UPDATE;
	tcon |= TCON_AUTO_RELOAD;
	writel(tcon, base + REG_TCON(channel));

	tcon &= ~TCON_UPDATE;
	writel(tcon, base + REG_TCON(channel));

	/* interrupt enable */
	writel(TINT_ENABLE | TINT_STATUS, base + REG_TINTR(channel));

	return 0;
}

static int nexell_pwm_set_enable(struct udevice *dev, uint channel,
				 bool enable)
{
	struct nexell_pwm_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->regs;
	u32 mask;

	debug("%s: Enable '%s' channel %u (max:%u)\n",
	      __func__, dev->name, channel, PWM_MAX_CHANNEL);

	if (channel >= PWM_MAX_CHANNEL)
		return -EINVAL;

	mask = TCON_START;
	clrsetbits_le32(base + REG_TCON(channel), mask, enable ? mask : 0);

	/* dump registers */
	debug(" tcfg0:%u\n", readl(base + REG_TCFG0(channel)));
	debug(" tcfg1:%u\n", readl(base + REG_TCFG1(channel)));
	debug(" tcon:%u\n", readl(base + REG_TCON(channel)));
	debug(" tcntb:%u\n", readl(base + REG_TCNTB(channel)));
	debug(" tcmpb:%u\n", readl(base + REG_TCMPB(channel)));
	debug(" tcnto:%u\n", readl(base + REG_TCNTO(channel)));
	debug(" tintcstat:%u\n", readl(base + REG_TINTR(channel)));

	return 0;
}

static int nexell_pwm_probe(struct udevice *dev)
{
	struct nexell_pwm_priv *priv = dev_get_priv(dev);
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(dev);
	struct clk apbclk;
	struct clk tclk;
	unsigned long tclk_freq;
	int ch;
	int ret;

	/* setup clock */
	ret = clk_get_by_name(dev, "pwm_apb", &apbclk);
	if (ret < 0) {
		pr_err("%s: failed to get pwm apb clk\n", dev->name);
		return ret;
	}
	clk_enable(&apbclk);

	for (ch = 0; ch < PWM_MAX_CHANNEL; ch++) {
		ret = clk_get_by_name(dev, pwm_tclk_name[ch], &tclk);
		if (ret < 0) {
			pr_err("failed to get pwm%d tclk\n", ch);
			return ret;
		}

		tclk_freq = fdtdec_get_int(blob, node, "tclk_freq", 0);
		if (tclk_freq == 0) {
			pr_err("failed to get pwm%d clk freq\n", ch);
			return -EINVAL;
		}

		clk_set_rate(&tclk, tclk_freq);
		priv->tclk_freqs[ch] = tclk_freq;
		clk_enable(&tclk);

		writel(0, priv->regs + REG_TCFG0(ch));
	}

	return 0;
}

static int nexell_pwm_ofdata_to_platdata(struct udevice *dev)
{
	struct nexell_pwm_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->regs = devm_ioremap(dev, addr, SZ_1K);
	if (!priv->regs)
		return -ENOMEM;

	return 0;
}

static const struct pwm_ops nexell_pwm_ops = {
	.set_config	= nexell_pwm_set_config,
	.set_enable	= nexell_pwm_set_enable,
};

static const struct udevice_id nexell_pwm_ids[] = {
	{ .compatible = "nexell,nxp3220-pwm" },
	{ }
};

U_BOOT_DRIVER(nexell_pwm) = {
	.name	= "nexell_pwm",
	.id	= UCLASS_PWM,
	.of_match = nexell_pwm_ids,
	.ops	= &nexell_pwm_ops,
	.probe	= nexell_pwm_probe,
	.ofdata_to_platdata	= nexell_pwm_ofdata_to_platdata,
	.priv_auto_alloc_size	= sizeof(struct nexell_pwm_priv),
};
