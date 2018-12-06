/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <clk.h>
#include <display.h>
#include <video.h>
#include <panel.h>
#include <video_bridge.h>
#include <backlight.h>
#include <asm/io.h>

#include "display.h"

DECLARE_GLOBAL_DATA_PTR;

#define	PARSE_DT_CONFIG		BIT(0)
#define	PARSE_DT_OVERLAY	BIT(1)
#define	PARSE_DT_TIMING		BIT(2)
#define MAX_ENABLE_GPIO		6

/* Maximum LCD size we support */
enum {
	LCD_MAX_WIDTH = 1920,
	LCD_MAX_HEIGHT = 1200,
	LCD_MAX_BPP = 32, /* 2^4 = 16 bpp */
};

struct nx_display_priv {
	struct nx_dpc_reg *dpc_base;
	struct nx_mlc_reg *mlc_base;
	struct clk clk_axi;
	struct clk clk_x2, clk_x1;
	struct nx_display dp;
	struct udevice *backlight;
	struct udevice *lcd, *panel, *bridge;
};

#define screen_overlay(d, n)	((struct nx_overlay *)&((d)->ovl[n]))
#define screen_primary(d)	((d)->ovl[(d)->primary])

/* the PAD output delay. */
#define	DPC_SYNC_DELAY_RGB_PVD		BIT(0)
#define	DPC_SYNC_DELAY_HSYNC_CP1	BIT(1)
#define	DPC_SYNC_DELAY_VSYNC_FRAME	BIT(2)
#define	DPC_SYNC_DELAY_DE_CP		BIT(3)

#define INTERLACE(f) (f & DISPLAY_FLAGS_INTERLACED ? true : false)

static void nx_display_set_clock(struct nx_display_priv *priv)
{
	struct nx_display *dp = &priv->dp;
	struct nx_mlc_reg *reg = priv->mlc_base;
	struct display_timing *timing = &dp->dpi.timing;
	unsigned int format = dp->dpi.dpp.out_format;
	unsigned long rate;
	int div = 1;

	clk_set_rate(&priv->clk_x2, timing->pixelclock.typ);

	if (dp->dpi.mpu_lcd)
		div = 2;

	if (format == DP_OUT_FORMAT_MRGB565)
		div = 2;
	else if	(format == DP_OUT_FORMAT_SRGB888)
		div = 6;

	rate = clk_get_rate(&priv->clk_x2);
	clk_set_rate(&priv->clk_x1, rate / div);

	clk_enable(&priv->clk_axi);

	nx_mlc_set_clock_pclk_mode(reg, NX_PCLK_MODE_ALWAYS);
	nx_mlc_set_clock_bclk_mode(reg, NX_BCLK_MODE_ALWAYS);

	clk_enable(&priv->clk_x2);
	clk_enable(&priv->clk_x1);
}

