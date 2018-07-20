/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <linux/err.h>
#include "../common/artik_mac.h"

#ifdef CONFIG_DM_REGULATOR
#include <power/regulator.h>
#endif

#define REG_RST_CONFIG	0x2008c86c
#define BOOTMODE_MASK	0x7
#define BOOTMODE_SDMMC	0x3

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
void set_board_info(void)
{
	u32 rst_config;

	rst_config = readl(REG_RST_CONFIG);
	rst_config &= BOOTMODE_MASK;

	/* set boot device only if it was sdmmc boot */
	if (rst_config == BOOTMODE_SDMMC)
		env_set_ulong("mmc_boot_dev", 2);
}
#endif

static int gpio_get_val(u32 gpio)
{
	int value = -EINVAL;
	int ret;

	ret = gpio_request(gpio, "gpio_req");
	if (ret && ret != -EBUSY) {
		printf("gpio: requesting pin %u failed\n", gpio);
		return -1;
	}

	gpio_direction_input(gpio);
	value = gpio_get_value(gpio);
	if (IS_ERR_VALUE(value))
		printf("gpio: read fail pin %u\n", gpio);

	gpio_free(gpio);

	return value;
}

#ifdef CONFIG_REVISION_TAG
u32 board_rev;

u32 get_board_rev(void)
{
	return board_rev;
}

static void check_hw_revision(void)
{
	u32 val = 0;

	val |= gpio_get_val(79);
	val <<= 1;
	val |= gpio_get_val(173);
	val <<= 1;
	val |= gpio_get_val(78);

	board_rev = val;
}

static void set_board_rev(void)
{
	char info[8] = {0, };

	snprintf(info, ARRAY_SIZE(info), "%d", board_rev);
	env_set("board_rev", info);
}

#endif

int board_init(void)
{
#ifdef CONFIG_REVISION_TAG
	check_hw_revision();
	printf("HW Revision:\t%d\n", board_rev);
#endif

	return 0;
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	set_board_info();
#endif
#ifdef CONFIG_CMD_FACTORY_INFO
	run_command("run factory_load", 0);
	generate_mac(0);
#ifdef CONFIG_HAS_ETH1
	generate_mac(1);
#endif
#endif	/* End of CONFIG_CMD_FACTORY_INFO */

#ifdef CONFIG_REVISION_TAG
	set_board_rev();
#endif
	return 0;
}
#endif

#ifdef CONFIG_DM_REGULATOR
int power_init_board(void)
{
	int ret = -ENODEV;

	ret = regulators_enable_boot_on(false);
	if (ret)
		return ret;

	return 0;
}
#endif
