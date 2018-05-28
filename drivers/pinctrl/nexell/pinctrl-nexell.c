/*
 * Pinctrl driver for Nexell SoCs
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/io.h>
#include <dm/root.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include "pinctrl-nexell.h"
#include "pinctrl-nxp3220.h"

DECLARE_GLOBAL_DATA_PTR;

/* given a pin-name, return the address of pin config registers */
void __iomem *pin_to_bank_base(struct udevice *dev, const char *pin_name,
						u32 *pin)
{
	struct nexell_pinctrl_priv *priv = dev_get_priv(dev);
	const struct nexell_pin_ctrl *pin_ctrl = priv->pin_ctrl;
	const struct nexell_pin_bank_data *bank_data = pin_ctrl->pin_banks;
	u32 nr_banks = pin_ctrl->nr_banks, idx = 0;
	char bank[10];

	/*
	 * The format of the pin name is <bank name>-<pin_number>.
	 * Example: gpioa-4 (gpioa is the bank name and 4 is the pin number)
	 */
	while (pin_name[idx] != '-') {
		bank[idx] = pin_name[idx];
		idx++;
	}
	bank[idx] = '\0';
	*pin = (u32)simple_strtoul(&pin_name[++idx], NULL, 10);

	/* lookup the pin bank data using the pin bank name */
	for (idx = 0; idx < nr_banks; idx++)
		if (!strcmp(bank, bank_data[idx].name))
			break;

	return bank_data[idx].addr;
}

int nexell_pinctrl_probe(struct udevice *dev)
{
	struct nexell_pinctrl_priv *priv = dev_get_priv(dev);
	struct nexell_pin_ctrl *ctrl;
	struct nexell_pin_bank_data *banks;
	void *blob = (void *)gd->fdt_blob;
	int node = dev_of_offset(dev);
	const char *list, *end;
	const fdt32_t *cell;
	unsigned long addr, size;
	int i, len, idx;

	ctrl = (struct nexell_pin_ctrl *)dev_get_driver_data(dev);
	banks = ctrl->pin_banks;

	if (!priv)
		return -ENODEV;

	list = fdt_getprop(blob, node, "reg-names", &len);
	if (!list)
		return -ENOENT;

	end = list + len;
	cell = fdt_getprop(blob, node, "reg", &len);
	if (!cell)
		return -ENOENT;

	for (idx = 0; list < end; idx += 2) {
		addr = fdt_translate_address((void *)blob, node, cell + idx);
		size = fdt_addr_to_cpu(cell[idx]);
		len = strlen(list);

		for (i = 0; i < ctrl->nr_banks; i++) {
			if (!strncmp(list, banks[i].name,
				     strlen(banks[i].name))) {
				banks[i].addr =
					devm_ioremap(dev, addr, size);
				break;
			}
		}
		list += len + 1;
	}

	priv->pin_ctrl = ctrl;

	return 0;
}