static void nx_display_set_format(struct nx_display_priv *priv)
{
	struct nx_display *dp = &priv->dp;
	struct nx_display_info *dpi = &dp->dpi;
	struct display_timing *timing = &dpi->timing;
	struct nx_mlc_reg *reg = priv->mlc_base;
	int width = dp->width;
	int height = dp->height;
	int video_prior = dp->video_prior;
	u32 bg_color = dp->back_color;
	int i;

	/* MLC TOP layer */
	nx_mlc_set_screen_size(reg, width, height);
	nx_mlc_set_video_priority(reg, video_prior);
	nx_mlc_set_background(reg, bg_color);

	nx_mlc_set_field_enable(reg, INTERLACE(timing->flags));
	nx_mlc_set_rgb_gamma_power(reg, 0, 0, 0);
	nx_mlc_set_rgb_gamma_enable(reg, false);
	nx_mlc_set_gamma_priority(reg, 0);
	nx_mlc_set_gamma_dither(reg, false);
	nx_mlc_set_power_mode(reg, true);

	debug("%s: screen %dx%d, %s, priority:%d, bg:0x%x\n",
	      __func__, width, height, INTERLACE(timing->flags) ?
	      "Interlace" : "Progressive", video_prior, bg_color);

	for (i = 0; i < dp->num_overlays; i++) {
		struct nx_overlay *ovl = screen_overlay(dp, i);
		int sx = ovl->left, sy = ovl->top;
		int ex = sx + ovl->width - 1, ey = sy + ovl->height - 1;
		int pixel_byte = ovl->bit_per_pixel / 8;
		int lock_size = 16;	/* fix mem lock size */
		int id = ovl->id;
		u32 format = ovl->format;
		bool alpha = false;

		if (!ovl->enable)
			continue;

		/* set alphablend */
		if (format == DP_MLC_FORMAT_A1R5G5B5 ||
		    format == DP_MLC_FORMAT_A1B5G5R5 ||
		    format == DP_MLC_FORMAT_A4R4G4B4 ||
		    format == DP_MLC_FORMAT_A4B4G4R4 ||
		    format == DP_MLC_FORMAT_A8R3G3B2 ||
		    format == DP_MLC_FORMAT_A8B3G3R2 ||
		    format == DP_MLC_FORMAT_A8R8G8B8 ||
		    format == DP_MLC_FORMAT_A8B8G8R8)
			alpha = true;

		if (ovl->bgr_mode) {
			format |= 1 << 31;
			debug("%s: BGR plane format:0x%x\n", __func__, format);
		}

		/* MLC ovl */
		nx_mlc_set_layer_lock_size(reg, id, lock_size);
		nx_mlc_set_layer_alpha(reg, id, 0, alpha);
		nx_mlc_set_rgb_color_inv(reg, id, 0, false);
		nx_mlc_set_rgb_format(reg, id, format);
		nx_mlc_set_rgb_invalid_position(reg, id, 0, 0, 0, 0, 0, false);
		nx_mlc_set_rgb_invalid_position(reg, id, 1, 0, 0, 0, 0, false);

		nx_mlc_set_layer_position(reg, id, sx, sy, ex, ey);
		nx_mlc_set_rgb_stride(reg, id, pixel_byte,
				      ovl->width * pixel_byte);
		nx_mlc_set_rgb_address(reg, id, ovl->fb);

		debug("%s: id.%d %d * %d, %dbpp, fmt:0x%x\n", __func__,
		      id, ovl->width, ovl->height, pixel_byte * 8, format);
		debug("%s: fb:0x%x, l:%d, t:%d, r:%d, b:%d, hs:%d, vs:%d\n",
		      __func__, ovl->fb, sx, sy, ex, ey,
		      ovl->width * pixel_byte, pixel_byte);
	}
}

#define INTERLACE(f) (f & DISPLAY_FLAGS_INTERLACED ? true : false)
#define RGB_MODE(fmt) ( \
		fmt == DP_OUT_FORMAT_CCIR656 || \
		fmt == DP_OUT_FORMAT_CCIR601_8 || \
		fmt == DP_OUT_FORMAT_CCIR601_16A || \
		fmt == DP_OUT_FORMAT_CCIR601_16B ? false : true)

static void nx_display_sync_format(struct nx_display_priv *priv)
{
	struct nx_dpc_reg *reg = priv->dpc_base;
	struct nx_display *dp = &priv->dp;
	struct display_timing *timing = &dp->dpi.timing;
	struct nx_display_par *dpp = &dp->dpi.dpp;
	unsigned int format = dpp->out_format;
	bool emb_sync = format == DP_OUT_FORMAT_CCIR656 ? true : false;
	enum nx_dpc_dither r_dither, g_dither, b_dither;

	if (format == DP_OUT_FORMAT_RGB555 ||
	    format == DP_OUT_FORMAT_MRGB555A ||
	    format == DP_OUT_FORMAT_MRGB555B) {
		r_dither = NX_DPC_DITHER_5BIT;
		g_dither = NX_DPC_DITHER_5BIT;
		b_dither = NX_DPC_DITHER_5BIT;
	} else if (format == DP_OUT_FORMAT_RGB565 ||
		   format == DP_OUT_FORMAT_MRGB565) {
		r_dither = NX_DPC_DITHER_5BIT;
		b_dither = NX_DPC_DITHER_5BIT;
		g_dither = NX_DPC_DITHER_6BIT;
	} else if (format == DP_OUT_FORMAT_RGB666 ||
		   format == DP_OUT_FORMAT_MRGB666) {
		r_dither = NX_DPC_DITHER_6BIT;
		g_dither = NX_DPC_DITHER_6BIT;
		b_dither = NX_DPC_DITHER_6BIT;
	} else {
		r_dither = NX_DPC_DITHER_BYPASS;
		g_dither = NX_DPC_DITHER_BYPASS;
		b_dither = NX_DPC_DITHER_BYPASS;
	}

	nx_dpc_set_mode(reg, format,
			INTERLACE(timing->flags), RGB_MODE(format),
			dpp->swap_rb ? true : false,
			emb_sync, dpp->yc_order);
	nx_dpc_set_dither(reg, r_dither, g_dither, b_dither);

	debug("%s: display: %s, %s\n",
	      __func__, RGB_MODE(format) ? "RGB" : "YUV",
	      INTERLACE(timing->flags) ? "INTERACE" : "PROGRESSIVE");
}

