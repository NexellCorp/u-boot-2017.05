/*
 * Copyright (C) 2018 Samsung Electronics
 * Author: Jaewon Kim <jaewon02.kim@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <pwm.h>

static int do_pwm_list(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_PWM, &dev);
	if (ret) {
		printf("No available PWM device\n");
		return CMD_RET_FAILURE;
	}

	do {
		printf("- %s\n", dev->name);

		ret = uclass_next_device(&dev);
		if (ret)
			return CMD_RET_FAILURE;
	} while (dev);

	return CMD_RET_SUCCESS;
}

static int do_pwm_setup(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	struct udevice *dev;
	uint channel, period_ns, duty_ns;
	int ret;

	if (argc < 4)
		return CMD_RET_USAGE;

	ret = uclass_first_device_err(UCLASS_PWM, &dev);
	if (ret) {
		printf("No available PWM device\n");
		return CMD_RET_FAILURE;
	}

	channel = simple_strtol(argv[1], NULL, 0);
	period_ns = simple_strtol(argv[2], NULL, 0);
	duty_ns = simple_strtol(argv[3], NULL, 0);

	ret = pwm_set_config(dev, channel, period_ns, duty_ns);
	if (ret) {
		printf("Cannot setup PWM\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_pwm_enable(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	struct udevice *dev;
	uint channel;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = uclass_first_device_err(UCLASS_PWM, &dev);
	if (ret) {
		printf("No available PWM device\n");
		return CMD_RET_FAILURE;
	}

	channel = simple_strtol(argv[1], NULL, 0);

	ret = pwm_set_enable(dev, channel, true);
	if (ret) {
		printf("Cannot enable pwm\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_pwm_disable(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	struct udevice *dev;
	uint channel;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = uclass_first_device_err(UCLASS_PWM, &dev);
	if (ret) {
		printf("No available PWM device\n");
		return CMD_RET_FAILURE;
	}

	channel = simple_strtol(argv[1], NULL, 0);

	ret = pwm_set_enable(dev, channel, false);
	if (ret) {
		printf("Cannot disable pwm\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_pwm_sub[] = {
	U_BOOT_CMD_MKENT(list, 1, 1, do_pwm_list, "", ""),
	U_BOOT_CMD_MKENT(setup, 4, 1, do_pwm_setup, "", ""),
	U_BOOT_CMD_MKENT(enable, 2, 1, do_pwm_enable, "", ""),
	U_BOOT_CMD_MKENT(disable, 2, 1, do_pwm_disable, "", ""),
};

static int do_pwm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'pwm' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_pwm_sub[0], ARRAY_SIZE(cmd_pwm_sub));
	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

U_BOOT_CMD(
	pwm, 5, 1, do_pwm,
	"PWM utility command",
	"pwm list - list PWM devices\n"
	"pwm setup <Channel> <Period> <Duty>\n"
	"pwm enable <Channel>\n"
	"pwm disable <Channel>\n"
);
