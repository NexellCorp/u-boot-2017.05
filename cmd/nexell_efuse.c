/*
 * (C) Copyright 2020 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <misc.h>
#include <hexdump.h>
#include <linux/ctype.h>

#define EFUSE_FLAG_READ_RAW    BIT(0)

static unsigned int copy_hex_to_bin(const char *src, u8 *dst, int bits)
{
	int size = (bits / 8);
	char *s = skip_spaces(src);
	u32 *d = (u32 *)dst;

	while (size > 0) {
		if (isspace(*s)) {
			s++;
			continue;
		}

		if (*s == '\0')
			break;

		hex2bin((u8 *)d, s, 4);
		*d = cpu_to_be32(*d);
		s += 8, d++;
		size -= 4;
	}

	if (size)
		printf("Error: The bits %d and hex file size different!!!\n",
		       bits);

	return size ? -EINVAL : 0;
}

static int do_efuse(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	struct udevice *dev;
	char *cmd, *s;
	u32 *buf = NULL, addr;
	int offs, bits;
	int flags = 0;
	int i, ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_GET_DRIVER(nexell_efuse), &dev);
	if (ret) {
		printf("%s: efuse-device not found !!!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < argc; i++)
		debug("argv[%d]: %s\n", i, argv[i]);

	cmd = argv[1];

	if (!strcmp(cmd, "info"))
		return misc_ioctl(dev, 0, NULL);

	if (!strcmp(cmd, "test")) {
		if (argc < 3)
			return CMD_RET_USAGE;

		flags = simple_strtoul(argv[2], NULL, 10) ? 1 : 0;

		return misc_ioctl(dev, 2, &flags);
	}

	if (!strncmp(cmd, "read", 4)) {
		bool is_dump = true;

		s = strchr(cmd, '.');
		if (s && (!strncmp(s, ".raw", 4) || !strncmp(s, ".dump", 5))) {
			if (argc < 4)
				return CMD_RET_USAGE;

			if (!strncmp(s, ".raw", 4))
				flags = EFUSE_FLAG_READ_RAW;

			offs = (int)simple_strtoul(argv[2], NULL, 16); /* hex */
			bits = (int)simple_strtoul(argv[3], NULL, 10); /* int */

			buf = malloc(bits / 8);
			if (!buf) {
				printf("%s:%d: Error not allocated memory!!!\n",
				       __func__, __LINE__);
				return -ENOMEM;
			}
			addr = (u32)buf;
		} else {
			if (argc < 5)
				return CMD_RET_USAGE;

			addr = (u32)simple_strtoul(argv[2], NULL, 16); /* hex */
			offs = (int)simple_strtoul(argv[3], NULL, 16); /* hex */
			bits = (int)simple_strtoul(argv[4], NULL, 10); /* int */
			is_dump = false;
		}

		misc_ioctl(dev, 1, &flags);
		ret = misc_read(dev, offs, (void *)addr, bits);
		if (ret < 0) {
			printf("%s: efuse read failed\n", __func__);
		} else if (is_dump) {
			printf("Read 0x%04x, %d bits\n", offs, bits);
			for (i = 0; i < bits / 32; i++) {
				printf("%08x", buf[i]);
				if (i != (bits / 32) - 1 && ((i + 1) % 4) != 0)
					printf(":");
				if (((i + 1) % 4) == 0)
					printf("\n");
			}
			printf("\n");
		}

		if (buf)
			free(buf);

		return ret;
	}

	if (!strncmp(cmd, "write", 5)) {
		u32 data;

		s = strchr(cmd, '.');

		if (s && !strncmp(s, ".reg", 4)) {
			if (argc < 4)
				return CMD_RET_USAGE;

			data = (u32)simple_strtoul(argv[2], NULL, 16); /* hex */
			offs = (int)simple_strtoul(argv[3], NULL, 16); /* hex */
			bits = 32;
			addr = (u32)&data;
		} else {
			if (argc < 5)
				return CMD_RET_USAGE;

			addr = (u32)simple_strtoul(argv[2], NULL, 16); /* hex */
			offs = (int)simple_strtoul(argv[3], NULL, 16); /* hex */
			bits = (int)simple_strtoul(argv[4], NULL, 10); /* int */

			if (s && !strncmp(s, ".hex", 4)) {
				buf = malloc(bits / 8);
				if (!buf)
					return -ENOMEM;

				ret = copy_hex_to_bin((char *)addr, (u8 *)buf,
						      bits);
				if (ret) {
					free(buf);
					return ret;
				}

				addr = (u32)buf;
			}
		}

		ret = misc_write(dev, offs, (void *)addr, bits);
		if (ret < 0)
			printf("%s: efuse write failed\n", __func__);

		if (buf)
			free(buf);

		return ret;
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(efuse, CONFIG_SYS_MAXARGS, 1, do_efuse,
	"Nexell Efuse programing",
	"info\n"
	"    show programing efuse registers\n"
	"efuse read addr offs bits\n"
	"    read 'bits'(int) starting at offset 'offs'(hex)\n"
	"    to memory address 'addr'(hex)\n"
	"    invert dump the efuse registers according to the efuse format\n"
	"efuse read.dump offs bits\n"
	"    dump 'bits'(int) starting at offset 'offs'(hex)\n"
	"    invert dump the efuse registers according to the efuse format\n"
	"efuse read.raw  offs bits\n"
	"    dump 'bits'(int) starting at offset 'offs'(hex)\n"
	"    dump the efuse registers\n"
	"efuse write addr offs size\n"
	"    write 'bits'(int) starting at offset 'offs'(hex)\n"
	"    from memory address 'addr'(hex)\n"
	"    data must be binary type on the memory.\n"
	"    NOTE> bits must be aligned 32bits.\n"
	"efuse write.hex addr offs size\n"
	"    write 'bits'(int) starting at offset 'offs'(hex)\n"
	"    from memory address 'addr'(hex).\n"
	"    data must be hex type on the memory.\n"
	"    NOTE> bits must be aligned 32bits.\n"
	"efuse write.reg value offs\n"
	"    write value(4byte:hex) starting at offset\n"
	"efuse test [1 or 0]\n"
	"    switch efuse test mode(programing to memory)\n"
	"NOTE>\n"
	" addr must be aligned 4byte\n"
	" bits must be aligned 32bits.\n"
);