static void nx_display_sync_delay(struct nx_display_priv *priv)
{
	struct nx_dpc_reg *reg = priv->dpc_base;
	struct nx_display *dp = &priv->dp;
	struct nx_display_par *dpp = &dp->dpi.dpp;
	int rgb_pvd = 0, hs_cp1 = 7;
	int vs_frm = 7, de_cp2 = 7;

	if (dpp->delay_mask & DPC_SYNC_DELAY_RGB_PVD)
		rgb_pvd = dpp->d_rgb_pvd;
	if (dpp->delay_mask & DPC_SYNC_DELAY_HSYNC_CP1)
		hs_cp1 = dpp->d_hsync_cp1;
	if (dpp->delay_mask & DPC_SYNC_DELAY_VSYNC_FRAME)
		vs_frm = dpp->d_vsync_frame;
	if (dpp->delay_mask & DPC_SYNC_DELAY_DE_CP)
		de_cp2 = dpp->d_de_cp2;

	nx_dpc_set_delay(reg, rgb_pvd, hs_cp1, vs_frm, de_cp2);

	debug("%s: display: delay RGB:%d, HS:%d, VS:%d, DE:%d\n",
	      __func__, rgb_pvd, hs_cp1, vs_frm, de_cp2);
}

#define SYNC_POL(s, t)	(s & t ? NX_DPC_POL_ACTIVELOW : NX_DPC_POL_ACTIVEHIGH)
#define SYNC_VAL(v, d)	(v ? v : d)

static void nx_display_sync_mode(struct nx_display_priv *priv)
{
	struct nx_dpc_reg *reg = priv->dpc_base;
	struct nx_display *dp = &priv->dp;
	struct display_timing *timing = &dp->dpi.timing;
	enum display_flags flags = timing->flags;
	struct nx_display_par *dpp = &dp->dpi.dpp;
	/* check with LOW bit */
	enum nx_dpc_polarity field_pol = dpp->invert_field ?
			NX_DPC_POL_ACTIVELOW : NX_DPC_POL_ACTIVEHIGH;
	enum nx_dpc_polarity hs_pol = SYNC_POL(flags, DISPLAY_FLAGS_HSYNC_LOW);
	enum nx_dpc_polarity vs_pol = SYNC_POL(flags, DISPLAY_FLAGS_VSYNC_LOW);
	int div = INTERLACE(flags) ? 2 : 1;
	int efp = SYNC_VAL(dpp->evfront_porch, timing->vfront_porch.typ);
	int ebp = SYNC_VAL(dpp->evback_porch, timing->vback_porch.typ);
	int esw = SYNC_VAL(dpp->evsync_len, timing->vsync_len.typ);
	int eso = SYNC_VAL(dpp->evstart_offs, 0);
	int eeo = SYNC_VAL(dpp->evend_offs, 0);
	int vso = SYNC_VAL(dpp->vstart_offs, 0);
	int veo = SYNC_VAL(dpp->vend_offs, 0);

	nx_dpc_set_sync(reg, INTERLACE(flags),
			timing->hactive.typ, timing->vactive.typ / div,
			timing->hsync_len.typ, timing->hfront_porch.typ,
			timing->hback_porch.typ,
			timing->vsync_len.typ, timing->vfront_porch.typ,
			timing->vback_porch.typ,
			vso, veo, field_pol, hs_pol, vs_pol,
			esw, efp, ebp, eso, eeo);

	debug("%s: display: x:%4d, hfp:%3d, hbp:%3d, hsw:%3d, hi:%d\n",
	      __func__, timing->hactive.typ, timing->hfront_porch.typ,
	      timing->hback_porch.typ, timing->hsync_len.typ, hs_pol);
	debug("%s: display: y:%4d, vfp:%3d, vbp:%3d, vsw:%3d, vi:%d\n",
	      __func__, timing->vactive.typ / div, timing->vfront_porch.typ,
	      timing->vback_porch.typ, timing->vsync_len.typ, vs_pol);
	debug("%s: display: offset vs:%d, ve:%d, es:%d, ee:%d\n",
	      __func__, vso, veo, eso, eeo);
	debug("%s: display: even   ef:%d, eb:%d, es:%d]\n",
	      __func__, efp, ebp, esw);
}

