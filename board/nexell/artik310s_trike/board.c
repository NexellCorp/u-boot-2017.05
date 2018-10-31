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
#include <dm.h>
#include <adc.h>
#include "../common/artik_mac.h"
#include "../common/boot_mode.h"

#ifdef CONFIG_DM_REGULATOR
#include <power/regulator.h>
#endif

#ifdef CONFIG_ARTIK_OTA
#include <artik_ota.h>
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
	if (rst_config == BOOTMODE_SDMMC) {
		env_set_ulong("mmc_boot_dev", 2);
		env_set("dfu_alt_info", CONFIG_DFU_ALT_BOOT_SD);
	}
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

#ifdef CONFIG_BOARD_TYPES
char board_type[32];

static void check_board_type(void)
{
	struct gpio_desc gpio = {};
	struct fdtdec_phandle_args args;
	struct udevice *adc;
	unsigned int chan, value;
	int node, ret;

	node = fdt_node_offset_by_compatible(gd->fdt_blob, 0,
					     "artik,boardtype");
	if (node < 0)
		return;

	gpio_request_by_name_nodev(offset_to_ofnode(node), "en-gpio", 0,
				   &gpio, GPIOD_IS_OUT);
	if (dm_gpio_is_valid(&gpio))
		dm_gpio_set_value(&gpio, 1);

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, node, "adc", NULL,
					     1, 0, &args);
	if (ret) {
		debug("%s: Cannot get adc phandle: ret=%d\n", __func__, ret);
		return;
	}

	chan = args.args[0];

	ret = uclass_get_device_by_of_offset(UCLASS_ADC, args.node, &adc);
	if (ret) {
		debug("%s: Cannot get ADC: ret=%d\n", __func__, ret);
		return;
	}

	ret = adc_channel_single_shot(adc->name, chan, &value);
	if (ret) {
		debug("Cannot read ADC2 value\n");
		return;
	}

	if (value < 227) /* 0 ~ 0.1v */
		strncpy(board_type, "Eagleye310", ARRAY_SIZE(board_type));
	else if (value >= 910 && value < 1365) /* 0.4 ~ 0.6v */
		strncpy(board_type, "FF-Board", ARRAY_SIZE(board_type));
	else if (value >= 3412) /* 1.5 ~ 1.8v */
		strncpy(board_type, "Dev-Kit", ARRAY_SIZE(board_type));
	else
		strncpy(board_type, "Unknown", ARRAY_SIZE(board_type));

	printf("BOARD: %s\n", board_type);
}
#endif

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

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	dcache_enable();
}
#endif

int board_init(void)
{
#ifdef CONFIG_REVISION_TAG
	check_hw_revision();
	printf("HW Revision:\t%d\n", board_rev);
#endif
#ifdef CONFIG_BOARD_TYPES
	check_board_type();
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
#ifdef CONFIG_BOARD_TYPES
	env_set("board_type", board_type);
#endif

	check_boot_mode();

#ifdef CONFIG_ARTIK_OTA
	check_ota_update();
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
