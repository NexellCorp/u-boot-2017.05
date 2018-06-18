/* SPDX-License-Identifier: (GPL-2.0+ or MIT) */
/*
 * Regulator driver for the SM5011
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

static const struct sec_voltage_desc buck_v1 = {
	.min = 562500,
	.max = 2150000,
	.step = 12500,
};

static const struct sec_voltage_desc ldo_v1 = {
	.min = 800000,
	.max = 3350000,
	.step = 50000,
};

static const struct sm5011_para sm5011_buck_param[] = {
	{0x00, 0x00, 0x00, 0x0, 0xff, 0x00, 0x00, &buck_v1},	/* dummy */
	{0x00, 0x00, 0x00, 0x0, 0xff, 0x00, 0x00, &buck_v1},	/* dummy */
	{SM5011_BUCK2, SM5011_REG_BUCK2CNTL2, 0x00, 0x0, 0xff,
				SM5011_REG_BUCK2CNTL1, 0x01, &buck_v1},
	{SM5011_BUCK3, SM5011_REG_BUCK3CNTL2, 0x00, 0x0, 0xff,
				SM5011_REG_BUCK3CNTL1, 0x01, &buck_v1},
	{SM5011_BUCK4, SM5011_REG_BUCK4CNTL2, 0x00, 0x0, 0xff,
				SM5011_REG_BUCK4CNTL1, 0x01, &buck_v1},
	{SM5011_BUCK5, SM5011_REG_BUCK5CNTL2, 0x00, 0x0, 0xff,
				SM5011_REG_BUCK5CNTL1, 0x01, &buck_v1},
	{SM5011_BUCK6, SM5011_REG_BUCK6CNTL2, 0x00, 0x0, 0xff,
				SM5011_REG_BUCK6CNTL1, 0x01, &buck_v1},
};

static const struct sm5011_para sm5011_ldo_param[] = {
	{0x00, 0x00, 0x00, 0x0, 0x00, 0x00, 0, &ldo_v1},	/* dummy */
	{SM5011_LDO1, SM5011_REG_LDO1CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO1CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO2, SM5011_REG_LDO2CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO2CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO3, SM5011_REG_LDO3CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO3CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO4, SM5011_REG_LDO4CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO4CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO5, SM5011_REG_LDO5CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO5CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO6, SM5011_REG_LDO6CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO6CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO7, SM5011_REG_LDO7CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO7CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO8, SM5011_REG_LDO8CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO8CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO9, SM5011_REG_LDO9CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO9CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO10, SM5011_REG_LDO10CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO10CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO11, SM5011_REG_LDO11CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO11CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO12, SM5011_REG_LDO12CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO12CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO13, SM5011_REG_LDO13CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO13CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO14, SM5011_REG_LDO14CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO14CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO15, SM5011_REG_LDO15CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO15CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO16, SM5011_REG_LDO16CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO16CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO17, SM5011_REG_LDO17CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO17CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO18, SM5011_REG_LDO18CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO18CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO19, SM5011_REG_LDO19CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO19CNTL1, 0x01, &ldo_v1},
	{SM5011_LDO20, SM5011_REG_LDO20CNTL2, 0x00, 0x00, 0x3f,
				SM5011_REG_LDO20CNTL1, 0x01, &ldo_v1},
};

static int sm5011_reg_set_slp_value(struct udevice *dev,
	const struct sm5011_para *param, int uv)
{
	const struct sec_voltage_desc *desc;
	int ret, val;

	desc = param->vol;
	if (uv < desc->min || uv > desc->max)
		return -EINVAL;

	val = (uv - desc->min) / desc->step;
	val = (val & param->vol_bitmask) << param->vol_bitpos;
	ret = pmic_clrsetbits(dev->parent, param->slp_vol_addr,
		param->vol_bitmask << param->vol_bitpos, val);

	return ret;
}


static int sm5011_reg_get_value(struct udevice *dev,
	const struct sm5011_para *param)
{
	const struct sec_voltage_desc *desc;
	int ret, uv, val;

	ret = pmic_reg_read(dev->parent, param->vol_addr);
	if (ret < 0)
		return ret;

	desc = param->vol;
	val = (ret >> param->vol_bitpos) & param->vol_bitmask;
	uv = desc->min + val * desc->step;

	return uv;
}

static int sm5011_reg_set_value(struct udevice *dev,
	const struct sm5011_para *param, int uv)
{
	const struct sec_voltage_desc *desc;
	int ret, val;

	desc = param->vol;
	debug("%s:%d\n", __func__, __LINE__);
	debug("min=%d, uv=%d, max=%d\n", desc->min, uv, desc->max);
	if (uv < desc->min || uv > desc->max)
		return -EINVAL;

	val = (uv - desc->min) / desc->step;

	debug("%s:%d val=%d\n", __func__, __LINE__, val);
	val = (val & param->vol_bitmask) << param->vol_bitpos;

	debug("%s:%d masked val=%d\n", __func__, __LINE__, val);
	debug("addr=0x%x, bitmask=0x%x, bitpos=0x%x\n"
		, param->vol_addr, param->vol_bitmask, param->vol_bitpos);
	ret = pmic_clrsetbits(dev->parent, param->vol_addr,
			param->vol_bitmask << param->vol_bitpos, val);

	return ret;
}

