/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */
#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <fastboot.h>
#include <spi.h>
#include <spi_flash.h>
#include <dm/device-internal.h>
#include <linux/err.h>

#include "f_fastboot_partmap.h"

/*
 * refer to cmd/sf.c
 */
static struct spi_flash *flash;

static ulong bytes_per_second(unsigned int len, ulong start_ms)
{
	/* less accurate but avoids overflow */
	if (len >= ((unsigned int)-1) / 1024)
		return len / (max(get_timer(start_ms) / 1024, 1UL));
	else
		return 1024 * len / max(get_timer(start_ms), 1UL);
}

static int fb_spi_flash_probe(int dev)
{
	struct udevice *new, *bus_dev;
	int bus = dev;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	int ret;

	debug("spi %d:%d %d %d\n", bus, cs, speed, mode);

#ifdef CONFIG_DM_SPI_FLASH
	/* Remove the old device, otherwise probe will just be a nop */
	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (!ret)
		device_remove(new, DM_REMOVE_NORMAL);

	flash  = NULL;
	ret = spi_flash_probe_bus_cs(bus, cs, speed, mode, &new);
	if (ret) {
		printf("Failed to initialize SPI flash at %u:%u (error %d)\n",
		       bus, cs, ret);
		return -EIO;
	}

	flash  = dev_get_uclass_priv(new);
#else
	if (flash)
		spi_flash_free(flash);

	new = spi_flash_probe(bus, cs, speed, mode);
	flash = new;

	if (!new) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return -EIO;
	}

	flash  = new;
#endif
	return 0;
}

static const char *fb_spi_flash_update_block(struct spi_flash *flash,
					     u32 offset, size_t len,
					     const char *buf, char *cmp_buf,
					     size_t *skipped)
{
	char *ptr = (char *)buf;

	debug("offset=%#x, sector_size=%#x, len=%#zx\n",
	      offset, flash->sector_size, len);
	/* Read the entire sector so to allow for rewriting */
	if (spi_flash_read(flash, offset, flash->sector_size, cmp_buf))
		return "read";
	/* Compare only what is meaningful (len) */
	if (memcmp(cmp_buf, buf, len) == 0) {
		debug("Skip region %x size %zx: no change\n",
		      offset, len);
		*skipped += len;
		return NULL;
	}
	/* Erase the entire sector */
	if (spi_flash_erase(flash, offset, flash->sector_size))
		return "erase";
	/* If it's a partial sector, copy the data into the temp-buffer */
	if (len != flash->sector_size) {
		memcpy(cmp_buf, buf, len);
		ptr = cmp_buf;
	}
	/* Write one complete sector */
	if (spi_flash_write(flash, offset, flash->sector_size, ptr))
		return "write";

	return NULL;
}

static int fb_spi_flash_update(struct spi_flash *flash, u32 offset,
			       size_t len, const char *buf)
{
	const char *err_oper = NULL;
	char *cmp_buf;
	const char *end = buf + len;
	size_t todo;            /* number of bytes to do in this pass */
	size_t skipped = 0;     /* statistics */
	const ulong start_time = get_timer(0);
	size_t scale = 1;
	const char *start_buf = buf;
	ulong delta;

	if (end - buf >= 200)
		scale = (end - buf) / 100;
	cmp_buf = memalign(ARCH_DMA_MINALIGN, flash->sector_size);
	if (cmp_buf) {
		ulong last_update = get_timer(0);

		for (; buf < end && !err_oper; buf += todo, offset += todo) {
			todo = min_t(size_t, end - buf, flash->sector_size);
			if (get_timer(last_update) > 100) {
				printf("   \rUpdating, %zu%% %lu B/s",
				       100 - (end - buf) / scale,
					bytes_per_second(buf - start_buf,
							 start_time));
				last_update = get_timer(0);
			}
			err_oper = fb_spi_flash_update_block(flash, offset,
							     todo, buf,
							     cmp_buf, &skipped);
		}
	} else {
		err_oper = "malloc";
	}

	free(cmp_buf);

	putc('\r');
	if (err_oper) {
		printf("SPI flash failed in %s step\n", err_oper);
		return -EIO;
	}

	delta = get_timer(start_time);
	debug("%zu bytes written, %zu bytes skipped", len - skipped,
	      skipped);
	debug(" in %ld.%lds, speed %ld B/s\n",
	      delta / 1000, delta % 1000, bytes_per_second(len, start_time));

	return 0;
}

static int fb_spi_flash_write(struct fb_part_par *f_part, void *buffer,
			      u64 bytes)
{
	int dev = f_part->dev_no;
	loff_t offset = f_part->start;
	loff_t size = bytes;
	int ret;

	debug("part name:%s [0x%x]\n", f_part->name, f_part->type);

	if (f_part->type & PART_TYPE_PARTITION)
		return -EINVAL;

	ret = fb_spi_flash_probe(dev);
	if (ret)
		return ret;

	ret = fb_spi_flash_update(flash, offset, size, buffer);

	debug("** part start: 0x%llx **\n", f_part->start);
	debug("** bytes     : 0x%llx (0x%llx) **\n", f_part->length, bytes);

	fastboot_okay("");

	return ret;
}

static int fb_spi_flash_capacity(int dev, u64 *length)
{
	int ret;

	debug("** spi.%d capacity **\n", dev);

	ret = fb_spi_flash_probe(dev);
	if (ret)
		return ret;

	*length = flash->size;

	debug("total = %llu\n", *length);

	return 0;
}

static struct fb_part_ops fb_partmap_ops_spi = {
	.write = fb_spi_flash_write,
	.capacity = fb_spi_flash_capacity,
};

static struct fb_part_dev fb_partmap_dev_spi = {
	.device	= "spi",
	.dev_max = 4,
	.mask = FASTBOOT_PART_RAW,
	.ops = &fb_partmap_ops_spi,
};

FB_PARTMAP_BIND_INIT(spi, &fb_partmap_dev_spi)
