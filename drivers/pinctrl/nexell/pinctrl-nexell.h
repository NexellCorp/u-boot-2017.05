/*
 * Pinctrl driver for Nexell SoCs
 * (C) Copyright 2018 Nexell
 * Bongyu, KOO <freestyle@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __PINCTRL_NEXELL_H_
#define __PINCTRL_NEXELL_H_

/**
 * struct nexell_pin_bank_data: represent a controller pin-bank data.
 * @addr: starting offset of the pin-bank registers.
 * @nr_pins: number of pins included in this bank.
 * @name: name to be prefixed for each pin in this pin bank.
 */
struct nexell_pin_bank_data {
	void __iomem	*addr;
	u8		nr_pins;
	const char	*name;
};

#define NEXELL_PIN_BANK(pins, id)			\
	{						\
		.nr_pins	= pins,			\
		.name		= id			\
	}

/**
 * struct nexell_pin_ctrl: represent a pin controller.
 * @pin_banks: list of pin banks included in this controller.
 * @nr_banks: number of pin banks.
 */
struct nexell_pin_ctrl {
	struct nexell_pin_bank_data *pin_banks;
	u32 nr_banks;
};

/**
 * struct nexell_pinctrl_priv: nexell pin controller driver private data
 * @pin_ctrl: pin controller bank information.
 */
struct nexell_pinctrl_priv {
	const struct nexell_pin_ctrl *pin_ctrl;
};

void __iomem *pin_to_bank_base(struct udevice *dev, const char *pin_name,
						u32 *pin);
int nexell_pinctrl_probe(struct udevice *dev);

#endif /* __PINCTRL_NEXELL_H_ */