static int sm5011_ldo_set_slp_value(struct udevice *dev, int uv)
{
	const struct sm5011_para *ldo_param = NULL;
	int ldo = dev->driver_data;

	switch (dev_get_driver_data(dev_get_parent(dev))) {
	case TYPE_SM5011:
		ldo_param = &sm5011_ldo_param[ldo];
		break;
	default:
		debug("Unsupported SM5011\n");
		return -EINVAL;
	}

	return sm5011_reg_set_slp_value(dev, ldo_param, uv);
}

static int sm5011_ldo_get_value(struct udevice *dev)
{
	const struct sm5011_para *ldo_param = NULL;
	int ldo = dev->driver_data;

	switch (dev_get_driver_data(dev_get_parent(dev))) {
	case TYPE_SM5011:
		ldo_param = &sm5011_ldo_param[ldo];
		break;
	default:
		debug("Unsupported SM5011\n");
		return -EINVAL;
	}

	return sm5011_reg_get_value(dev, ldo_param);
}

static int sm5011_ldo_set_value(struct udevice *dev, int uv)
{
	const struct sm5011_para *ldo_param = NULL;
	int ldo = dev->driver_data;

	switch (dev_get_driver_data(dev_get_parent(dev))) {
	case TYPE_SM5011:
		ldo_param = &sm5011_ldo_param[ldo];
		break;
	default:
		debug("Unsupported SM5011\n");
		return -EINVAL;
	}

	return sm5011_reg_set_value(dev, ldo_param, uv);
}

static int sm5011_reg_get_enable(struct udevice *dev,
	const struct sm5011_para *param)
{
	bool enable;
	int ret;

	ret = pmic_reg_read(dev->parent, param->reg_enaddr);
	if (ret < 0)
		return ret;

	enable = (ret >> param->reg_enbitpos) & 0x1;

	return enable;
}

static int sm5011_reg_set_enable(struct udevice *dev,
	const struct sm5011_para *param, bool enable)
{
	int ret;

	ret = pmic_reg_read(dev->parent, param->reg_enaddr);
	if (ret < 0)
		return ret;

	debug("%s:%d enable addr=0x%x\n", __func__, __LINE__
		, param->reg_enaddr);
	debug("reg enable bitpos=0x%x\n", param->reg_enbitpos);

	ret = pmic_clrsetbits(dev->parent, param->reg_enaddr,
		0x1 << param->reg_enbitpos,
		enable ? 0x1 << param->reg_enbitpos : 0);

	return ret;
}

static bool sm5011_ldo_get_enable(struct udevice *dev)
{
	const struct sm5011_para *ldo_param = NULL;
	int ldo = dev->driver_data;

	switch (dev_get_driver_data(dev_get_parent(dev))) {
	case TYPE_SM5011:
		ldo_param = &sm5011_ldo_param[ldo];
		break;
	default:
		debug("Unsupported SM5011\n");
		return -EINVAL;
	}

	return sm5011_reg_get_enable(dev, ldo_param);
}

static int sm5011_ldo_set_enable(struct udevice *dev, bool enable)
{
	const struct sm5011_para *ldo_param = NULL;
	int ldo = dev->driver_data;

	switch (dev_get_driver_data(dev_get_parent(dev))) {
	case TYPE_SM5011:
		ldo_param = &sm5011_ldo_param[ldo];
		break;
	default:
		debug("Unsupported SM5011\n");
		return -EINVAL;
	}

	return sm5011_reg_set_enable(dev, ldo_param, enable);
}

static int sm5011_ldo_probe(struct udevice *dev)
{
	struct dm_sm5011_buck_platdata *pdata = dev->platdata;
	struct dm_regulator_uclass_platdata *uc_pdata;

	uc_pdata = dev_get_uclass_platdata(dev);

	uc_pdata->type = REGULATOR_TYPE_LDO;
	uc_pdata->mode_count = 0;

	if (!pdata->boot_on) {
		if (pdata->init_uV != -ENODATA)
			sm5011_ldo_set_value(dev, pdata->init_uV);

		/* DCDC - Enable */
		if (pdata->on == 1)
			sm5011_ldo_set_enable(dev, 1);
		else if (pdata->on == 0)
			sm5011_ldo_set_enable(dev, 0);
	}

	return 0;
}

