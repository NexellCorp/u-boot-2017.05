// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 */
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/gpio.h>
#include <asm/io.h>

#include "dwc_eth_qos.h"

/* These registers are Tegra186-specific */
#define EQOS_TEGRA186_REGS_BASE 0x8800
struct eqos_tegra186_regs {
	uint32_t sdmemcomppadctrl;			/* 0x8800 */
	uint32_t auto_cal_config;			/* 0x8804 */
	uint32_t unused_8808;				/* 0x8808 */
	uint32_t auto_cal_status;			/* 0x880c */
};

struct tegra186_eqos_platdata {
	struct eth_pdata eth_pdata;

	struct eqos_tegra186_regs *tegra186_regs;
	struct reset_ctl reset_ctl;
	struct gpio_desc phy_reset_gpio;
	struct clk clk_master_bus;
	struct clk clk_rx;
	struct clk clk_ptp_ref;
	struct clk clk_tx;
	struct clk clk_slave_bus;
};

static int eqos_start_clks_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = clk_enable(&plat->clk_slave_bus);
	if (ret < 0) {
		pr_err("clk_enable(clk_slave_bus) failed: %d", ret);
		goto err;
	}

	ret = clk_enable(&plat->clk_master_bus);
	if (ret < 0) {
		pr_err("clk_enable(clk_master_bus) failed: %d", ret);
		goto err_disable_clk_slave_bus;
	}

	ret = clk_enable(&plat->clk_rx);
	if (ret < 0) {
		pr_err("clk_enable(clk_rx) failed: %d", ret);
		goto err_disable_clk_master_bus;
	}

	ret = clk_enable(&plat->clk_ptp_ref);
	if (ret < 0) {
		pr_err("clk_enable(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_rx;
	}

	ret = clk_set_rate(&plat->clk_ptp_ref, 125 * 1000 * 1000);
	if (ret < 0) {
		pr_err("clk_set_rate(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_ptp_ref;
	}

	ret = clk_enable(&plat->clk_tx);
	if (ret < 0) {
		pr_err("clk_enable(clk_tx) failed: %d", ret);
		goto err_disable_clk_ptp_ref;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_disable_clk_ptp_ref:
	clk_disable(&plat->clk_ptp_ref);
err_disable_clk_rx:
	clk_disable(&plat->clk_rx);
err_disable_clk_master_bus:
	clk_disable(&plat->clk_master_bus);
err_disable_clk_slave_bus:
	clk_disable(&plat->clk_slave_bus);
err:
	debug("%s: FAILED: %d\n", __func__, ret);
	return ret;
}

static void eqos_stop_clks_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clk_disable(&plat->clk_tx);
	clk_disable(&plat->clk_ptp_ref);
	clk_disable(&plat->clk_rx);
	clk_disable(&plat->clk_master_bus);
	clk_disable(&plat->clk_slave_bus);

	debug("%s: OK\n", __func__);
}

static int eqos_start_resets_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = dm_gpio_set_value(&plat->phy_reset_gpio, 1);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, assert) failed: %d", ret);
		return ret;
	}

	udelay(2);

	ret = dm_gpio_set_value(&plat->phy_reset_gpio, 0);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, deassert) failed: %d", ret);
		return ret;
	}

	ret = reset_assert(&plat->reset_ctl);
	if (ret < 0) {
		pr_err("reset_assert() failed: %d", ret);
		return ret;
	}

	udelay(2);

	ret = reset_deassert(&plat->reset_ctl);
	if (ret < 0) {
		pr_err("reset_deassert() failed: %d", ret);
		return ret;
	}

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_stop_resets_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);

	reset_assert(&plat->reset_ctl);
	dm_gpio_set_value(&plat->phy_reset_gpio, 1);

	return 0;
}

static int eqos_calibrate_pads_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	setbits_le32(&plat->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	udelay(1);

	setbits_le32(&plat->tegra186_regs->auto_cal_config,
		     EQOS_AUTO_CAL_CONFIG_START | EQOS_AUTO_CAL_CONFIG_ENABLE);

	ret = wait_for_bit(__func__, &plat->tegra186_regs->auto_cal_status,
			   EQOS_AUTO_CAL_STATUS_ACTIVE, true, 10, false);
	if (ret) {
		pr_err("calibrate didn't start");
		goto failed;
	}

	ret = wait_for_bit(__func__, &plat->tegra186_regs->auto_cal_status,
			   EQOS_AUTO_CAL_STATUS_ACTIVE, false, 10, false);
	if (ret) {
		pr_err("calibrate didn't finish");
		goto failed;
	}

	ret = 0;

failed:
	clrbits_le32(&plat->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	debug("%s: returns %d\n", __func__, ret);

	return ret;
}

static void eqos_disable_calibration_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clrbits_le32(&plat->tegra186_regs->auto_cal_config,
		     EQOS_AUTO_CAL_CONFIG_ENABLE);
}

static ulong eqos_get_tick_clk_rate_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);

	return clk_get_rate(&plat->clk_slave_bus);
}

static int eqos_set_tx_clk_speed_tegra186(struct udevice *dev, int speed)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	ulong rate;
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	switch (speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		break;
	default:
		pr_err("invalid speed %d", speed);
		return -EINVAL;
	}

	ret = clk_set_rate(&plat->clk_tx, rate);
	if (ret < 0) {
		pr_err("clk_set_rate(tx_clk, %lu) failed: %d", rate, ret);
		return ret;
	}

	return 0;
}

