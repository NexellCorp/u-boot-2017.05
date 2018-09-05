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

struct dm_sm5011_platdata {
	int num_bucks;
	int num_ldos;

	int npwrstm_pin;
	int irq_base;
	int irq_gpio;
	int entcxo_pin;
	int enl16_pin;

	bool wakeup;

	/* Bcuk2 */
	int buck2mode;

	/* Bcuk3 */
	int buck3mode;

	/* Bcuk4 */
	int buck4mode;

	/* Bcuk5 */
	int buck5mode;

	/* Bcuk6 */
	int buck6mode;

	int en_32kout;
	int smpl_en;
	int smpl_power_on_type;
	int smpl_timer_val;

	int longkey_val;

	int npwrstm_en;
	int mask_int_en;

	int envbatng_en;
	int envref_en;

	int mrstb_en;
	int mrstb_hehavior;
	int mrstb_nreset;
	int mrstb_key;
	int mrstb_timer_val;

	int wdt_en;
	int wdt_timer_val;

	int comp_en;
	int comp_time_val;
	int comp_duty_val;
	int comp_vref_val;

	/* ---- RTC ---- */
	int rtc_24hr_mode;
	struct rtc_time *init_time;
};

static const struct pmic_child_info pmic_reg_info[] = {
	{.prefix = "BUCK", .driver = SM5011_BUCK_DRIVER},
	{.prefix = "LDO", .driver = SM5011_LDO_DRIVER},
	{},
};

static int sm5011_reg_count(struct udevice *dev)
{
	return SM5011_NUM_OF_REGS;
}

static int sm5011_write(struct udevice *dev, uint reg, const uint8_t *buff,
			int len)
{
	if (dm_i2c_write(dev, reg, buff, len)) {
		pr_err("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int sm5011_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	if (dm_i2c_read(dev, reg, buff, len)) {
		pr_err("read error from device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int sm5011_param_setup(struct udevice *dev, uint8_t *cache)
{
	return 0;
}

static int sm5011_device_setup(struct udevice *dev, uint8_t *cache)
{
	return 0;
}

static int sm5011_probe(struct udevice *dev)
{
	uint8_t value = 0x0;
	uint8_t cache[256] = {
		0x0,
	};

	debug("%s:%d\n", __func__, __LINE__);

	sm5011_param_setup(dev, cache);
	sm5011_device_setup(dev, cache);

	sm5011_read(dev, SM5011_REG_DEVICEID, &value, 1);

	printf("SM5011: Rev %02x", value >> 4);

	sm5011_read(dev, SM5011_REG_PWRONREG, &value, 1);
	printf(" [%02x", value);
	sm5011_read(dev, SM5011_REG_PWROFFREG, &value, 1);
	printf("/%02x", value);
	sm5011_read(dev, SM5011_REG_REBOOTREG, &value, 1);
	printf("/%02x]\n", value);

	debug("%s:%d: CHIP ID=0x%02x\n ", __func__, __LINE__, value);

	return 0;
}

static int sm5011_bind(struct udevice *dev)
{
	ofnode regulators_node;
	int children;

	debug("%s: dev->name:%s\n", __func__, dev->name);

	regulators_node = dev_read_subnode(dev, "voltage-regulators");

	if (ofnode_valid(regulators_node)) {
		debug("%s: found regulators subnode\n", __func__);
		children = pmic_bind_children(dev, regulators_node,
					      pmic_reg_info);
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
	{}
};

U_BOOT_DRIVER(pmic_sm5011) = {
	.name = "sm5011_pmic",
	.id = UCLASS_PMIC,
	.of_match = sm5011_ids,
	.platdata_auto_alloc_size = sizeof(struct dm_sm5011_platdata),
	.bind = sm5011_bind,
	.probe = sm5011_probe,
	.ops = &sm5011_ops,
};