static int sm5011_ldo_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_sm5011_ldo_platdata *pdata = dev->platdata;
	const void *blob = gd->fdt_blob;
	int offset = dev->of_offset;
	debug("%s:%d\n", __func__, __LINE__);

	pdata->init_uV = fdtdec_get_int(blob, offset,
		"sm5011,init_uV", -ENODATA);
	debug(" pdata->init_uV=%d\n", pdata->init_uV);

	pdata->boot_on = fdtdec_get_bool(blob, offset,
		"regulator-boot-on");
	debug(" pdata->boot_on=%d\n", pdata->boot_on);

	pdata->always_on = fdtdec_get_bool(blob, offset,
		"regulator-always-on");
	debug(" pdata->always_on=%d\n", pdata->always_on);

	pdata->on = fdtdec_get_int(blob, offset,
		"sm5011,on", -ENODATA);
	debug(" pdata->on=%d\n", pdata->on);

	return 0;
}

static int sm5011_ldo_bind(struct udevice *dev)
{
	return 0;
}

static int sm5011_buck_set_slp_value(struct udevice *dev, int uv)
{
	int buck = dev->driver_data;

	return sm5011_reg_set_slp_value(dev, &sm5011_buck_param[buck], uv);
}

static int sm5011_buck_get_value(struct udevice *dev)
{
	int buck = dev->driver_data;

	return sm5011_reg_get_value(dev, &sm5011_buck_param[buck]);
}

static int sm5011_buck_set_value(struct udevice *dev, int uv)
{
	int buck = dev->driver_data;

	debug("%s:%d buck=%d\n", __func__, __LINE__, buck);

	return sm5011_reg_set_value(dev, &sm5011_buck_param[buck], uv);
}

static bool sm5011_buck_get_enable(struct udevice *dev)
{
	int buck = dev->driver_data;

	return sm5011_reg_get_enable(dev, &sm5011_buck_param[buck]);
}

static int sm5011_buck_set_enable(struct udevice *dev, bool enable)
{
	int buck = dev->driver_data;

	return sm5011_reg_set_enable(dev, &sm5011_buck_param[buck], enable);
}

static int sm5011_buck_probe(struct udevice *dev)
{
	struct dm_sm5011_buck_platdata *pdata = dev->platdata;
	struct dm_regulator_uclass_platdata *uc_pdata;
	uint value;

	uc_pdata = dev_get_uclass_platdata(dev);

	uc_pdata->type = REGULATOR_TYPE_BUCK;
	uc_pdata->mode_count = 0;

	if (!pdata->boot_on) {
		if (pdata->init_uV != -ENODATA)
			sm5011_buck_set_value(dev, pdata->init_uV);

		/* DCDC - Enable */
		if (pdata->on == 1)
			sm5011_buck_set_enable(dev, 1);
		else if (pdata->on == 0)
			sm5011_buck_set_enable(dev, 0);
	}

	return 0;
}

static int sm5011_buck_ofdata_to_platdata(struct udevice *dev)
{
	struct dm_sm5011_buck_platdata *pdata = dev->platdata;
	const void *blob = gd->fdt_blob;
	int offset = dev->of_offset;
	debug("%s:%d\n", __func__, __LINE__);

	pdata->init_uV = fdtdec_get_int(blob, offset,
		"sm5011,init_uV", -ENODATA);
	debug(" pdata->init_uV=%d\n", pdata->init_uV);

	pdata->boot_on = fdtdec_get_bool(blob, offset,
		"regulator-boot-on");
	debug(" pdata->boot_on=%d\n", pdata->boot_on);

	pdata->always_on = fdtdec_get_bool(blob, offset,
		"regulator-always-on");
	debug(" pdata->always_on=%d\n", pdata->always_on);

	pdata->on = fdtdec_get_int(blob, offset,
		"sm5011,on", -ENODATA);
	debug(" pdata->on=%d\n", pdata->on);

	return 0;
}

static int sm5011_buck_bind(struct udevice *dev)
{
	return 0;
}

static const struct dm_regulator_ops sm5011_ldo_ops = {
	.get_value  = sm5011_ldo_get_value,
	.set_value  = sm5011_ldo_set_value,
	.get_enable = sm5011_ldo_get_enable,
	.set_enable = sm5011_ldo_set_enable,
};

U_BOOT_DRIVER(sm5011_ldo) = {
	.name = SM5011_LDO_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &sm5011_ldo_ops,
	.bind = sm5011_ldo_bind,
	.probe = sm5011_ldo_probe,
	.ofdata_to_platdata = sm5011_ldo_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct dm_sm5011_ldo_platdata),
};

static const struct dm_regulator_ops sm5011_buck_ops = {
	.get_value  = sm5011_buck_get_value,
	.set_value  = sm5011_buck_set_value,
	.get_enable = sm5011_buck_get_enable,
	.set_enable = sm5011_buck_set_enable,
};

U_BOOT_DRIVER(sm5011_buck) = {
	.name = SM5011_BUCK_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &sm5011_buck_ops,
	.bind = sm5011_buck_bind,
	.probe = sm5011_buck_probe,
	.ofdata_to_platdata = sm5011_buck_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct dm_sm5011_buck_platdata),
};
