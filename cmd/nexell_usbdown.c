/*
 * (C) Copyright 2018 Nexell
 * SangJong, Han <hans@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <mach/usb.h>

DECLARE_GLOBAL_DATA_PTR;

int do_usbdown(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong addr;

	addr = simple_strtoul(argv[1], NULL, 16);

	printf("Download Address %lx", addr);

	usb_download(addr);

	flush_dcache_all();
	printf("Download complete\n");

	return 0;
}

U_BOOT_CMD(
	udown, CONFIG_SYS_MAXARGS, 1, do_usbdown,
	"USB Download",
	"addr(hex)"
);
