// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Nexell .
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

struct nexell_eqos_platdata {
	struct eth_pdata eth_pdata;

	struct gpio_desc phy_reset_gpio;
	struct clk clk_master_bus;
	struct clk clk_ptp_ref;
	struct clk clk_tx;
	struct clk clk_slave_bus;
};

static int eqos_start_clks_nexell(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);
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

	ret = clk_enable(&plat->clk_ptp_ref);
	if (ret < 0) {
		pr_err("clk_enable(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_master_bus;
	}

	ret = clk_set_rate(&plat->clk_ptp_ref, 125 * 1000 * 1000);
	if (ret < 0) {
		pr_err("clk_set_rate(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_ptp_ref;
	}

	if (eqos->interface == PHY_INTERFACE_MODE_RGMII) {
		ret = clk_enable(&plat->clk_tx);
		if (ret < 0) {
			pr_err("clk_enable(clk_tx) failed: %d", ret);
			goto err_disable_clk_ptp_ref;
		}
	}

	/* wake FIFO(sram) */
	writel(1, eqos->regs + 0x4000);

	debug("%s: OK\n", __func__);
	return 0;

err_disable_clk_ptp_ref:
	clk_disable(&plat->clk_ptp_ref);
err_disable_clk_master_bus:
	clk_disable(&plat->clk_master_bus);
err_disable_clk_slave_bus:
	clk_disable(&plat->clk_slave_bus);
err:
	debug("%s: FAILED: %d\n", __func__, ret);
	return ret;
}

static void eqos_stop_clks_nexell(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	if (eqos->interface == PHY_INTERFACE_MODE_RGMII)
		clk_disable(&plat->clk_tx);
	clk_disable(&plat->clk_ptp_ref);
	clk_disable(&plat->clk_master_bus);
	clk_disable(&plat->clk_slave_bus);

	debug("%s: OK\n", __func__);
}

static int eqos_start_resets_nexell(struct udevice *dev)
{
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = dm_gpio_set_value(&plat->phy_reset_gpio, 0);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, assert) failed: %d", ret);
		return ret;
	}

	mdelay(10);

	ret = dm_gpio_set_value(&plat->phy_reset_gpio, 1);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, assert) failed: %d", ret);
		return ret;
	}

	mdelay(20);

	ret = dm_gpio_set_value(&plat->phy_reset_gpio, 0);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, deassert) failed: %d", ret);
		return ret;
	}

	mdelay(30);

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_stop_resets_nexell(struct udevice *dev)
{
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);

	dm_gpio_set_value(&plat->phy_reset_gpio, 1);

	return 0;
}

static ulong eqos_get_tick_clk_rate_nexell(struct udevice *dev)
{
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);

	ulong rate = clk_get_rate(&plat->clk_slave_bus);

	return rate;
}

static int eqos_set_tx_clk_speed_nexell(struct udevice *dev, int speed)
{
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);
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

static int eqos_start_nexell(struct udevice *dev)
{
	int ret;

	ret = eqos_start_clks_nexell(dev);
	if (ret < 0) {
		pr_err("eqos_start_clks_nexell() failed: %d", ret);
		return ret;
	}

	ret = eqos_start_resets_nexell(dev);
	if (ret < 0) {
		pr_err("eqos_start_resets_nexell() failed: %d", ret);
		eqos_stop_clks_nexell(dev);
	}

	return ret;
}

static void eqos_stop_nexell(struct udevice *dev)
{
	eqos_stop_resets_nexell(dev);
	eqos_stop_clks_nexell(dev);
}

const struct eqos_drv_ops eqos_nexell_ops = {
	.start = eqos_start_nexell,
	.stop = eqos_stop_nexell,
	.get_tick_clk_rate = eqos_get_tick_clk_rate_nexell,
	.set_tx_clk_speed = eqos_set_tx_clk_speed_nexell,
};

