// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018 Nexell
 * Junghyun, Kim <jhkim@nexell.co.kr>
 *
 */
#include <config.h>
#include <common.h>
#include <lcd.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPLASH_SOURCE
#include <splash.h>

static struct splash_location splash_loc[] = {
	{
	.name = SPLASH_STORAGE_NAME,
	.storage = SPLASH_STORAGE_DEVICE,
	.flags = SPLASH_STORAGE_FLAGS,
	.devpart = SPLASH_STORAGE_DEVPART,
#ifdef SPLASH_STORAGE_NAND_UBI
	.mtdpart = SPLASH_STORAGE_DEVPART,
	.ubivol = SPLASH_STORAGE_NAME,
#endif
	},
};
#endif

#ifdef CONFIG_CMD_BMP
void draw_logo(void)
{
	int x = BMP_ALIGN_CENTER, y = BMP_ALIGN_CENTER;
	ulong load;
#ifdef CONFIG_SPLASH_SOURCE
	char *s;
	int ret;

	s = env_get("splashimage");
	if (!s)
		return;

	load = simple_strtoul(s, NULL, 16);

	/* load env_get("splashfile") */
	ret = splash_source_load(splash_loc,
			sizeof(splash_loc) / sizeof(struct splash_location));
	if (ret)
		return;

#ifndef CONFIG_QUICKBOOT_QUIET
	printf("splashimage: 0x%lx\n", load);
#endif
#endif
	if (!load)
		return;

	bmp_display(load, x, y);
}
#endif