static void nx_display_set_mode(struct nx_display_priv *priv)
{
	nx_display_sync_format(priv);
	nx_display_sync_mode(priv);
	nx_display_sync_delay(priv);
}

static void nx_display_ovl_enable(struct nx_display_priv *priv)
{
	struct nx_display *dp = &priv->dp;
	struct nx_mlc_reg *reg = priv->mlc_base;
	int i;

	for (i = 0; i < dp->num_overlays; i++) {
		struct nx_overlay *ovl = screen_overlay(dp, i);
		int id = ovl->id;

		if (!ovl->enable)
			continue;

		debug("%s: id.%d %s\n", __func__, id,
		      ovl->rgb_layer ? "RGB" : "YUV");

		if (ovl->rgb_layer) {
			nx_mlc_set_layer_enable(reg, id, true);
			nx_mlc_set_layer_dirty(reg, id, true);
			continue;
		}

		/* video layer */
		nx_mlc_set_vid_line_buffer_power(reg, true);
		nx_mlc_set_layer_enable(reg, id, 1);
		nx_mlc_set_layer_dirty(reg, id, true);
	}

	nx_mlc_set_enable(reg, true);
	nx_mlc_set_dirty(reg);
}

static int nx_display_enable(struct nx_display_priv *priv)
{
	nx_display_ovl_enable(priv);

	nx_dpc_set_reg_flush(priv->dpc_base);

	/* dpc enable */
	nx_dpc_set_enable(priv->dpc_base, true);

	return 0;
}

static int nx_display_setup(struct nx_display_priv *priv)
{
	nx_display_set_clock(priv);
	nx_display_set_format(priv);
	nx_display_set_mode(priv);

	return 0;
}

static void nx_display_parse_config(ofnode node, struct nx_display *dp)
{
	struct nx_display_par *dpp = &dp->dpi.dpp;

	dpp->out_format = ofnode_read_u32_default(node, "out-format", 0);
	dpp->invert_field = ofnode_read_u32_default(node, "invert-field", 0);
	dpp->swap_rb = ofnode_read_u32_default(node, "swap-rb", 0);
	dpp->yc_order = ofnode_read_u32_default(node, "yc-order", 0);

	/* sync delay */
	dpp->delay_mask = ofnode_read_u32_default(node, "delay-mask", 0);
	dpp->d_rgb_pvd = ofnode_read_u32_default(node, "delay-rgb-pad", 0);
	dpp->d_hsync_cp1 = ofnode_read_u32_default(node, "delay-hs-cp1", 7);
	dpp->d_vsync_frame = ofnode_read_u32_default(node, "delay-vs-frame", 7);
	dpp->d_de_cp2 = ofnode_read_u32_default(node, "delay-de-cp2", 7);

	dpp->evfront_porch = ofnode_read_u32_default(node, "evfront-porch", 0);
	dpp->evback_porch = ofnode_read_u32_default(node, "evback-porch", 0);
	dpp->evsync_len = ofnode_read_u32_default(node, "evsync-len", 0);
	dpp->vstart_offs = ofnode_read_u32_default(node, "vsync-start-offs", 0);
	dpp->vend_offs = ofnode_read_u32_default(node, "vsync-end-offs", 0);
	dpp->evstart_offs = ofnode_read_u32_default(node,
						    "evsync-start-offs", 0);
	dpp->evend_offs = ofnode_read_u32_default(node, "evsync-end-offs", 0);

	debug("display config:\n");
	debug("\tfmt:0x%x, invert field:%d, swap RB:%d, yc:0x%x\n",
	      dpp->out_format, dpp->invert_field,
	      dpp->swap_rb, dpp->yc_order);
	debug("\tdelay  [rgb:%d, hs:%d, vs:%d, de cp2:%d]\n",
	      dpp->d_rgb_pvd, dpp->d_hsync_cp1,
	      dpp->d_vsync_frame, dpp->d_de_cp2);
	debug("\toffset [vs:%d, ve:%d, es:%d, ee:%d]\n",
	      dpp->vstart_offs, dpp->vend_offs,
	      dpp->evstart_offs, dpp->evend_offs);
	debug("\teven   [ef:%d, eb:%d, es:%d]\n",
	      dpp->evfront_porch, dpp->evback_porch, dpp->evsync_len);
}