static int eqos_start_tegra186(struct udevice *dev)
{
	int ret;

	ret = eqos_start_clks_tegra186(dev);
	if (ret < 0) {
		pr_err("eqos_start_clks_tegra186() failed: %d", ret);
		return ret;
	}

	ret = eqos_start_resets_tegra186(dev);
	if (ret < 0) {
		pr_err("eqos_start_resets_tegra186() failed: %d", ret);
		eqos_stop_clks_tegra186(dev);
	}

	return ret;
}

static void eqos_stop_tegra186(struct udevice *dev)
{
	eqos_stop_resets_tegra186(dev);
	eqos_stop_clks_tegra186(dev);
}

const struct eqos_drv_ops eqos_tegra186_ops = {
	.start = eqos_start_tegra186,
	.stop = eqos_stop_tegra186,
	.calibrate = eqos_calibrate_pads_tegra186,
	.disable_calibrate = eqos_disable_calibration_tegra186,
	.get_tick_clk_rate = eqos_get_tick_clk_rate_tegra186,
	.set_tx_clk_speed = eqos_set_tx_clk_speed_tegra186,
};

static int eqos_probe_resources_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = reset_get_by_name(dev, "eqos", &plat->reset_ctl);
	if (ret) {
		pr_err("reset_get_by_name(rst) failed: %d", ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "phy-reset-gpios", 0,
				   &plat->phy_reset_gpio,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		pr_err("gpio_request_by_name(phy reset) failed: %d", ret);
		goto err_free_reset_eqos;
	}

	ret = clk_get_by_name(dev, "slave_bus", &plat->clk_slave_bus);
	if (ret) {
		pr_err("clk_get_by_name(slave_bus) failed: %d", ret);
		goto err_free_gpio_phy_reset;
	}

	ret = clk_get_by_name(dev, "master_bus", &plat->clk_master_bus);
	if (ret) {
		pr_err("clk_get_by_name(master_bus) failed: %d", ret);
		goto err_free_clk_slave_bus;
	}

	ret = clk_get_by_name(dev, "rx", &plat->clk_rx);
	if (ret) {
		pr_err("clk_get_by_name(rx) failed: %d", ret);
		goto err_free_clk_master_bus;
	}

	ret = clk_get_by_name(dev, "ptp_ref", &plat->clk_ptp_ref);
	if (ret) {
		pr_err("clk_get_by_name(ptp_ref) failed: %d", ret);
		goto err_free_clk_rx;
		return ret;
	}

	ret = clk_get_by_name(dev, "tx", &plat->clk_tx);
	if (ret) {
		pr_err("clk_get_by_name(tx) failed: %d", ret);
		goto err_free_clk_ptp_ref;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_free_clk_ptp_ref:
	clk_free(&plat->clk_ptp_ref);
err_free_clk_rx:
	clk_free(&plat->clk_rx);
err_free_clk_master_bus:
	clk_free(&plat->clk_master_bus);
err_free_clk_slave_bus:
	clk_free(&plat->clk_slave_bus);
err_free_gpio_phy_reset:
	dm_gpio_free(dev, &plat->phy_reset_gpio);
err_free_reset_eqos:
	reset_free(&plat->reset_ctl);

	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources_tegra186(struct udevice *dev)
{
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clk_free(&plat->clk_tx);
	clk_free(&plat->clk_ptp_ref);
	clk_free(&plat->clk_rx);
	clk_free(&plat->clk_slave_bus);
	clk_free(&plat->clk_master_bus);
	dm_gpio_free(dev, &plat->phy_reset_gpio);
	reset_free(&plat->reset_ctl);

	debug("%s: OK\n", __func__);
	return 0;
}

static int tegra186_eqos_probe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct tegra186_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	eqos->regs = devfdt_get_addr(dev);
	if (eqos->regs == FDT_ADDR_T_NONE) {
		pr_err("devfdt_get_addr() failed");
		return -ENODEV;
	}
	eqos->mac_regs = (void *)(eqos->regs + EQOS_MAC_REGS_BASE);
	eqos->mtl_regs = (void *)(eqos->regs + EQOS_MTL_REGS_BASE);
	eqos->dma_regs = (void *)(eqos->regs + EQOS_DMA_REGS_BASE);

	plat->tegra186_regs = (void *)(eqos->regs + EQOS_TEGRA186_REGS_BASE);

	eqos->phy_addr = 0;
	eqos->interface = PHY_INTERFACE_MODE_RGMII;

	ret = eqos_probe_resources_tegra186(dev);
	if (ret < 0) {
		pr_err("eqos_probe_resources_tegra186() failed: %d", ret);
		return ret;
	}

	eqos->drv_ops = &eqos_tegra186_ops;
	eqos->config = (void *)dev_get_driver_data(dev);

	ret = eqos_probe(dev);
	if (ret < 0) {
		pr_err("eqos_probe failed: %d", ret);
		eqos_remove_resources_tegra186(dev);
	}

	return ret;
}

int tegra186_eqos_remove(struct udevice *dev)
{
	eqos_remove(dev);
	eqos_remove_resources_tegra186(dev);

	return 0;
}

static const struct eqos_config eqos_tegra186_config = {
	.reg_access_always_ok = false,
};

static const struct udevice_id eqos_ids[] = {
	{
		.compatible = "nvidia,tegra186-eqos",
		.data = (ulong)&eqos_tegra186_config
	},
	{ }
};

U_BOOT_DRIVER(eth_eqos) = {
	.name = "tegra186_eth_eqos",
	.id = UCLASS_ETH,
	.of_match = eqos_ids,
	.probe = tegra186_eqos_probe,
	.remove = tegra186_eqos_remove,
	.ops = &eqos_ops,
	.priv_auto_alloc_size = sizeof(struct eqos_priv),
	.platdata_auto_alloc_size = sizeof(struct tegra186_eqos_platdata),
};
