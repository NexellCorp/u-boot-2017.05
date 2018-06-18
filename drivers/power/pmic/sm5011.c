/* SPDX-License-Identifier: (GPL-2.0+ or MIT) */
/*
 * PMIC driver for the SM5011
 *
 * (C) Copyright 2018 Nexell
 * GyoungBo, Min <mingyoungbo@nexell.co.kr>
 */

#include <common.h>
#include <fdtdec.h>
#include <errno.h>
#include <dm.h>
#include <i2c.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/sm5011.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct pmic_child_info pmic_reg_info[] = {
	{ .prefix = "BUCK", .driver = SM5011_BUCK_DRIVER },
	{ .prefix = "LDO", .driver = SM5011_LDO_DRIVER },
	{ },
};

static int sm5011_reg_count(struct udevice *dev)
{
	return SM5011_NUM_OF_REGS;
}

static int sm5011_write(struct udevice *dev, uint reg, const uint8_t *buff,
			  int len)
{
	if (dm_i2c_write(dev, reg, buff, len)) {
		error("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int sm5011_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	if (dm_i2c_read(dev, reg, buff, len)) {
		error("read error from device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

#if defined(CONFIG_PMIC_REG_DUMP)
static int sm5011_reg_dump(struct udevice *dev, const char *title)
{
	int i;
	int ret = 0;
	uint8_t value = 0;

	printf("############################################################\n");
	printf("##\e[31m %s \e[0m\n", title);
	printf("##       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F\n");
	for (i = 0; i <= SM5011_NUM_OF_REGS; i++) {
		if (i%16 == 0)
			printf("##  %02X:", i);
		if (i%4 == 0)
			printf(" ");
		ret = dm_i2c_read(dev, i, &value, 1);
		if (!ret)
			printf("%02x ", value);
		else
			printf("\e[31mxx\e[0m ");
		if ((i+1)%16 == 0)
			printf("\n");
	}
	printf("############################################################\n");

	return 0;
}
#endif

static int sm5011_param_setup(struct udevice *dev, uint8_t *cache)
{
	struct dm_sm5011_platdata *pdata = dev->platdata;

	return 0;
}

static int sm5011_device_setup(struct udevice *dev, uint8_t *cache)
{
	struct dm_sm5011_platdata *pdata = dev->platdata;

	return 0;
}

static int sm5011_probe(struct udevice *dev)
{
	uint8_t value = 0x0;
	uint8_t cache[256] = {0x0,};

	debug("%s:%d\n", __func__, __LINE__);

#if defined(CONFIG_PMIC_REG_DUMP)
	sm5011_reg_dump(dev, "PMIC Register Dump");
#endif

	sm5011_param_setup(dev, cache);
	sm5011_device_setup(dev, cache);

#if defined(CONFIG_PMIC_REG_DUMP)
	sm5011_reg_dump(dev, "PMIC Setup Register Dump");
#endif

	sm5011_read(dev, SM5011_REG_DEVICEID, &value, 1);
	printf("%s:%d: CHIP ID=0x%02x\n ", __func__, __LINE__, value);

	return 0;
}

static int sm5011_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_sm5011_platdata *pdata = dev->platdata;
	const void *blob = gd->fdt_blob;
	int sm5011_node;

	sm5011_node = fdt_subnode_offset(blob,
		fdt_path_offset(blob, "/"),	"init-sm5011");

	return 0;
}

static int sm5011_bind(struct udevice *dev)
{
	int reg_node = 0;
	const void *blob = gd->fdt_blob;
	int children;

	debug("%s: dev->name:%s\n", __func__, dev->name);

	if (!strncmp(dev->name, "sm5011", 6)) {
		reg_node = fdt_subnode_offset(blob, fdt_path_offset(blob, "/"),
				"voltage-regulators");
	} else {
		reg_node = fdt_subnode_offset(blob, dev->of_offset,
				"voltage-regulators");
	}

	if (reg_node > 0) {
		debug("%s: found regulators subnode\n", __func__);
		children = pmic_bind_children(dev, reg_node, pmic_reg_info);
		if (!children)
			debug("%s: %s - no child found\n", __func__, dev->name);
	} else {
		debug("%s: regulators subnode not found!\n", __func__);
	}

	/* Always return success for this device */
	return 0;
}

static struct dm_pmic_ops sm5011_ops = {
	.reg_count = sm5011_reg_count,
	.read = sm5011_read,
	.write = sm5011_write,
};

static const struct udevice_id sm5011_ids[] = {
	{ .compatible = "sm,sm5011", .data = (ulong)TYPE_SM5011 },
	{ }
};

U_BOOT_DRIVER(pmic_sm5011) = {
	.name = "sm5011_pmic",
	.id = UCLASS_PMIC,
	.of_match = sm5011_ids,
	.ofdata_to_platdata = sm5011_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct dm_sm5011_platdata),
	.bind = sm5011_bind,
	.probe = sm5011_probe,
	.ops = &sm5011_ops,
};