static void nx_display_parse_screen(ofnode node, struct nx_display *dp)
{
	struct display_timing *timing = &dp->dpi.timing;

	dp->width = timing->hactive.typ;
	dp->height = timing->vactive.typ;

	dp->width = ofnode_read_u32_default(node, "width", dp->width);
	dp->height = ofnode_read_u32_default(node, "height", dp->height);
	dp->back_color = ofnode_read_u32_default(node, "back-color", 0);
	dp->video_prior = ofnode_read_u32_default(node, "video-priority", 0);
}

static void nx_display_parse_layer(ofnode node, struct nx_display *dp,
				   int id)
{
	struct display_timing *timing = &dp->dpi.timing;
	struct nx_overlay *ovl = screen_overlay(dp, id);

	ovl->width = timing->hactive.typ;
	ovl->height = timing->vactive.typ;

	ovl->primary = ofnode_read_bool(node, "primary");
	ovl->fb = ofnode_read_u32_default(node, "framebuffer", 0);
	ovl->left = ofnode_read_u32_default(node, "left", 0);
	ovl->top = ofnode_read_u32_default(node, "top", 0);
	ovl->width = ofnode_read_u32_default(node, "width", ovl->width);
	ovl->height = ofnode_read_u32_default(node, "height", ovl->height);
	ovl->bit_per_pixel = ofnode_read_u32_default(node, "bit-per-pixel", 32);
	ovl->format = ofnode_read_u32_default(node, "format", 0);
	ovl->alpha_depth = ofnode_read_u32_default(node, "alpha-depth", -1);
	ovl->tp_color = ofnode_read_u32_default(node, "tp-color", -1);
	ovl->id = id;
	ovl->rgb_layer = true;
	ovl->enable = true;

	if (ovl->primary)
		dp->primary = ovl->id;
}

static void nx_display_parse_overlays(ofnode parent, struct nx_display *dp)
{
	ofnode node;
	struct display_timing *timing = &dp->dpi.timing;
	int id = 0;

	dp->width = timing->hactive.typ;
	dp->height = timing->vactive.typ;
	dp->num_overlays = 0;

	ofnode_for_each_subnode(node, parent) {
		const char *name = ofnode_get_name(node);
		char *s;

		if (!strcmp(name, "screen"))
			nx_display_parse_screen(node, dp);

		if (!strncmp(name, "rgb_", strlen("rgb_"))) {
			s = strchr(name, '_');
			if (!s)
				continue;

			id = simple_strtol(++s, NULL, 10);
			if (id > MAX_OVERLAYS)
				continue;

			dp->num_overlays++;
			if (dp->num_overlays > MAX_OVERLAYS)
				break;

			nx_display_parse_layer(node, dp, id);
			id++;
		}
	}
}

