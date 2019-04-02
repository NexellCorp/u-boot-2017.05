/*
 * Copyright (C) 2018  Nexell Co., Ltd.
 *
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _DISPLAY_LOW_H_
#define _DISPLAY_LOW_H_

#include <common.h>
#include <linux/io.h>
#include <dt-bindings/video/nexell.h>

/*
 * Nexell multiple layer (MLC) specific struct
 */
struct nx_mlc_reg {
	u32 mlccontrolt;
	u32 mlcscreensize;
	u32 mlcbgcolor;
	struct {
		u32 mlcleftright;
		u32 mlctopbottom;
		u32 mlcinvalidleftright0;
		u32 mlcinvalidtopbottom0;
		u32 mlcinvalidleftright1;
		u32 mlcinvalidtopbottom1;
		u32 mlccontrol;
		u32 mlchstride;
		u32 mlcvstride;
		u32 mlctpcolor;
		u32 mlcinvcolor;
		u32 mlcaddress;
		u32 __reserved0;
	} mlcrgblayer[2];
	struct {
		u32 mlcleftright;
		u32 mlctopbottom;
		u32 mlccontrol;
		u32 mlcvstride;
		u32 mlctpcolor;

		u32 mlcinvcolor;
		u32 mlcaddress;
		u32 mlcaddresscb;
		u32 mlcaddresscr;
		u32 mlcvstridecb;
		u32 mlcvstridecr;
		u32 mlchscale;
		u32 mlcvscale;
		u32 mlcluenh;
		u32 mlcchenh[4];
	} mlcvideolayer;
	struct {
		u32 mlcleftright;
		u32 mlctopbottom;
		u32 mlcinvalidleftright0;
		u32 mlcinvalidtopbottom0;
		u32 mlcinvalidleftright1;
		u32 mlcinvalidtopbottom1;
		u32 mlccontrol;
		u32 mlchstride;
		u32 mlcvstride;
		u32 mlctpcolor;
		u32 mlcinvcolor;
		u32 mlcaddress;
	} mlcrgblayer2;
	u32 mlcpaletetable2;
	u32 mlcgammacont;
	u32 mlcrgammatablewrite;
	u32 mlcggammatablewrite;
	u32 mlcbgammatablewrite;
	u32 yuvlayergammatable_red;
	u32 yuvlayergammatable_green;
	u32 yuvlayergammatable_blue;

	u32 dimctrl;
	u32 dimlut0;
	u32 dimlut1;
	u32 dimbusyflag;
	u32 dimprdarrr0;
	u32 dimprdarrr1;
	u32 dimram0rddata;
	u32 dimram1rddata;
	u32 __reserved2[(0x3c0 - 0x12c) / 4];
	u32 mlcclkenb;
};

enum nx_pclk_mode {
	NX_PCLK_MODE_DYNAMIC = 0,
	NX_PCLK_MODE_ALWAYS = 1
};

enum nx_bclk_mode {
	NX_BCLK_MODE_DISABLE = 0,
	NX_BCLK_MODE_DYNAMIC = 2,
	NX_BCLK_MODE_ALWAYS = 3
};

enum nx_mlc_format_vid {
	NX_MLC_FMT_VID_420 = 0 << 16,
	NX_MLC_FMT_VID_422 = 1 << 16,
	NX_MLC_FMT_VID_444 = 3 << 16,
	NX_MLC_FMT_VID_YUYV = 2 << 16,
	NX_MLC_FMT_VID_422_CBCR = 4 << 16,
	NX_MLC_FMT_VID_420_CBCR = 5 << 16,
};

enum nx_mlc_priority_vid {
	NX_MLC_PRIORITY_VIDEO_1ST = 0,
	NX_MLC_PRIORITY_VIDEO_2ND = 1,
	NX_MLC_PRIORITY_VIDEO_3RD = 2,
	NX_MLC_PRIORITY_VIDEO_4TH = 3
};

#define	MAX_ALPHA_VALUE		15 /* 0: transparency, 15: opacity */

/* Nexell multiple layer (MLC) specific functions */
void nx_mlc_set_clock_pclk_mode(struct nx_mlc_reg *reg, enum nx_pclk_mode mode);
void nx_mlc_set_clock_bclk_mode(struct nx_mlc_reg *reg, enum nx_bclk_mode mode);

void nx_mlc_set_field_enable(struct nx_mlc_reg *reg, bool enb);
void nx_mlc_set_gamma_dither(struct nx_mlc_reg *reg, bool enb);
void nx_mlc_set_gamma_priority(struct nx_mlc_reg *reg, int priority);
void nx_mlc_set_power_mode(struct nx_mlc_reg *reg, bool power);
void nx_mlc_set_background(struct nx_mlc_reg *reg, u32 color);
void nx_mlc_set_video_priority(struct nx_mlc_reg *reg,
			       enum nx_mlc_priority_vid priority);
void nx_mlc_set_screen_size(struct nx_mlc_reg *reg,
			    int width, int height);
void nx_mlc_set_enable(struct nx_mlc_reg *reg, bool enb);
void nx_mlc_set_dirty(struct nx_mlc_reg *reg);

