/*
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef __DWC_ETH_QOS_H
#define __DWC_ETH_QOS_H

#define EQOS_MAC_REGS_BASE 0x000
#define EQOS_MTL_REGS_BASE 0xd00
#define EQOS_DMA_REGS_BASE 0x1000

#define EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD	BIT(31)

#define EQOS_AUTO_CAL_CONFIG_START			BIT(31)
#define EQOS_AUTO_CAL_CONFIG_ENABLE			BIT(29)

#define EQOS_AUTO_CAL_STATUS_ACTIVE			BIT(31)


struct eqos_config {
	bool reg_access_always_ok;
};

struct eqos_desc {
	u32 des0;
	u32 des1;
	u32 des2;
	u32 des3;
};

struct eqos_drv_ops {
	int (*start)(struct udevice *);
	void (*stop)(struct udevice *);
	int (*calibrate)(struct udevice *);
	void (*disable_calibrate)(struct udevice *);
	ulong (*get_tick_clk_rate)(struct udevice *);
	int (*set_tx_clk_speed)(struct udevice *, int);
};

struct eqos_priv {
	struct udevice *dev;
	const struct eqos_config *config;
	fdt_addr_t regs;
	struct eqos_mac_regs *mac_regs;
	struct eqos_mtl_regs *mtl_regs;
	struct eqos_dma_regs *dma_regs;
	struct mii_dev *mii;
	struct phy_device *phy;
	void *descs;
	struct eqos_desc *tx_descs;
	struct eqos_desc *rx_descs;
	int tx_desc_idx, rx_desc_idx;
	void *tx_dma_buf;
	void *rx_dma_buf;
	void *rx_pkt;
	bool started;
	bool reg_access_ok;
	phy_interface_t interface;
	int phy_addr;

	const struct eqos_drv_ops *drv_ops;
};

extern const struct eth_ops eqos_ops;

int eqos_probe(struct udevice *dev);
int eqos_remove(struct udevice *dev);

#endif