static int nx_display_parse_display(struct udevice *dev)
{
	struct nx_display_priv *priv = dev_get_priv(dev);
	struct nx_display *dp = &priv->dp;
	struct display_timing *timing = &dp->dpi.timing;
	ofnode node;
	int ret;

	priv->mlc_base = (struct nx_mlc_reg *)devfdt_get_addr_index(dev, 0);
	priv->dpc_base = (struct nx_dpc_reg *)devfdt_get_addr_index(dev, 1);

	ret = clk_get_by_name(dev, "axi", &priv->clk_axi);
	if (ret) {
		printf("not found display clock 'axi'\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "clk_x1", &priv->clk_x1);
	if (ret) {
		printf("not found display clock 'clk_x1'\n");
		return ret;
	}

	ret = clk_get_by_name(dev, "clk_x2", &priv->clk_x2);
	if (ret) {
		printf("not found display clock 'clk_x2'\n");
		return ret;
	}

	if (ofnode_decode_display_timing(dev_ofnode(dev), 0, timing)) {
		printf("Failed to decode display timing\n");
		return -EINVAL;
	}

	node = ofnode_find_subnode(dev_ofnode(dev), "display-config");
	if (ofnode_valid(node))
		nx_display_parse_config(node, dp);

	node = ofnode_find_subnode(dev_ofnode(dev), "display-overlays");
	if (ofnode_valid(node))
		nx_display_parse_overlays(node, dp);

	return 0;
}

static int nx_display_parse_dt(struct udevice *dev)
{
	struct nx_display_priv *priv = dev_get_priv(dev);
	int ret;

	ret = nx_display_parse_display(dev);
	if (ret)
		return ret;

	ret = uclass_first_device(UCLASS_DISPLAY, &priv->lcd);
	if (ret) {
		printf("%s: Cannot find uclass display for '%s'\n",
		       __func__, dev->name);
		return ret;
	}

	ret = uclass_get_device(UCLASS_PANEL, 0, &priv->panel);
	if (ret) {
		ret = uclass_get_device(UCLASS_PANEL_BACKLIGHT,
					0, &priv->backlight);
		if (ret)
			debug("%s: Cannot find panel/backlight for '%s'\n",
			      __func__, dev->name);
	}

#ifdef CONFIG_VIDEO_BRIDGE
	uclass_get_device(UCLASS_VIDEO_BRIDGE, 0, &priv->bridge);
#endif
	return 0;
}

static int nx_display_start(struct nx_display_priv *priv)
{
	struct nx_display *dp = &priv->dp;
	struct nx_display_info *dpi = &dp->dpi;
	int ret;

	ret = nx_display_setup(priv);
	if (ret)
		return ret;

	/* enable UCLASS_DISPLAY */
	if (priv->lcd) {
		ret = display_enable(priv->lcd,
				     screen_primary(dp).bit_per_pixel,
				     &dpi->timing);
		if (ret) {
			debug("%s: Display enable ret: %d\n", __func__, ret);
			return ret;
		}
	}

	ret = nx_display_enable(priv);
	if (ret)
		return ret;

	/* backlight / pwm */
	if (priv->panel) {
		ret = panel_enable_backlight(priv->panel);
		if (ret) {
			printf("Failed backlight: %d\n", ret);
			return ret;
		}
	}

	if (priv->backlight)
		backlight_enable(priv->backlight);

#ifdef CONFIG_VIDEO_BRIDGE
	if (priv->bridge) {
		ret = video_bridge_set_backlight(priv->bridge, 80);
		if (ret) {
			printf("Failed video bridge: %d\n", ret);
			return ret;
		}
	}
#endif

	return ret;
}

static int nx_display_probe(struct udevice *dev)
{
	struct video_uc_platdata *plat = dev_get_uclass_platdata(dev);
	struct video_priv *v_priv = dev_get_uclass_priv(dev);
	struct nx_display_priv *priv = dev_get_priv(dev);
	struct nx_display *dp = &priv->dp;
	struct nx_overlay *ovl;
	int ret;

	ret = nx_display_parse_dt(dev);
	if (ret)
		return ret;

	ovl = screen_overlay(dp, dp->primary);

	v_priv->xsize = ovl->width;
	v_priv->ysize = ovl->height;

	switch (ovl->bit_per_pixel) {
	case 8:
		v_priv->bpix = VIDEO_BPP8;
		break;
	case 16:
		v_priv->bpix = VIDEO_BPP16;
		break;
	case 32:
		v_priv->bpix = VIDEO_BPP32;
		break;
	default:
		v_priv->bpix = ovl->bit_per_pixel;
		break;
	}

	if (ovl->fb) {
		plat->base = ovl->fb;
		gd->video_bottom = ovl->fb;
		gd->video_top = ovl->fb +
			(ovl->width * ovl->height * ovl->bit_per_pixel / 8);
	} else {
		ovl->fb = plat->base;
	}

	printf("FB: 0x%lx ~ 0x%lx (%ld), %dx%d, %dbpp screen.%d\n",
	       gd->video_bottom, gd->video_top,
	       gd->video_top - gd->video_bottom,
	       ovl->width, ovl->height, ovl->bit_per_pixel, ovl->id);

	return nx_display_start(priv);
}

static int nx_display_bind(struct udevice *dev)
{
	struct video_uc_platdata *uc_plat = dev_get_uclass_platdata(dev);

	/* This is the maximum panel size we expect to see */
	uc_plat->size = LCD_MAX_WIDTH * LCD_MAX_HEIGHT * LCD_MAX_BPP / 8;

	return 0;
}

static const struct video_ops nx_video_ops = {
};

static const struct udevice_id nx_display_ids[] = {
	{.compatible = "nexell,nxp3220-display", },
	{}
};

U_BOOT_DRIVER(nexell_display) = {
	.name = "nexell-display",
	.id = UCLASS_VIDEO,
	.of_match = nx_display_ids,
	.ops = &nx_video_ops,
	.bind = nx_display_bind,
	.probe = nx_display_probe,
	.priv_auto_alloc_size = sizeof(struct nx_display_priv),
};