void nx_mlc_set_layer_alpha(struct nx_mlc_reg *reg,
			    int layer, u32 alpha, bool enb);
void nx_mlc_set_layer_lock_size(struct nx_mlc_reg *reg,
				int layer, u32 locksize);
void nx_mlc_set_layer_position(struct nx_mlc_reg *reg,
			       int layer, int sx, int sy, int ex, int ey);
void nx_mlc_set_layer_enable(struct nx_mlc_reg *reg, int layer, bool enb);
void nx_mlc_set_layer_dirty(struct nx_mlc_reg *reg,
			    int layer, bool enb);

void nx_mlc_set_rgb_gamma_power(struct nx_mlc_reg *reg,
				int red, int green, int blue);
void nx_mlc_set_rgb_gamma_enable(struct nx_mlc_reg *reg, bool enb);
void nx_mlc_set_rgb_transparency(struct nx_mlc_reg *reg,
				 int layer, u32 color, bool enb);
void nx_mlc_set_rgb_color_inv(struct nx_mlc_reg *reg,
			      int layer, u32 color, bool enb);
void nx_mlc_set_rgb_format(struct nx_mlc_reg *reg, int layer,
			   unsigned int format);
void nx_mlc_set_rgb_invalid_position(struct nx_mlc_reg *reg,
				     int layer, int region,
				     int sx, int sy, int ex, int ey,
				     bool enb);
void nx_mlc_set_rgb_stride(struct nx_mlc_reg *reg,
			   int layer, int hstride, int vstride);
void nx_mlc_set_rgb_address(struct nx_mlc_reg *reg,
			    int layer, u32 addr);
u32 nx_mlc_get_rgb_address(struct nx_mlc_reg *reg, int layer);

void nx_mlc_set_vid_line_buffer_power(struct nx_mlc_reg *reg, bool power);
void nx_mlc_set_vid_scale_filter(struct nx_mlc_reg *reg,
				 int bhlumaenb, int bhchromaenb,
				 int bvlumaenb, int bvchromaenb);
void nx_mlc_get_vid_scale_filter(struct nx_mlc_reg *reg,
				 int *bhlumaenb, int *bhchromaenb,
				 int *bvlumaenb, int *bvchromaenb);
void nx_mlc_set_vid_format(struct nx_mlc_reg *reg,
			   enum nx_mlc_format_vid format);
void nx_mlc_set_vid_scale(struct nx_mlc_reg *reg,
			  int sw, int sh, int dw, int dh,
			  int h_lu_enb, int h_ch_enb,
			  int v_lu_enb, int v_ch_enb);
void nx_mlc_set_vid_address_yuyv(struct nx_mlc_reg *reg,
				 u32 addr, int stride);
void nx_mlc_set_vid_stride(struct nx_mlc_reg *reg,
			   int lu_stride, int cb_stride, int cr_stride);
void nx_mlc_set_vid_address(struct nx_mlc_reg *reg,
			    u32 lu_addr, u32 cb_addr, u32 cr_addr);
void nx_mlc_wait_vblank(struct nx_mlc_reg *reg, int layer);

/*
 * Nexell display control(DPC) specific struct
 */
struct nx_dpc_reg {
	u32 ntsc_stata;
	u32 ntsc_ecmda;
	u32 ntsc_ecmdb;
	u32 ntsc_glk;
	u32 ntsc_sch;
	u32 ntsc_hue;
	u32 ntsc_sat;
	u32 ntsc_cont;
	u32 ntsc_bright;
	u32 ntsc_fsc_adjh;
	u32 ntsc_fsc_adjl;
	u32 ntsc_ecmdc;
	u32 ntsc_csdly;
	u32 __ntsc_reserved_0_[3];
	u32 ntsc_dacsel10;
	u32 ntsc_dacsel32;
	u32 ntsc_dacsel54;
	u32 ntsc_daclp;
	u32 ntsc_dacpd;
	u32 __ntsc_reserved_1_[(0x20 - 0x15)];
	u32 ntsc_icntl;
	u32 ntsc_hvoffst;
	u32 ntsc_hoffst;
	u32 ntsc_voffset;
	u32 ntsc_hsvso;
	u32 ntsc_hsob;
	u32 ntsc_hsoe;
	u32 ntsc_vsob;
	u32 ntsc_vsoe;
	u32 __reserved[(0xf8 / 4) - 0x29];
	u32 dpchtotal;
	u32 dpchswidth;
	u32 dpchastart;
	u32 dpchaend;
	u32 dpcvtotal;
	u32 dpcvswidth;
	u32 dpcvastart;
	u32 dpcvaend;
	u32 dpcctrl0;
	u32 dpcctrl1;
	u32 dpcevtotal;
	u32 dpcevswidth;
	u32 dpcevastart;
	u32 dpcevaend;
	u32 dpcctrl2;
	u32 dpcvseoffset;
	u32 dpcvssoffset;
	u32 dpcevseoffset;
	u32 dpcevssoffset;
	u32 dpcdelay0;
	u32 dpcupscalecon0;
	u32 dpcupscalecon1;
	u32 dpcupscalecon2;

