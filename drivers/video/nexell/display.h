/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _NEXELL_DISPLAY_H_
#define _NEXELL_DISPLAY_H_

#include <dm.h>
#include "low.h"

#define	MAX_OVERLAYS	2

struct nx_display_par {
	/* sync format */
	unsigned int out_format;
	int invert_field;	/* 0= Normal Field 1: Invert Field */
	int swap_rb;
	unsigned int yc_order;	/* for CCIR output */
	/* sync delay  */
	int delay_mask;		/* if not 0, set defalut delays */
	int d_rgb_pvd;		/* delay value RGB/PVD signal   , 0 ~ 16, 0 */
	int d_hsync_cp1;	/* delay value HSYNC/CP1 signal , 0 ~ 63, 12 */
	int d_vsync_frame;	/* delay value VSYNC/FRAM signal, 0 ~ 63, 12 */
	int d_de_cp2;		/* delay value DE/CP2 signal    , 0 ~ 63, 12 */
	/* interlace sync */
	int evfront_porch;
	int evback_porch;
	int evsync_len;
	int vstart_offs;
	int vend_offs;
	int evstart_offs;
	int evend_offs;
};

struct nx_display_info {
	struct nx_display_par dpp;
	struct display_timing timing;
	bool mpu_lcd;
};

struct nx_overlay {
	int id;
	unsigned int fb;
	int primary;
	bool rgb_layer;
	unsigned int format;
	bool bgr_mode;
	int left;
	int top;
	int width;
	int height;
	int bit_per_pixel;
	int alpha_depth;
	int tp_color;
	bool enable;
};

struct nx_display {
	int width;
	int height;
	int video_prior;
	unsigned int back_color;
	unsigned int color_key;
	/* multi layer */
	struct nx_overlay ovl[MAX_OVERLAYS];
	int primary;
	int num_overlays;
	struct nx_display_info dpi;
};

#endif

