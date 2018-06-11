/*
 * (C) Copyright 2018 Nexell
 * Youngbok, Park <ybpark@nx.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <timer.h>
#include <clk.h>

#include <asm/io.h>

/* #define debug printf */

DECLARE_GLOBAL_DATA_PTR;

struct nx_timer_priv {
	void __iomem *base;
	unsigned long timestamp;
	unsigned long lastdec;
};

#define	TIMER_FREQ	1000000
#define	TIMER_COUNT	0xFFFFFFFF

#define	REG_TCFG0			(0x00)
#define	REG_TCFG1			(0x04)
#define	REG_TCON			(0x08)
#define	REG_TCNTB0			(0x0C)
#define	REG_TCMPB0			(0x10)
#define	REG_TCNT0			(0x14)
#define	REG_CSTAT			(0x18)

#define	TCON_BIT_AUTO			(1<<3)
#define	TCON_BIT_INVT			(1<<2)
#define	TCON_BIT_UP			(1<<1)
#define	TCON_BIT_RUN			(1<<0)

/*
 * Timer HW
 */
static inline void timer_clock(void __iomem *base, int mux, int scl)
{
	u32 val = readl(base + REG_TCFG0);

	writel(val | (scl-1) , base + REG_TCFG0);
	writel(mux , base + REG_TCFG1);
}

static inline void timer_count(void __iomem *base, unsigned int cnt)
{
	writel(cnt, base + REG_TCNTB0);
	writel(cnt, base + REG_TCMPB0);
}

static inline void timer_start(void __iomem *base)
{
	writel(1 , base + REG_CSTAT);
	writel(TCON_BIT_UP, base + REG_TCON);
	writel(TCON_BIT_AUTO | TCON_BIT_RUN, base + REG_TCON);

	dmb();
}

static inline void timer_stop(void __iomem *base)
{
	writel(0, base + REG_TCON);
}

static inline unsigned long timer_read(void __iomem *base)
{
	unsigned long ret;

	ret = TIMER_COUNT - readl(base + REG_TCNT0);
	return ret;
}

static void _reset_timer_masked(struct udevice *dev)
{
	struct nx_timer_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->base;

	/* reset time */
	/* capure current decrementer value time */
	priv->lastdec = timer_read(base);
	/* start "advancing" time stamp from 0 */
	priv->timestamp = 0;
}

static unsigned long _get_timer_masked(struct udevice *dev)
{
	unsigned long now;
	struct nx_timer_priv *priv = dev_get_priv(dev);
	void __iomem *base = priv->base;
	unsigned long lastdec = priv->lastdec;
	unsigned long timestamp = priv->timestamp;

	now = timer_read(base);	/* current tick value */

	if (now >= lastdec) {			/* normal mode (non roll) */
		/* move stamp fordward with absoulte diff ticks */
		timestamp += now - lastdec;
	} else {
		/* we have overflow of the count down timer */
		/* nts = ts + ld + (TLV - now)
		 * ts=old stamp, ld=time that passed before passing through -1
		 * (TLV-now) amount of time after passing though -1
		 * nts = new "advancing time stamp"...
		 * it could also roll and cause problems.
		 */
		timestamp += now + TIMER_COUNT - lastdec;
	}
	/* save last */
	lastdec = now;
	priv->lastdec = lastdec;

	debug("now=%lu, last=%lu, timestamp=%lu\n", now, lastdec, timestamp);
	priv->timestamp = timestamp;

	return timestamp;
}

static int nx_timer_get_count(struct udevice *dev, u64 *count)
{
	*count = (u64)_get_timer_masked(dev);
	debug("%s: %lld\n", __func__, *count);

	return 0;
}

static int nx_timer_probe(struct udevice *dev)
{
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(dev);
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct nx_timer_priv *priv = dev_get_priv(dev);
	struct clk clk;
	unsigned long rate, set_rate, tclk = 0;
	unsigned long mout, thz, cmp = -1UL;
	int tscl = 0, tmux = 0;
	int mux = 0, scl = 0;
	void __iomem *base;
	int ret;

	base = devfdt_get_addr_ptr(dev);
	priv->base = base;

	/* get with PCLK */
	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0)
		return ret;

	set_rate = fdtdec_get_int(blob, node, "clock_frequency", 0);
	if (set_rate)
		clk_set_rate(&clk, set_rate);
	else
		clk_set_rate(&clk, TIMER_FREQ);

	rate = clk_get_rate(&clk);
	clk_enable(&clk);

	for (mux = 0; 5 > mux; mux++) {
		mout = rate / (1 << mux);
		scl = mout / TIMER_FREQ;
		thz = mout / scl;

		if (!(mout % TIMER_FREQ) && 256 > scl) {
			tclk = thz, tmux = mux, tscl = scl;
			break;
		}
		if (scl > 256)
			continue;
		if (abs(thz-TIMER_FREQ) >= cmp)
			continue;
		tclk = thz, tmux = mux, tscl = scl;
		cmp = abs(thz - TIMER_FREQ);
	}

	debug(" tmux:%d, tscl:%d, tclk:%ld\n", tmux, tscl, tclk);

	timer_stop(base);
	timer_clock(base, tmux, tscl);
	timer_count(base, TIMER_COUNT);
	timer_start(base);

	_reset_timer_masked(dev);

	uc_priv->clock_rate = TIMER_FREQ;

	return 0;
}

static const struct timer_ops nx_timer_ops = {
	.get_count = nx_timer_get_count,
};

static const struct udevice_id nx_timer_ids[] = {
	{ .compatible = "nexell,nxp3220-timer" },
	{}
};

U_BOOT_DRIVER(nx_timer) = {
	.name = "nxp3220-timer",
	.id = UCLASS_TIMER,
	.of_match = nx_timer_ids,
	.priv_auto_alloc_size = sizeof(struct nx_timer_priv),
	.probe = nx_timer_probe,
	.ops = &nx_timer_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