	u32 dpcrnumgencon0;
	u32 dpcrnumgencon1;
	u32 dpcrnumgencon2;
	u32 dpcrndconformula_l;
	u32 dpcrndconformula_h;
	u32 dpcfdtaddr;
	u32 dpcfrdithervalue;
	u32 dpcfgdithervalue;
	u32 dpcfbdithervalue;
	u32 dpcdelay1;
	u32 dpcmputime0;
	u32 dpcmputime1;
	u32 dpcmpuwrdatal;
	u32 dpcmpuindex;
	u32 dpcmpustatus;
	u32 dpcmpudatah;
	u32 dpcmpurdatal;
	u32 dpcdummy12;
	u32 dpccmdbufferdatal;
	u32 dpccmdbufferdatah;
	u32 dpcpolctrl;
	u32 dpcpadposition[8];
	u32 dpcrgbmask[2];
	u32 dpcrgbshift;
	u32 dpcdataflush;
};

enum nx_dpc_ycorder {
	NX_DPC_YCORDER_CB_YCR_Y = 0,
	NX_DPC_YCORDER_CR_YCB_Y = 1,
	NX_DPC_YCORDER_YCBYCR = 2,
	NX_DPC_YCORDER_YCRYCB = 3
};

enum nx_dpc_dither {
	NX_DPC_DITHER_BYPASS = 0,
	NX_DPC_DITHER_4BIT = 1,
	NX_DPC_DITHER_5BIT = 2,
	NX_DPC_DITHER_6BIT = 3
};

enum nx_dpc_polarity {
	NX_DPC_POL_ACTIVEHIGH = 0,
	NX_DPC_POL_ACTIVELOW = 1
};

enum nx_dpc_padmux {
	NX_DPC_PADNUX_MLC = 0,
	NX_DPC_PADNUX_MPU = 1,
};

enum nx_dpc_padclk {
	NX_DPC_PADCLK_CLK = 0,
	NX_DPC_PADCLK_INVCLK = 1,
	NX_DPC_PADCLK_RESERVEDCLK = 2,
	NX_DPC_PADCLK_RESERVEDINVCLK = 3,
	NX_DPC_PADCLK_CLK_DIV2_0 = 4,
	NX_DPC_PADCLK_CLK_DIV2_90 = 5,
	NX_DPC_PADCLK_CLK_DIV2_180 = 6,
	NX_DPC_PADCLK_CLK_DIV2_270 = 7,
};

/* Nexell display control(DPC) specific functions */
void nx_dpc_clear_interrupt_pending_all(struct nx_dpc_reg *reg);
void nx_dpc_set_interrupt_enable_all(struct nx_dpc_reg *reg, bool enb);
int nx_dpc_get_interrupt_enable(struct nx_dpc_reg *reg);
int nx_dpc_get_interrupt_pending(struct nx_dpc_reg *reg);
void nx_dpc_set_clock_out_inv(struct nx_dpc_reg *reg,
			      int index, bool out_clk_inv);
void nx_dpc_set_mode(struct nx_dpc_reg *reg,
		     unsigned int outformat,
		     bool interlace,
		     bool rgbmode, bool swaprb,
		     bool embsync,
		     enum nx_dpc_ycorder ycorder);
void nx_dpc_set_hsync(struct nx_dpc_reg *reg,
		      u32 avwidth, u32 hsw, u32 hfp, u32 hbp, bool invhsync);
void nx_dpc_set_vsync(struct nx_dpc_reg *reg,
		      u32 avheight, u32 vsw, u32 vfp, u32 vbp,
		      u32 eavheight, u32 evsw, u32 evfp,
		      u32 evbp, bool invvsync);
void nx_dpc_set_vsync_offset(struct nx_dpc_reg *reg,
			     u32 vssoffset, u32 vseoffset,
			     u32 evssoffset, u32 evseoffset);
void nx_dpc_set_sync(struct nx_dpc_reg *reg, bool interlace,
		     int avwidth, int avheight,
		     int hsw, int hfp, int hbp, int vsw, int vfp, int vbp,
		     int vso, int veo,
		     enum nx_dpc_polarity field_pol,
		     enum nx_dpc_polarity hs_pol,
		     enum nx_dpc_polarity vs_pol,
		     int evsw, int evfp, int evbp,
		     int evso, int eveo);
void nx_dpc_set_delay(struct nx_dpc_reg *reg,
		      int delay_rgb_pvd, int delay_hs_cp1,
		      int delay_vs_frm, int delay_de_cp2);
void nx_dpc_set_dither(struct nx_dpc_reg *reg, enum nx_dpc_dither dither_r,
		       enum nx_dpc_dither dither_g,
		       enum nx_dpc_dither dither_b);
void nx_dpc_set_reg_flush(struct nx_dpc_reg *reg);
void nx_dpc_set_enable(struct nx_dpc_reg *reg, bool enb);
int  nx_dpc_get_enable(struct nx_dpc_reg *reg);

#endif