static int eqos_probe_resources_nexell(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = gpio_request_by_name(dev, "phy-reset-gpios", 0,
				   &plat->phy_reset_gpio,
				   GPIOD_IS_OUT | GPIOD_ACTIVE_LOW);
	if (ret) {
		pr_err("gpio_request_by_name(phy reset) failed: %d", ret);
		return ret;
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

	ret = clk_get_by_name(dev, "ptp_ref", &plat->clk_ptp_ref);
	if (ret) {
		pr_err("clk_get_by_name(ptp_ref) failed: %d", ret);
		goto err_free_clk_master_bus;
		return ret;
	}

	if (eqos->interface == PHY_INTERFACE_MODE_RGMII) {
		ret = clk_get_by_name(dev, "tx", &plat->clk_tx);
		if (ret) {
			pr_err("clk_get_by_name(tx) failed: %d", ret);
			goto err_free_clk_ptp_ref;
		}
	}

	debug("%s: OK\n", __func__);
	return 0;

err_free_clk_ptp_ref:
	clk_free(&plat->clk_ptp_ref);
err_free_clk_master_bus:
	clk_free(&plat->clk_master_bus);
err_free_clk_slave_bus:
	clk_free(&plat->clk_slave_bus);
err_free_gpio_phy_reset:
	dm_gpio_free(dev, &plat->phy_reset_gpio);

	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources_nexell(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct nexell_eqos_platdata *plat = dev_get_platdata(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	if (eqos->interface == PHY_INTERFACE_MODE_RGMII)
		clk_free(&plat->clk_tx);
	clk_free(&plat->clk_ptp_ref);
	clk_free(&plat->clk_slave_bus);
	clk_free(&plat->clk_master_bus);
	dm_gpio_free(dev, &plat->phy_reset_gpio);

	debug("%s: OK\n", __func__);
	return 0;
}

static int nexell_eqos_probe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	const char *phy_mode;
	struct ofnode_phandle_args phandle_args;
	int phy;
	int ret;

	eqos->regs = devfdt_get_addr(dev);
	if (eqos->regs == FDT_ADDR_T_NONE) {
		pr_err("devfdt_get_addr() failed");
		return -ENODEV;
	}
	eqos->mac_regs = (void *)(eqos->regs + EQOS_MAC_REGS_BASE);
	eqos->mtl_regs = (void *)(eqos->regs + EQOS_MTL_REGS_BASE);
	eqos->dma_regs = (void *)(eqos->regs + EQOS_DMA_REGS_BASE);

	phy_mode = dev_read_string(dev, "phy-mode");
	if (phy_mode)
		eqos->interface = phy_get_interface_by_name(phy_mode);
	else
		eqos->interface = PHY_INTERFACE_MODE_RGMII;

	if (dev_read_phandle_with_args(dev, "phy-handle", NULL, 0, 0,
				       &phandle_args)) {
		debug("phy-handle does not exist under %s\n", dev->name);
		return -ENOENT;
	} else {
		eqos->phy_addr = ofnode_read_u32_default(phandle_args.node,
							 "reg", -1);
	}

	ret = eqos_probe_resources_nexell(dev);
	if (ret < 0) {
		pr_err("eqos_probe_resources_nexell() failed: %d", ret);
		return ret;
	}

	eqos->drv_ops = &eqos_nexell_ops;
	eqos->config = (void *)dev_get_driver_data(dev);

	ret = eqos_probe(dev);
	if (ret < 0) {
		pr_err("eqos_probe failed: %d", ret);
		eqos_remove_resources_nexell(dev);
	}

	return ret;
}

int nexell_eqos_remove(struct udevice *dev)
{
	eqos_remove(dev);
	eqos_remove_resources_nexell(dev);

	return 0;
}

static const struct eqos_config eqos_nexell_config = {
	.reg_access_always_ok = false,
};

static const struct udevice_id eqos_ids[] = {
	{
		.compatible = "nexell,nxp3220-dwmac",
		.data = (ulong)&eqos_nexell_config
	},
	{ }
};

U_BOOT_DRIVER(eth_eqos) = {
	.name = "nexell_eth_eqos",
	.id = UCLASS_ETH,
	.of_match = eqos_ids,
	.probe = nexell_eqos_probe,
	.remove = nexell_eqos_remove,
	.ops = &eqos_ops,
	.priv_auto_alloc_size = sizeof(struct eqos_priv),
	.platdata_auto_alloc_size = sizeof(struct nexell_eqos_platdata),
};
