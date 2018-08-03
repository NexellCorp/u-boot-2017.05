/*
 * (C) Copyright 2018 Nexell
 * SangJong, Han <hans@nexell.co.kr>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <clk.h>
#include <usb.h>
#include <generic-phy.h>
#include <linux/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define	HIGH_USB_VER			0x0200		/* 2.0 */
#define	HIGH_MAX_PKT_SIZE_EP0		64
#define	HIGH_MAX_PKT_SIZE_EP1		512		/* bulk */
#define	HIGH_MAX_PKT_SIZE_EP2		512		/* bulk */

#define	FULL_USB_VER			0x0110		/* 1.1 */
#define	FULL_MAX_PKT_SIZE_EP0		8		/* Do not modify */
#define	FULL_MAX_PKT_SIZE_EP1		64		/* bulk */
#define	FULL_MAX_PKT_SIZE_EP2		64		/* bulk */

#define RX_FIFO_SIZE			512
#define NPTX_FIFO_START_ADDR		RX_FIFO_SIZE
#define NPTX_FIFO_SIZE			512
#define PTX_FIFO_SIZE			512

#define	DEVICE_DESCRIPTOR_SIZE		(18)
#define	CONFIG_DESCRIPTOR_SIZE		(9 + 9 + 7 + 7)

/*
 * NXP3220: 0x2375, S5PXX18: 0x04e8, Samsung Vendor ID : 0x04e8
 * NXP3220: 0x3220, S5PXX18: 0x1234, Nexell Product ID : 0x1234
 */
#define VENDORID			0x2375
#define PRODUCTID			0x3220

/* SPEC1.1 */

/* configuration descriptor: bmAttributes */
enum config_attributes {
	CONF_ATTR_DEFAULT		= 0x80,
	CONF_ATTR_REMOTE_WAKEUP		= 0x20,
	CONF_ATTR_SELFPOWERED		= 0x40
};

/* endpoint descriptor */
enum endpoint_attributes {
	EP_ADDR_IN			= 0x80,
	EP_ADDR_OUT			= 0x00,
	EP_ATTR_CONTROL			= 0x00,
	EP_ATTR_ISOCHRONOUS		= 0x01,
	EP_ATTR_BULK			= 0x02,
	EP_ATTR_INTERRUPT		= 0x03
};

/* Standard bRequest codes */
enum standard_request_code {
	STANDARD_GET_STATUS		= 0,
	STANDARD_CLEAR_FEATURE		= 1,
	STANDARD_RESERVED_1		= 2,
	STANDARD_SET_FEATURE		= 3,
	STANDARD_RESERVED_2		= 4,
	STANDARD_SET_ADDRESS		= 5,
	STANDARD_GET_DESCRIPTOR		= 6,
	STANDARD_SET_DESCRIPTOR		= 7,
	STANDARD_GET_CONFIGURATION	= 8,
	STANDARD_SET_CONFIGURATION	= 9,
	STANDARD_GET_INTERFACE		= 10,
	STANDARD_SET_INTERFACE		= 11,
	STANDARD_SYNCH_FRAME		= 12
};

enum descriptortype {
	DESCRIPTORTYPE_DEVICE		= 1,
	DESCRIPTORTYPE_CONFIGURATION	= 2,
	DESCRIPTORTYPE_STRING		= 3,
	DESCRIPTORTYPE_INTERFACE	= 4,
	DESCRIPTORTYPE_ENDPOINT		= 5
};

#define CONTROL_EP			0
#define BULK_IN_EP			1
#define BULK_OUT_EP			2

/**
 * USB2.0 HS OTG
 **/
struct nx_usb_otg_gcsr_reg {
	u32 gotgctl;			/* 0x000 R/W OTG Control and Status
					   Register */
	u32 gotgint;			/* 0x004 R/W OTG Interrupt Register */
	u32 gahbcfg;			/* 0x008 R/W Core AHB Configuration
					   Register */
	u32 gusbcfg;			/* 0x00C R/W Core USB Configuration
					   Register */
	u32 grstctl;			/* 0x010 R/W Core Reset Register */
	u32 gintsts;			/* 0x014 R/W Core Interrupt Register */
	u32 gintmsk;			/* 0x018 R/W Core Interrupt Mask
					   Register */
	u32 grxstsr;			/* 0x01C R   Receive Status Debug Read
					   Register */
	u32 grxstsp;			/* 0x020 R/W Receive Status Debug Pop
					   Register */
	u32 grxfsiz;			/* 0x024 R/W Receive FIFO Size Register
					   */
	u32 gnptxfsiz;			/* 0x028 R   Non-Periodic Transmit FIFO
					   Size Register */
	u32 gnptxsts;			/* 0x02C R/W Non-Periodic Transmit
					   FIFO/Queue Status Register */
	u32 reserved0;			/* 0x030     Reserved */
	u32 reserved1;			/* 0x034     Reserved */
	u32 reserved2;			/* 0x038     Reserved */
	u32 guid;			/* 0x03C R   User ID Register */
	u32 gsnpsid;			/* 0x040 R   Synopsys ID Register */
	u32 ghwcfg1;			/* 0x044 R   User HW Config1 Register */
	u32 ghwcfg2;			/* 0x048 R   User HW Config2 Register */
	u32 ghwcfg3;			/* 0x04C R   User HW Config3 Register */
	u32 ghwcfg4;			/* 0x050 R   User HW Config4 Register */
	u32 glpmcfg;			/* 0x054 R/W Core LPM Configuration
					   Register */
	u32 reserved3[(0x100-0x058)/4];	/* 0x058 ~ 0x0FC */
	u32 hptxfsiz;			/* 0x100 R/W Host Periodic Transmit FIFO
					   Size Register */
	u32 dieptxf[15];		/* 0x104 ~ 0x13C R/W Device IN Endpoint
					   Transmit FIFO Size Register */
	u32 reserved4[(0x400-0x140)/4];	/* 0x140 ~ 0x3FC */
};

struct nx_usb_otg_host_channel_reg {
	u32 hcchar;			/* 0xn00 R/W Host Channel-n
					   Characteristics Register */
	u32 hcsplt;			/* 0xn04 R/W Host Channel-n Split
					   Control Register */
	u32 hcint;			/* 0xn08 R/W Host Channel-n Interrupt
					   Register */
	u32 hcintmsk;			/* 0xn0C R/W Host Channel-n Interrupt
					   Mask Register */
	u32 hctsiz;			/* 0xn10 R/W Host Channel-n Transfer
					   Size Register */
	u32 hcdma;			/* 0xn14 R/W Host Channel-n DMA Address
					   Register */
	u32 reserved[2];		/* 0xn18, 0xn1C Reserved */
};

struct nx_usb_otg_hmcsr_reg {
	u32 hcfg;			/* 0x400 R/W Host Configuration Register
					   */
	u32 hfir;			/* 0x404 R/W Host Frame Interval
					   Register */
	u32 hfnum;			/* 0x408 R   Host Frame Number/Frame
					   Time Remaining Register */
	u32 reserved0;			/* 0x40C     Reserved */
	u32 hptxsts;			/* 0x410 R/W Host Periodic Transmit
					   FIFO/Queue Status Register */
	u32 haint;			/* 0x414 R   Host All Channels Interrupt
					   Register */
	u32 haintmsk;			/* 0x418 R/W Host All Channels Interrupt
					   Mask Register */
	u32 reserved1[(0x440-0x41C)/4];	/* 0x41C ~ 0x43C Reserved */
	u32 hprt;			/* 0x440 R/W Host Port Control and
					   Status Register */
	u32 reserved2[(0x500-0x444)/4];	/* 0x444 ~ 0x4FC Reserved */
	struct nx_usb_otg_host_channel_reg hcc[16];/* 0x500 ~ 0x6FC */
	u32 reserved3[(0x800-0x700)/4];	/* 0x700 ~ 0x7FC */
};

struct nx_usb_otg_device_epi_reg {
	u32 diepctl;			/* 0xn00 R/W Device Control IN endpoint
					   n Control Register */
	u32 reserved0;			/* 0xn04     Reserved */
	u32 diepint;			/* 0xn08 R/W Device Endpoint-n Interrupt
					   Register */
	u32 reserved1;			/* 0xn0C     Reserved */
	u32 dieptsiz;			/* 0xn10 R/W Device Endpoint-n Transfer
					   Size Register */
	u32 diepdma;			/* 0xn14 R/W Device Endpoint-n DMA
					   Address Register */
	u32 dtxfsts;			/* 0xn18 R   Device IN Endpoint Transmit
					   FIFO Status Register */
	u32 diepdmab;			/* 0xn1C R   Device Endpoint-n DMA
					   Buffer Address Register */
};

struct nx_usb_otg_device_ep0_reg {
	u32 doepctl;			/* 0xn00 R/W Device Control OUT endpoint
					   n Control Register */
	u32 reserved0;			/* 0xn04     Reserved */
	u32 doepint;			/* 0xn08 R/W Device Endpoint-n Interrupt
					   Register */
	u32 reserved1;			/* 0xn0C     Reserved */
	u32 doeptsiz;			/* 0xn10 R/W Device Endpoint-n Transfer
					   Size Register */
	u32 doepdma;			/* 0xn14 R/W Device Endpoint-n DMA
					   Address Register */
	u32 reserved2;			/* 0xn18     Reserved */
	u32 doepdmab;			/* 0xn1C R   Device Endpoint-n DMA
					   Buffer Address Register */
};

struct nx_usb_otg_dmcsr_reg {
	u32 dcfg;			/* 0x800 R/W Device Configuration
					   Register */
	u32 dctl;			/* 0x804 R/W Device Control Register */
	u32 dsts;			/* 0x808 R   Device Status Register */
	u32 reserved0;			/* 0x80C     Reserved */
	u32 diepmsk;			/* 0x810 R/W Device IN Endpoint Common
					   Interrupt Mask Register */
	u32 doepmsk;			/* 0x814 R/W Device OUT Endpoint Common
					   Interrupt Mask Register */
	u32 daint;			/* 0x818 R   Device All Endpoints
					   Interrupt Register */
	u32 daintmsk;			/* 0x81C R/W Device All Endpoints
					   Interrupt Mask Register */
	u32 reserved1;			/* 0x820     Reserved */
	u32 reserved2;			/* 0x824     Reserved */
	u32 dvbusdis;			/* 0x828 R/W Device VBUS Discharge Time
					   Register */
	u32 dvbuspulse;			/* 0x82C R/W Device VBUS Pulsing Time
					   Register */
	u32 dthrctl;			/* 0x830 R/W Device Threshold Control
					   Register */
	u32 diepempmsk;			/* 0x834 R/W Device IN Endpoint FIFO
					   Empty Interrupt Mask Register */
	u32 reserved3;			/* 0x838     Reserved */
	u32 reserved4;			/* 0x83C     Reserved */
	u32 reserved5[0x10];		/* 0x840 ~ 0x87C    Reserved */
	u32 reserved6[0x10];		/* 0x880 ~ 0x8BC    Reserved */
	u32 reserved7[0x10];		/* 0x8C0 ~ 0x8FC    Reserved */
	struct nx_usb_otg_device_epi_reg depir[16]; /* 0x900 ~ 0xAFC */
	struct nx_usb_otg_device_ep0_reg depor[16]; /* 0xB00 ~ 0xCFC */
};

struct nx_usb_otg_reg {
	struct nx_usb_otg_gcsr_reg  gcsr;/* 0x0000 ~ 0x03FC */
	struct nx_usb_otg_hmcsr_reg hcsr;/* 0x0400 ~ 0x07FC */
	struct nx_usb_otg_dmcsr_reg dcsr;/* 0x0800 ~ 0x0CFC */
	u32 reserved0[(0xe00-0xd00)/4];	/* 0x0D00 ~ 0x0DFC	Reserved */
	u32 pcgcctl;			/* 0x0E00 R/W Power and Clock Gating
					   Control Register */
	u32 reserved1[(0x1000-0xe04)/4];/* 0x0E04 ~ 0x0FFC	Reserved */
	u32 epfifo[15][1024];		/* 0x1000 ~ 0xFFFC Endpoint Fifo */
	/*u32 epfifo[16][1024];*/	/* 0x1000 ~ 0x10FFC Endpoint Fifo */
	/*u32 reserved2[(0x20000-0x11000)/4];*//* 0x11000 ~ 0x20000 Reserved */
	/*u32 debugfifo[0x8000];*/	/* 0x20000 ~ 0x3FFFC Debug Purpose
					   Direct Fifo Acess Register */
};

/* definitions related to CSR setting */
/* USB Global Interrupt Status register(GINTSTS) setting value */
#define WKUP_INT			(1u<<31)
#define OEP_INT				(1<<19)
#define IEP_INT				(1<<18)
#define ENUM_DONE			(1<<13)
#define USB_RST				(1<<12)
#define USB_SUSP			(1<<11)
#define RXF_LVL				(1<<4)

/* NX_OTG_GOTGCTL*/
#define B_SESSION_VALID			(0x1<<19)
#define A_SESSION_VALID			(0x1<<18)

/* NX_OTG_GAHBCFG*/
#define PTXFE_HALF			(0<<8)
#define PTXFE_ZERO			(1<<8)
#define NPTXFE_HALF			(0<<7)
#define NPTXFE_ZERO			(1<<7)
#define MODE_SLAVE			(0<<5)
#define MODE_DMA			(1<<5)
#define BURST_SINGLE			(0<<1)
#define BURST_INCR			(1<<1)
#define BURST_INCR4			(3<<1)
#define BURST_INCR8			(5<<1)
#define BURST_INCR16			(7<<1)
#define GBL_INT_UNMASK			(1<<0)
#define GBL_INT_MASK			(0<<0)

/* NX_OTG_GRSTCTL*/
#define AHB_MASTER_IDLE			(1u<<31)
#define CORE_SOFT_RESET			(0x1<<0)

/* NX_OTG_GINTSTS/NX_OTG_GINTMSK core interrupt register */
#define INT_RESUME			(1u<<31)
#define INT_DISCONN			(0x1<<29)
#define INT_CONN_ID_STS_CNG		(0x1<<28)
#define INT_OUT_EP			(0x1<<19)
#define INT_IN_EP			(0x1<<18)
#define INT_ENUMDONE			(0x1<<13)
#define INT_RESET			(0x1<<12)
#define INT_SUSPEND			(0x1<<11)
#define INT_TX_FIFO_EMPTY		(0x1<<5)
#define INT_RX_FIFO_NOT_EMPTY		(0x1<<4)
#define INT_SOF				(0x1<<3)
#define INT_DEV_MODE			(0x0<<0)
#define INT_HOST_MODE			(0x1<<1)

/* NX_OTG_GRXSTSP STATUS*/
#define GLOBAL_OUT_NAK			(0x1<<17)
#define OUT_PKT_RECEIVED		(0x2<<17)
#define OUT_TRNASFER_COMPLETED		(0x3<<17)
#define SETUP_TRANSACTION_COMPLETED	(0x4<<17)
#define SETUP_PKT_RECEIVED		(0x6<<17)

/* NX_OTG_DCTL device control register */
#define NORMAL_OPERATION		(0x1<<0)
#define SOFT_DISCONNECT			(0x1<<1)

/* NX_OTG_DAINT device all endpoint interrupt register */
#define INT_IN_EP0			(0x1<<0)
#define INT_IN_EP1			(0x1<<1)
#define INT_IN_EP3			(0x1<<3)
#define INT_OUT_EP0			(0x1<<16)
#define INT_OUT_EP2			(0x1<<18)
#define INT_OUT_EP4			(0x1<<20)

/* NX_OTG_DIEPCTL0/NX_OTG_DOEPCTL0 */
#define DEPCTL_EPENA			(0x1u<<31)
#define DEPCTL_EPDIS			(0x1<<30)
#define DEPCTL_SNAK			(0x1<<27)
#define DEPCTL_CNAK			(0x1<<26)
#define DEPCTL_STALL			(0x1<<21)
#define DEPCTL_ISO_TYPE			(EP_TYPE_ISOCHRONOUS<<18)
#define DEPCTL_BULK_TYPE		(EP_TYPE_BULK<<18)
#define DEPCTL_INTR_TYPE		(EP_TYPE_INTERRUPT<<18)
#define DEPCTL_USBACTEP			(0x1<<15)

/*ep0 enable, clear nak, next ep0, max 64byte */
#define EPEN_CNAK_EP0_64 (DEPCTL_EPENA|DEPCTL_CNAK|(CONTROL_EP<<11)|(0<<0))

/*ep0 enable, clear nak, next ep0, 8byte */
#define EPEN_CNAK_EP0_8 (DEPCTL_EPENA|DEPCTL_CNAK|(CONTROL_EP<<11)|(3<<0))

/* DIEPCTLn/DOEPCTLn */
#define BACK2BACK_SETUP_RECEIVED	(0x1<<6)
#define INTKN_TXFEMP			(0x1<<4)
#define NON_ISO_IN_EP_TIMEOUT		(0x1<<3)
#define CTRL_OUT_EP_SETUP_PHASE_DONE	(0x1<<3)
#define AHB_ERROR			(0x1<<2)
#define TRANSFER_DONE			(0x1<<0)

struct nx_setup_packet {
	u8 requesttype;
	u8 request;
	u16 value;
	u16 index;
	u16 length;
};

enum usb_speed {
	USB_HIGH,
	USB_FULL,
	USB_LOW
	/*,0xFFFFFFFFUL*/
};

enum ep_type {
	EP_TYPE_CONTROL, EP_TYPE_ISOCHRONOUS, EP_TYPE_BULK, EP_TYPE_INTERRUPT
};

/* EP0 state */
enum ep0_state {
	EP0_STATE_INIT			= 0,
	EP0_STATE_GET_DSCPT		= 1,
	EP0_STATE_GET_INTERFACE		= 2,
	EP0_STATE_GET_CONFIG		= 3,
	EP0_STATE_GET_STATUS		= 4
};

struct nx_usbboot_status {
	bool		downloading;
	u8		*rx_buf_addr;
	s32		rx_size;
	u32		ep0_state;
	enum usb_speed	speed;
	u32		ctrl_max_pktsize;
	u32		bulkin_max_pktsize;
	u32		bulkout_max_pktsize;
	u8		*current_ptr;
	u32		current_fifo_size;
	u32		remain_size;
	u32		up_addr;
	u32		up_size;
	u8		*up_ptr;
	u8		cur_config;
	u8		cur_interface;
	u8		cur_setting;
	u8		reserved;
	const u8	*device_descriptor;
	const u8	*config_descriptor;
} __aligned(4);

static u8 gs_device_descriptor_fs[DEVICE_DESCRIPTOR_SIZE]
	__aligned(4) = {
	18,				/* 0 desc size */
	(u8)(DESCRIPTORTYPE_DEVICE),	/* 1 desc type (DEVICE) */
	(u8)(FULL_USB_VER % 0x100),	/* 2 USB release */
	(u8)(FULL_USB_VER / 0x100),	/* 3 => 1.00 */
	0xFF,				/* 4 class */
	0xFF,				/* 5 subclass */
	0xFF,				/* 6 protocol */
	(u8)FULL_MAX_PKT_SIZE_EP0,	/* 7 max pack size */
	(u8)(VENDORID % 0x100),		/* 8 vendor ID LSB */
	(u8)(VENDORID / 0x100),		/* 9 vendor ID MSB */
	(u8)(PRODUCTID % 0x100),	/* 10 product ID LSB
					   (second product) */
	(u8)(PRODUCTID / 0x100),	/* 11 product ID MSB */
	0x00,				/* 12 device release LSB */
	0x00,				/* 13 device release MSB */
	0x00,				/* 14 manufacturer string desc index */
	0x00,				/* 15 product string desc index */
	0x00,				/* 16 serial num string desc index */
	0x01				/* 17 num of possible configurations */
};

static u8 gs_device_descriptor_hs[DEVICE_DESCRIPTOR_SIZE]
	__aligned(4) = {
	18,				/* 0 desc size */
	(u8)(DESCRIPTORTYPE_DEVICE),	/* 1 desc type (DEVICE) */
	(u8)(HIGH_USB_VER % 0x100),	/*  2 USB release */
	(u8)(HIGH_USB_VER / 0x100),	/* 3 => 1.00 */
	0xFF,				/* 4 class */
	0xFF,				/* 5 subclass */
	0xFF,				/* 6 protocol */
	(u8)HIGH_MAX_PKT_SIZE_EP0,	/* 7 max pack size */
	(u8)(VENDORID	% 0x100),	/* 8 vendor ID LSB */
	(u8)(VENDORID	/ 0x100),	/* 9 vendor ID MSB */
	(u8)(PRODUCTID % 0x100),	/* 10 product ID LSB
					   (second product) */
	(u8)(PRODUCTID / 0x100),	/* 11 product ID MSB */
	0x00,				/* 12 device release LSB */
	0x00,				/* 13 device release MSB */
	0x00,				/* 14 manufacturer string desc index */
	0x00,				/* 15 product string desc index */
	0x00,				/* 16 serial num string desc index */
	0x01				/* 17 num of possible configurations */
};

static const u8	gs_config_descriptor_fs[CONFIG_DESCRIPTOR_SIZE]
	__aligned(4) = {
	/* Configuration Descriptor */
	0x09,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_CONFIGURATION),	/* 1 desc type (CONFIGURATION)
						 */
	(u8)(CONFIG_DESCRIPTOR_SIZE % 0x100),	/* 2 total length of data
						   returned LSB */
	(u8)(CONFIG_DESCRIPTOR_SIZE / 0x100),	/* 3 total length of data
						   returned MSB */
	0x01,					/* 4 num of interfaces */
	0x01,					/* 5 value to select config (1
						   for now) */
	0x00,					/* 6 index of string desc ( 0
						   for now) */
	CONF_ATTR_DEFAULT|CONF_ATTR_SELFPOWERED,/* 7 bus powered */
	25,					/* 8 max power, 50mA for now */

	/* Interface Decriptor */
	0x09,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_INTERFACE),		/* 1 desc type (INTERFACE) */
	0x00,					/* 2 interface index. */
	0x00,					/* 3 value for alternate setting
						   */
	0x02,					/* 4 bNumEndpoints (number
						   endpoints used, excluding
						   EP0) */
	0xFF,					/* 5 */
	0xFF,					/* 6 */
	0xFF,					/* 7 */
	0x00,					/* 8 string index, */

	/* Endpoint descriptor (EP 1 Bulk IN) */
	0x07,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_ENDPOINT),		/* 1 desc type (ENDPOINT) */
	BULK_IN_EP|EP_ADDR_IN,			/* 2 endpoint address: endpoint
						   1, IN */
	EP_ATTR_BULK,				/* 3 endpoint attributes: Bulk
						 */
	(u8)(FULL_MAX_PKT_SIZE_EP1 % 0x100),	/* 4 max packet size LSB */
	(u8)(FULL_MAX_PKT_SIZE_EP1 / 0x100),	/* 5 max packet size MSB */
	0x00,					/* 6 polling interval
						   (4ms/bit=time,500ms) */

	/* Endpoint descriptor (EP 2 Bulk OUT) */
	0x07,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_ENDPOINT),		/* 1 desc type (ENDPOINT) */
	BULK_OUT_EP|EP_ADDR_OUT,		/* 2 endpoint address: endpoint
						   2, OUT */
	EP_ATTR_BULK,				/* 3 endpoint attributes: Bulk
						 */
	(u8)(FULL_MAX_PKT_SIZE_EP2 % 0x100),	/* 4 max packet size LSB */
	(u8)(FULL_MAX_PKT_SIZE_EP2 / 0x100),	/* 5 max packet size MSB */
	0x00					/* 6 polling interval
						   (4ms/bit=time,500ms) */
};

static const u8	gs_config_descriptor_hs[CONFIG_DESCRIPTOR_SIZE]
	__aligned(4) = {
	/* Configuration Descriptor */
	0x09,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_CONFIGURATION),	/* 1 desc type (CONFIGURATION)
						 */
	(u8)(CONFIG_DESCRIPTOR_SIZE % 0x100),	/* 2 total length of data
						   returned LSB */
	(u8)(CONFIG_DESCRIPTOR_SIZE / 0x100),	/* 3 total length of data
						   returned MSB */
	0x01,					/* 4 num of interfaces */
	0x01,					/* 5 value to select config (1
						   for now) */
	0x00,					/* 6 index of string desc ( 0
						   for now) */
	CONF_ATTR_DEFAULT|CONF_ATTR_SELFPOWERED,/* 7 bus powered */
	25,					/* 8 max power, 50mA for now */

	/* Interface Decriptor */
	0x09,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_INTERFACE),		/* 1 desc type (INTERFACE) */
	0x00,					/* 2 interface index. */
	0x00,					/* 3 value for alternate setting
						   */
	0x02,					/* 4 bNumEndpoints (number
						   endpoints used, excluding
						   EP0) */
	0xFF,					/* 5 */
	0xFF,					/* 6 */
	0xFF,					/* 7 */
	0x00,					/* 8 string index, */

	/* Endpoint descriptor (EP 1 Bulk IN) */
	0x07,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_ENDPOINT),		/* 1 desc type (ENDPOINT) */
	BULK_IN_EP|EP_ADDR_IN,			/* 2 endpoint address: endpoint
						   1, IN */
	EP_ATTR_BULK,				/* 3 endpoint attributes: Bulk
						 */
	(u8)(HIGH_MAX_PKT_SIZE_EP1 % 0x100),	/* 4 max packet size LSB */
	(u8)(HIGH_MAX_PKT_SIZE_EP1 / 0x100),	/* 5 max packet size MSB */
	0x00,					/* 6 polling interval
						   (4ms/bit=time,500ms) */
	/* Endpoint descriptor (EP 2 Bulk OUT) */
	0x07,					/* 0 desc size */
	(u8)(DESCRIPTORTYPE_ENDPOINT),		/* 1 desc type (ENDPOINT) */
	BULK_OUT_EP|EP_ADDR_OUT,		/* 2 endpoint address: endpoint
						   2, OUT */
	EP_ATTR_BULK,				/* 3 endpoint attributes: Bulk
						 */
	(u8)(HIGH_MAX_PKT_SIZE_EP2 % 0x100),	/* 4 max packet size LSB */
	(u8)(HIGH_MAX_PKT_SIZE_EP2 / 0x100),	/* 5 max packet size MSB */
	0x00					/* 6 polling interval
						   (4ms/bit=time,500ms) */
};

/* @brief ECID Module's Register List */
struct nx_ecid_reg {
	u32 chipname[12];    /* 0x000 */
	u32 _rev0;            /* 0x030 */
	u32 guid[4];          /* 0x034 */
	u32 ec0;              /* 0x044 */
	u32 _rev1;            /* 0x048 */
	u32 ec2;              /* 0x04C */
	u32 blow[3];          /* 0x050 */
	u32 _rev2;            /* 0x05C */
	u32 blowd[4];         /* 0x060 */
	u32 _rev3[36];        /* 0x070 */
	u32 ecid[4];          /* 0x100 */
	u32 sbootkey0[4];     /* 0x110 */
	u32 sbootkey1[4];     /* 0x120 */
	u32 _rev4[8];         /* 0x130 */
	u32 sboothash0[8];    /* 0x150 */
	u32 _rev5[8];         /* 0x170 */
	u32 sboothash1[8];    /* 0x190 */
	u32 sboothash2[8];    /* 0x1B0 */
	u32 sjtag[4];         /* 0x1D0 */
	u32 anti_rb[4];       /* 0x1E0 */
	u32 efuse_cfg;        /* 0x1F0 */
	u32 efuse_prot;       /* 0x1F4 */
	u32 _rev6[2];         /* 0x1F8 */
	u32 boot_cfg;         /* 0x200 */
	u32 _rev7[3];         /* 0x204 */
	u32 back_enc_ek[8];   /* 0x210 */
	u32 root_enc_key[8];  /* 0x230 */
	u32 cm0_sboot_key[16];/* 0x250 */
	u32 root_priv_key[17];/* 0x290 */
	u32 _rev8[11];        /* 0x2D4 */
	u32 puf[136];         /* 0x300 */
	u32 puf_cfg;          /* 0x520 */
	u32 cm0_anti_rb;      /* 0x524 */
	u32 cm0_anti_rb_cfg;  /* 0x528 */
	u32 _rev9;            /* 0x52C */
	u32 hpm_ids[4];       /* 0x530 */
};

struct nx_usbdown_priv {
	void __iomem *reg_otg;
	void __iomem *reg_ecid;
	struct nx_usbboot_status ustatus;
	struct phy phy;
	struct clk clk;
};

static void nx_usb_get_usbid(void __iomem *ecid, u16 *vid, u16 *pid)
{
	struct nx_ecid_reg *reg = ecid;
	u32 id = readl(&reg->ecid[3]);

	if (!id) {   /* ecid is not burned */
		*vid = VENDORID;
		*pid = PRODUCTID;
		debug("\nECID Null!!\nVID %x, PID %x\n", *vid, *pid);
	} else {
		*vid = (id >> 16)&0xFFFF;
		*pid = (id >> 0)&0xFFFF;
		debug("VID %x, PID %x\n", *vid, *pid);
	}
}

static void nx_usb_write_in_fifo(struct nx_usb_otg_reg *reg_otg,
				 u32 ep, u8 *buf, s32 num)
{
	s32 i;
	u32 *dwbuf = (u32 *)buf;		/* assume all data ptr is 4
						   bytes aligned */
	for (i = 0; i < (num + 3) / 4; i++)
		writel(dwbuf[i], &reg_otg->epfifo[ep][0]);
}

static void nx_usb_read_out_fifo(struct nx_usb_otg_reg *reg_otg,
				 u32 ep, u8 *buf, s32 num)
{
	s32 i;
	u32 *dwbuf = (u32 *)buf;
	for (i = 0; i < (num + 3) / 4; i++)
		dwbuf[i] = readl(&reg_otg->epfifo[ep][0]);
}

static void nx_usb_ep0_int_hndlr(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	struct nx_setup_packet *setup_packet;
	u32 buf[2];
	u16 addr;

	setup_packet = (struct nx_setup_packet *)buf;
	debug("Event EP0\n");
	dmb();

	if (ustatus->ep0_state == EP0_STATE_INIT) {
		buf[0] = readl(&reg_otg->epfifo[CONTROL_EP][0]);
		buf[1] = readl(&reg_otg->epfifo[CONTROL_EP][0]);

		debug("Req: %x  %x %d %x %d\n",
		      setup_packet->requesttype,
		      setup_packet->request,
		      setup_packet->value,
		      setup_packet->index,
		      setup_packet->length);

		switch (setup_packet->request) {
		case STANDARD_SET_ADDRESS:
			/* Set Address Update bit */
			addr = (setup_packet->value & 0xFF);
			debug("STANDARD_SET_ADDRESS: %x ", addr);
			writel(1 << 18 | addr << 4 |
				ustatus->speed << 0,
				&reg_otg->dcsr.dcfg);
			ustatus->ep0_state = EP0_STATE_INIT;

			break;

		case STANDARD_SET_DESCRIPTOR:
			debug("STANDARD_SET_DESCRIPTOR\n");
			break;

		case STANDARD_SET_CONFIGURATION:
			debug("STANDARD_SET_CONFIGURATION\n");
			/* Configuration value in configuration descriptor */
			ustatus->cur_config = setup_packet->value;
			ustatus->ep0_state = EP0_STATE_INIT;
			break;

		case STANDARD_GET_CONFIGURATION:
			debug("STANDARD_GET_CONFIGURATION\n");
			writel((1<<19)|(1<<0),
			       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);
			/*ep0 enable, clear nak, next ep0, 8byte */
			writel(EPEN_CNAK_EP0_8,
			       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
			writel(ustatus->cur_config,
			       &reg_otg->epfifo[CONTROL_EP][0]);
			ustatus->ep0_state = EP0_STATE_INIT;
			break;

		case STANDARD_GET_DESCRIPTOR:
			debug("STANDARD_GET_DESCRIPTOR :");
			ustatus->remain_size =
				(u32)setup_packet->length;
			debug("0");
			switch (setup_packet->value>>8) {
			case DESCRIPTORTYPE_DEVICE:
				ustatus->current_ptr =
					(u8 *)ustatus->device_descriptor;
				debug("1");
				ustatus->current_fifo_size =
					ustatus->ctrl_max_pktsize;
				debug("2");
				if (ustatus->remain_size
				   > DEVICE_DESCRIPTOR_SIZE)
					ustatus->remain_size =
						DEVICE_DESCRIPTOR_SIZE;
				ustatus->ep0_state = EP0_STATE_GET_DSCPT;
				debug("3");
				break;

			case DESCRIPTORTYPE_CONFIGURATION:
				ustatus->current_ptr =
					(u8 *)ustatus->config_descriptor;
				debug("4");
				ustatus->current_fifo_size =
					ustatus->ctrl_max_pktsize;
				debug("5");
				if (ustatus->remain_size
				   > CONFIG_DESCRIPTOR_SIZE)
					ustatus->remain_size =
						CONFIG_DESCRIPTOR_SIZE;
				ustatus->ep0_state = EP0_STATE_GET_DSCPT;
				debug("6");
				break;
			default:
				writel(readl(&reg_otg->dcsr.depir[0].diepctl)
				       | DEPCTL_STALL,
				       &reg_otg->dcsr.depir[0].diepctl);
				break;
			}

			debug("7");
			break;

		case STANDARD_CLEAR_FEATURE:
			debug("STANDARD_CLEAR_FEATURE :");
			break;

		case STANDARD_SET_FEATURE:
			debug("STANDARD_SET_FEATURE :");
			break;

		case STANDARD_GET_STATUS:
			debug("STANDARD_GET_STATUS :");
			ustatus->ep0_state = EP0_STATE_GET_STATUS;
			break;

		case STANDARD_GET_INTERFACE:
			debug("STANDARD_GET_INTERFACE\n");
			ustatus->ep0_state = EP0_STATE_GET_INTERFACE;
			break;

		case STANDARD_SET_INTERFACE:
			debug("STANDARD_SET_INTERFACE\n");
			ustatus->cur_interface = setup_packet->value;
			ustatus->cur_setting = setup_packet->value;
			ustatus->ep0_state = EP0_STATE_INIT;
			break;

		case STANDARD_SYNCH_FRAME:
			debug("STANDARD_SYNCH_FRAME\n");
			ustatus->ep0_state = EP0_STATE_INIT;
			break;

		default:
			break;
		}
	}
	writel((1<<19) | (ustatus->ctrl_max_pktsize<<0),
	       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);

	if (ustatus->speed == USB_HIGH) {
		/*clear nak, next ep0, 64byte */
		writel(((1<<26)|(CONTROL_EP<<11)|(0<<0)),
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
	} else {
		/*clear nak, next ep0, 8byte */
		writel(((1<<26)|(CONTROL_EP<<11)|(3<<0)),
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
	}
	dmb();
}

static void nx_usb_transfer_ep0(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;

	switch (ustatus->ep0_state) {
	case EP0_STATE_INIT:
		writel((1<<19)|(0<<0),
		       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);
		/*ep0 enable, clear nak, next ep0, 8byte */
		writel(EPEN_CNAK_EP0_8,
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
		debug("EP0_STATE_INIT\n");
		break;

	/* GET_DESCRIPTOR:DEVICE */
	case EP0_STATE_GET_DSCPT:
		debug("EP0_STATE_GD_DEV_0 :");
		if (ustatus->speed == USB_HIGH) {
			/*ep0 enable, clear nak, next ep0, max 64byte */
			writel(EPEN_CNAK_EP0_64,
			       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
		} else {
			writel(EPEN_CNAK_EP0_8,
			       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
		}
		if (ustatus->current_fifo_size
		   >= ustatus->remain_size) {
			writel((1<<19)|(ustatus->remain_size<<0),
			       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);
			nx_usb_write_in_fifo(priv->reg_otg,
					     CONTROL_EP,
					     ustatus->current_ptr,
					     ustatus->remain_size);
			ustatus->ep0_state = EP0_STATE_INIT;
		} else {
			writel((1<<19)|(ustatus->current_fifo_size<<0),
			       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);
			nx_usb_write_in_fifo(priv->reg_otg,
					     CONTROL_EP,
					     ustatus->current_ptr,
					     ustatus->current_fifo_size);
			ustatus->remain_size -=
				ustatus->current_fifo_size;
			ustatus->current_ptr +=
				ustatus->current_fifo_size;
		}
		break;

	case EP0_STATE_GET_INTERFACE:
	case EP0_STATE_GET_CONFIG:
	case EP0_STATE_GET_STATUS:
		debug("EP0_STATE_INTERFACE_GET\n");
		debug("EP0_STATE_GET_STATUS\n");

		writel((1<<19)|(1<<0),
		       &reg_otg->dcsr.depir[CONTROL_EP].dieptsiz);
		writel(EPEN_CNAK_EP0_8,
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);

		if (ustatus->ep0_state == EP0_STATE_GET_INTERFACE)
			writel(ustatus->cur_interface,
			       &reg_otg->epfifo[CONTROL_EP][0]);
		else if (ustatus->ep0_state == EP0_STATE_GET_CONFIG)
			writel(ustatus->cur_config,
			       &reg_otg->epfifo[CONTROL_EP][0]);
		else
			writel(0, &reg_otg->epfifo[CONTROL_EP][0]);
		ustatus->ep0_state = EP0_STATE_INIT;
		break;

	default:
		break;
	}
}

static void nx_usb_int_bulkin(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u8 *bulkin_buf;
	u32 remain_cnt;

	debug("Bulk In Function\n");

	bulkin_buf = (u8 *)ustatus->up_ptr;
	remain_cnt = ustatus->up_size -
		((u32)((ulong)(ustatus->up_ptr -
			       ustatus->up_addr)));

	if (remain_cnt > ustatus->bulkin_max_pktsize) {
		writel((1<<19) | (ustatus->bulkin_max_pktsize<<0),
		       &reg_otg->dcsr.depir[BULK_IN_EP].dieptsiz);

		/* ep1 enable, clear nak, bulk, usb active,
		   next ep2, max pkt 64 */
		writel(1u<<31 | 1<<26 | 2<<18 | 1<<15 |
			ustatus->bulkin_max_pktsize<<0,
		       &reg_otg->dcsr.depir[BULK_IN_EP].diepctl);

		nx_usb_write_in_fifo(priv->reg_otg,
				     BULK_IN_EP, bulkin_buf,
				     ustatus->bulkin_max_pktsize);

		ustatus->up_ptr += ustatus->bulkin_max_pktsize;

	} else if (remain_cnt > 0) {
		writel((1<<19)|(remain_cnt<<0),
		       &reg_otg->dcsr.depir[BULK_IN_EP].dieptsiz);

		/* ep1 enable, clear nak, bulk, usb active,
		  next ep2, max pkt 64 */
		writel(1u<<31 | 1<<26 | 2<<18 | 1<<15 |
			ustatus->bulkin_max_pktsize<<0,
		       &reg_otg->dcsr.depir[BULK_IN_EP].diepctl);

		nx_usb_write_in_fifo(priv->reg_otg,
				     BULK_IN_EP, bulkin_buf, remain_cnt);

		ustatus->up_ptr += remain_cnt;

	} else { /*remain_cnt = 0*/
		writel((DEPCTL_SNAK|DEPCTL_BULK_TYPE),
		       &reg_otg->dcsr.depir[BULK_IN_EP].diepctl);
	}
}

static void nx_usb_int_bulkout(struct nx_usbdown_priv *priv,
			       u32 fifo_cnt_byte)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;

	debug("Bulk Out Function\n");

	nx_usb_read_out_fifo(priv->reg_otg,
			     BULK_OUT_EP, (u8 *)ustatus->rx_buf_addr,
			     fifo_cnt_byte);

	ustatus->rx_buf_addr	+= fifo_cnt_byte;
	ustatus->rx_size -= fifo_cnt_byte;

	if (ustatus->rx_size <= 0) {
		debug("Download completed!\n");

		ustatus->downloading = false;
	}

	writel((1<<19)|(ustatus->bulkout_max_pktsize<<0),
	       &reg_otg->dcsr.depor[BULK_OUT_EP].doeptsiz);

	/*ep2 enable, clear nak, bulk, usb active, next ep2, max pkt 64*/
	writel(1u<<31|1<<26|2<<18|1<<15|ustatus->bulkout_max_pktsize<<0,
	       &reg_otg->dcsr.depor[BULK_OUT_EP].doepctl);
}

static void nx_usb_reset(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u32 i;

	/* set all out ep nak */
	for (i = 0; i < 16; i++)
		writel(readl(&reg_otg->dcsr.depor[i].doepctl) | DEPCTL_SNAK,
		       &reg_otg->dcsr.depor[i].doepctl);

	ustatus->ep0_state = EP0_STATE_INIT;
	writel(((1<<BULK_OUT_EP)|(1<<CONTROL_EP))<<16 |
		((1<<BULK_IN_EP)|(1<<CONTROL_EP)),
	       &reg_otg->dcsr.daintmsk);
	writel(CTRL_OUT_EP_SETUP_PHASE_DONE|AHB_ERROR|TRANSFER_DONE,
	       &reg_otg->dcsr.doepmsk);
	writel(INTKN_TXFEMP|NON_ISO_IN_EP_TIMEOUT|AHB_ERROR|TRANSFER_DONE,
	       &reg_otg->dcsr.diepmsk);

	/* Rx FIFO Size */
	writel(RX_FIFO_SIZE, &reg_otg->gcsr.grxfsiz);


	/* Non Periodic Tx FIFO Size */
	writel(NPTX_FIFO_SIZE<<16 | NPTX_FIFO_START_ADDR<<0,
	       &reg_otg->gcsr.gnptxfsiz);

	/* clear all out ep nak */
	for (i = 0; i < 16; i++)
		writel(readl(&reg_otg->dcsr.depor[i].doepctl) |
		       (DEPCTL_EPENA|DEPCTL_CNAK),
		       &reg_otg->dcsr.depor[i].doepctl);

	/*clear device address */
	writel(readl(&reg_otg->dcsr.dcfg) & ~(0x7F<<4),
	       &reg_otg->dcsr.dcfg);
	dmb();
}

static bool nx_usb_set_init(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u32 status = readl(&reg_otg->dcsr.dsts); /* System status read */
	u16 VID = VENDORID, PID = PRODUCTID;

	/* Set if Device is High speed or Full speed */
	if (((status & 0x6) >> 1) == USB_HIGH) {
		debug("High Speed Detection\n");
	} else if (((status & 0x6) >> 1) == USB_FULL) {
		debug("Full Speed Detection\n");
	} else {
		debug("**** Error:Neither High_Speed nor Full_Speed\n");
		return false;
	}

	if (((status & 0x6) >> 1) == USB_HIGH)
		ustatus->speed = USB_HIGH;
	else
		ustatus->speed = USB_FULL;

	ustatus->rx_size = 512;

	/*
	 * READ ECID for Product and Vendor ID
	 */
	nx_usb_get_usbid(priv->reg_ecid, &VID, &PID);
	debug("%s %x %x\n", __func__, VID, PID);

	gs_device_descriptor_fs[8] = (u8)(VID & 0xff);
	gs_device_descriptor_hs[8] = gs_device_descriptor_fs[8];
	gs_device_descriptor_fs[9] = (u8)(VID >> 8);
	gs_device_descriptor_hs[9] = gs_device_descriptor_fs[9];
	gs_device_descriptor_fs[10] = (u8)(PID & 0xff);
	gs_device_descriptor_hs[10] = gs_device_descriptor_fs[10];
	gs_device_descriptor_fs[11] = (u8)(PID >> 8);
	gs_device_descriptor_hs[11] = gs_device_descriptor_fs[11];

	/* set endpoint */
	/* Unmask NX_OTG_DAINT source */
	writel(0xFF, &reg_otg->dcsr.depir[CONTROL_EP].diepint);
	writel(0xFF, &reg_otg->dcsr.depor[CONTROL_EP].doepint);
	writel(0xFF, &reg_otg->dcsr.depir[BULK_IN_EP].diepint);
	writel(0xFF, &reg_otg->dcsr.depor[BULK_OUT_EP].doepint);

	/* Init For Ep0*/
	if (ustatus->speed == USB_HIGH) {
		ustatus->ctrl_max_pktsize = HIGH_MAX_PKT_SIZE_EP0;
		ustatus->bulkin_max_pktsize = HIGH_MAX_PKT_SIZE_EP1;
		ustatus->bulkout_max_pktsize = HIGH_MAX_PKT_SIZE_EP2;
		ustatus->device_descriptor = gs_device_descriptor_hs;
		ustatus->config_descriptor = gs_config_descriptor_hs;

		/*MPS:64bytes */
		writel(((1<<26)|(CONTROL_EP<<11)|(0<<0)),
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
		/*ep0 enable, clear nak */
		writel((1u<<31)|(1<<26)|(0<<0),
		       &reg_otg->dcsr.depor[CONTROL_EP].doepctl);
	} else {
		ustatus->ctrl_max_pktsize = FULL_MAX_PKT_SIZE_EP0;
		ustatus->bulkin_max_pktsize = FULL_MAX_PKT_SIZE_EP1;
		ustatus->bulkout_max_pktsize = FULL_MAX_PKT_SIZE_EP2;
		ustatus->device_descriptor = gs_device_descriptor_fs;
		ustatus->config_descriptor = gs_config_descriptor_fs;

		/*MPS:8bytes */
		writel(((1<<26)|(CONTROL_EP<<11)|(3<<0)),
		       &reg_otg->dcsr.depir[CONTROL_EP].diepctl);
		/*ep0 enable, clear nak */
		writel((1u<<31)|(1<<26)|(3<<0),
		       &reg_otg->dcsr.depor[CONTROL_EP].doepctl);
	}

	/* set_opmode */
	writel(INT_RESUME|INT_OUT_EP|INT_IN_EP|INT_ENUMDONE|
	       INT_RESET|INT_SUSPEND|INT_RX_FIFO_NOT_EMPTY,
	       &reg_otg->gcsr.gintmsk);

	writel(MODE_SLAVE|BURST_SINGLE|GBL_INT_UNMASK,
	       &reg_otg->gcsr.gahbcfg);

	writel((1<<19)|(ustatus->bulkout_max_pktsize<<0),
	       &reg_otg->dcsr.depor[BULK_OUT_EP].doeptsiz);

	writel((1<<19)|(0<<0), &reg_otg->dcsr.depor[BULK_IN_EP].doeptsiz);

	/* bulk out ep enable, clear nak, bulk, usb active, next ep2, max pkt */
	writel(1u<<31|1<<26|2<<18|1<<15|ustatus->bulkout_max_pktsize<<0,
	       &reg_otg->dcsr.depor[BULK_OUT_EP].doepctl);

	/* bulk in ep enable, clear nak, bulk, usb active, next ep1, max pkt */
	writel(0u<<31|1<<26|2<<18|1<<15|ustatus->bulkin_max_pktsize<<0,
	       &reg_otg->dcsr.depir[BULK_IN_EP].diepctl);

	dmb();

	return true;
}

static void nx_usb_pkt_receive(struct nx_usbdown_priv *priv)
{
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u32 rx_status, fifo_cnt_byte;

	rx_status = readl(&reg_otg->gcsr.grxstsp);

	if ((rx_status & (0xf<<17)) == SETUP_PKT_RECEIVED) {
		debug("SETUP_PKT_RECEIVED\n");
		nx_usb_ep0_int_hndlr(priv);
	} else if ((rx_status & (0xf<<17)) == OUT_PKT_RECEIVED) {
		fifo_cnt_byte = (rx_status & 0x7ff0)>>4;
		debug("OUT_PKT_RECEIVED\n");

		if ((rx_status & BULK_OUT_EP) && (fifo_cnt_byte)) {
			nx_usb_int_bulkout(priv, fifo_cnt_byte);
			writel(INT_RESUME|INT_OUT_EP|INT_IN_EP|INT_ENUMDONE|
				INT_RESET|INT_SUSPEND|INT_RX_FIFO_NOT_EMPTY,
				&reg_otg->gcsr.gintmsk);
			dmb();
			return;
		}
	} else if ((rx_status & (0xf<<17)) == GLOBAL_OUT_NAK) {
		debug("GLOBAL_OUT_NAK\n");
	} else if ((rx_status & (0xf<<17)) == OUT_TRNASFER_COMPLETED) {
		debug("OUT_TRNASFER_COMPLETED\n");
	} else if ((rx_status & (0xf<<17)) == SETUP_TRANSACTION_COMPLETED) {
		debug("SETUP_TRANSACTION_COMPLETED\n");
	} else {
		debug("Reserved\n");
	}
	dmb();
}

static void nx_usb_transfer(struct nx_usbdown_priv *priv)
{
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u32 ep_int;
	u32 ep_int_status;

	ep_int = readl(&reg_otg->dcsr.daint);

	if (ep_int & (1<<CONTROL_EP)) {
		ep_int_status =
			readl(&reg_otg->dcsr.depir[CONTROL_EP].diepint);

		if (ep_int_status & INTKN_TXFEMP) {
			while (1) {
				if (!((readl(&reg_otg->gcsr.gnptxsts) &
				       0xFFFF) <
				      ustatus->ctrl_max_pktsize))
					break;
			}
			nx_usb_transfer_ep0(priv);
		}

		writel(ep_int_status,
		       &reg_otg->dcsr.depir[CONTROL_EP].diepint);
	}

	if (ep_int & ((1<<CONTROL_EP)<<16)) {
		ep_int_status =
			readl(&reg_otg->dcsr.depor[CONTROL_EP].doepint);

		writel((1<<29)|(1<<19)|(8<<0),
		       &reg_otg->dcsr.depor[CONTROL_EP].doeptsiz);
		/*ep0 enable, clear nak */
		writel(1u<<31|1<<26,
		       &reg_otg->dcsr.depor[CONTROL_EP].doepctl);
		/* Interrupt Clear */
		writel(ep_int_status,
		       &reg_otg->dcsr.depor[CONTROL_EP].doepint);
	}

	if (ep_int & (1<<BULK_IN_EP)) {
		ep_int_status =
			readl(&reg_otg->dcsr.depir[BULK_IN_EP].diepint);

		/* Interrupt Clear */
		writel(ep_int_status,
		       &reg_otg->dcsr.depir[BULK_IN_EP].diepint);

		if (ep_int_status & INTKN_TXFEMP)
			nx_usb_int_bulkin(priv);
	}

	if (ep_int & ((1<<BULK_OUT_EP)<<16)) {
		ep_int_status =
			readl(&reg_otg->dcsr.depor[BULK_OUT_EP].doepint);
		/* Interrupt Clear */
		writel(ep_int_status,
		       &reg_otg->dcsr.depor[BULK_OUT_EP].doepint);
	}
}

static void nx_udc_int_hndlr(struct nx_usbdown_priv *priv)
{
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	u32 int_status;
	bool tmp;

	/* Core Interrupt Register */
	int_status = readl(&reg_otg->gcsr.gintsts);
	if (int_status & INT_RESET) {
		debug("INT_RESET\n");
		nx_usb_reset(priv);
	}

	if (int_status & INT_ENUMDONE) {
		debug("INT_ENUMDONE :");

		tmp = nx_usb_set_init(priv);
		if (!tmp) {
			/* Interrupt Clear */
			writel(int_status, &reg_otg->gcsr.gintsts);
			return;
		}
	}

	if (int_status & INT_RESUME)
		debug("INT_RESUME\n");

	if (int_status & INT_SUSPEND)
		debug("INT_SUSPEND\n");

	if (int_status & INT_RX_FIFO_NOT_EMPTY) {
		debug("INT_RX_FIFO_NOT_EMPTY\n");
		/* Read only register field */

		writel(INT_RESUME|INT_OUT_EP|INT_IN_EP|
			INT_ENUMDONE|INT_RESET|INT_SUSPEND,
		       &reg_otg->gcsr.gintmsk);
		nx_usb_pkt_receive(priv);
		writel(INT_RESUME|INT_OUT_EP|INT_IN_EP|
			INT_ENUMDONE|INT_RESET|INT_SUSPEND|
			INT_RX_FIFO_NOT_EMPTY, &reg_otg->gcsr.gintmsk);
	}

	if ((int_status & INT_IN_EP) || (int_status & INT_OUT_EP)) {
		debug("INT_IN or OUT_EP\n");
		/* Read only register field */
		nx_usb_transfer(priv);
	}

	writel(int_status, &reg_otg->gcsr.gintsts); /* Interrupt Clear */
	debug("[GINTSTS:0x%08x:0x%08x]\n", int_status,
	      (WKUP_INT|OEP_INT|IEP_INT|ENUM_DONE|USB_RST|USB_SUSP|RXF_LVL));
}

/* Nexell USBOTG PHY registers */

/* USBOTG Configuration 0 Register */
#define NX_OTG_CON0				0x30
#define NX_OTG_CON0_SS_SCALEDOWN_MODE		(3 << 0)

/* USBOTG Configuration 1 Register */
#define NX_OTG_CON1				0x34
#define NX_OTG_CON1_VBUSVLDEXTSEL		BIT(25)
#define NX_OTG_CON1_VBUSVLDEXT			BIT(24)
#define NX_OTG_CON1_VBUS_INTERNAL ~( \
					NX_OTG_CON1_VBUSVLDEXTSEL | \
					NX_OTG_CON1_VBUSVLDEXT)
#define NX_OTG_CON1_VBUS_VLDEXT0 ( \
					NX_OTG_CON1_VBUSVLDEXTSEL | \
					NX_OTG_CON1_VBUSVLDEXT)

#define NX_OTG_CON1_POR				BIT(8)
#define NX_OTG_CON1_POR_ENB			BIT(7)
#define NX_OTG_CON1_POR_MASK			(0x3 << 7)
#define NX_OTG_CON1_RST				BIT(3)
#define NX_OTG_CON1_UTMI_RST			BIT(2)

/* USBOTG Configuration 2 Register */
#define NX_OTG_CON2				0x38
#define NX_OTG_CON2_OTGTUNE_MASK		(0x7 << 23)
#define NX_OTG_CON2_WORDINTERFACE		BIT(9)
#define NX_OTG_CON2_WORDINTERFACE_ENB		BIT(8)
#define NX_OTG_CON2_WORDINTERFACE_16 ( \
					NX_OTG_CON2_WORDINTERFACE | \
					NX_OTG_CON2_WORDINTERFACE_ENB)

/* USBOTG Configuration 3 Register */
#define NX_OTG_CON3				0x3C
#define NX_OTG_CON3_ACAENB			BIT(15)
#define NX_OTG_CON3_DCDENB			BIT(14)
#define NX_OTG_CON3_VDATSRCENB			BIT(13)
#define NX_OTG_CON3_VDATDETENB			BIT(12)
#define NX_OTG_CON3_CHRGSEL			BIT(11)
#define NX_OTG_CON3_DET_N_CHG ( \
					NX_OTG_CON3_ACAENB | \
					NX_OTG_CON3_DCDENB | \
					NX_OTG_CON3_VDATSRCENB | \
					NX_OTG_CON3_VDATDETENB | \
					NX_OTG_CON3_CHRGSEL)

static bool nx_usb_down(struct udevice *dev, ulong buffer)
{
	struct nx_usbdown_priv *priv = dev_get_priv(dev);
	struct nx_usbboot_status *ustatus = &priv->ustatus;
	struct nx_usb_otg_reg *reg_otg = priv->reg_otg;
	unsigned int *nsih = (unsigned int *)ustatus->rx_buf_addr;

	ustatus->rx_buf_addr = (u8 *)buffer;
	nsih = (unsigned int *)buffer;

	generic_phy_init(&priv->phy);
	generic_phy_power_on(&priv->phy);

	if (priv->clk.dev)
		clk_enable(&priv->clk);

	/* usb core soft reset */
	writel(CORE_SOFT_RESET, &reg_otg->gcsr.grstctl);
	dmb();

	while (1) {
		if (readl(&reg_otg->gcsr.grstctl) & AHB_MASTER_IDLE)
			break;
	}
	dmb();

	/* init_core */
	writel(PTXFE_HALF|NPTXFE_HALF|MODE_SLAVE|
		BURST_SINGLE|GBL_INT_UNMASK, &reg_otg->gcsr.gahbcfg);
	writel(0<<15		/* PHY Low Power Clock sel */
		|1<<14		/* Non-Periodic TxFIFO Rewind Enable */
		|5<<10		/* Turnaround time */
		|0<<9		/* 0:HNP disable, 1:HNP enable */
		|0<<8		/* 0:SRP disable, 1:SRP enable */
		|0<<7		/* ULPI DDR sel */
		|0<<6		/* 0: high speed utmi+, 1: full speed serial */
		|0<<4		/* 0: utmi+, 1:ulpi */
		|1<<3		/* phy i/f  0:8bit, 1:16bit */
		|7<<0		/* HS/FS Timeout**/,
	       &reg_otg->gcsr.gusbcfg);

	dmb();

	if ((readl(&reg_otg->gcsr.gintsts) & 0x1) == INT_DEV_MODE) {
		/* soft disconnect on */
		writel(readl(&reg_otg->dcsr.dctl) | SOFT_DISCONNECT,
		       &reg_otg->dcsr.dctl);
		udelay(10);
		/* soft disconnect off */
		writel(readl(&reg_otg->dcsr.dctl) & ~SOFT_DISCONNECT,
		       &reg_otg->dcsr.dctl);
		udelay(10);

		/* usb init device */
		writel(1<<18, &reg_otg->dcsr.dcfg); /* [][1: full speed(30Mhz)
							0:high speed] */
		writel(INT_RESUME|INT_OUT_EP|INT_IN_EP|
			INT_ENUMDONE|INT_RESET|INT_SUSPEND|
			INT_RX_FIFO_NOT_EMPTY, &reg_otg->gcsr.gintmsk);
		udelay(10);
	}
	dmb();

	ustatus->cur_config = 0;
	ustatus->cur_interface = 0;
	ustatus->cur_setting = 0;
	ustatus->speed = USB_HIGH;
	ustatus->ep0_state = EP0_STATE_INIT;

	ustatus->downloading = true;

	dmb();

	while (ustatus->downloading) {
		if (ctrlc())
			goto _exit;

		if (readl(&reg_otg->gcsr.gintsts) &
		    (WKUP_INT|OEP_INT|IEP_INT|ENUM_DONE|USB_RST|USB_SUSP|
		     RXF_LVL)) {
			nx_udc_int_hndlr(priv);
		     /*	writel(0xFFFFFFFF, &reg_otg->gcsr.gintsts); */
			mdelay(3);
		}
	}

	ustatus->rx_buf_addr -= 512;
	ustatus->rx_size = nsih[17];

	ustatus->downloading = true;
	printf(" Size  %d(hex : %x)\n", ustatus->rx_size, ustatus->rx_size);
	dmb();

	while (ustatus->downloading) {
		if (ctrlc())
			goto _exit;

		if (readl(&reg_otg->gcsr.gintsts) &
		   (WKUP_INT | OEP_INT | IEP_INT | ENUM_DONE |
		    USB_RST | USB_SUSP | RXF_LVL)) {
			nx_udc_int_hndlr(priv);
		    /*	writel(0xFFFFFFFF, &reg_otg->gcsr.gintsts); */
		}
	}

_exit:
	dmb();
	/* usb core soft reset */
	writel(CORE_SOFT_RESET, &reg_otg->gcsr.grstctl);
	while (1) {
		if (readl(&reg_otg->gcsr.grstctl) & AHB_MASTER_IDLE)
			break;
	}

	if (priv->clk.dev)
		clk_disable(&priv->clk);

	generic_phy_power_off(&priv->phy);
	generic_phy_exit(&priv->phy);

	return true;
}

static int nx_usb_probe(struct udevice *dev)
{
	struct nx_usbdown_priv *priv = dev_get_priv(dev);
	fdt_addr_t base;
	int ret;

	base = devfdt_get_addr_index(dev, 0);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->reg_otg = devm_ioremap(dev, base, SZ_4K);

	base = devfdt_get_addr_index(dev, 1);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->reg_ecid = devm_ioremap(dev, base, SZ_256);

	ret = generic_phy_get_by_index(dev, 0, &priv->phy);
	if (ret) {
		printf("Failed to get USB PHY for %s (%d)\n", dev->name, ret);
		return ret;
	}

	clk_get_by_index(dev, 0, &priv->clk);

	debug("%s: otg %p phy %s\n",
	      __func__, priv->reg_otg, priv->phy.dev->name);

	return 0;
}

static const struct dm_usb_ops nx_usbdown_ops = {
};

static const struct udevice_id nx_usbdown_ids[] = {
	{ .compatible = "nexell,dwc2-usb-downloader" },
	{ }
};

U_BOOT_DRIVER(nx_usbdown) = {
	.name = "nexell,usb-downloader",
	.id = UCLASS_USB,
	.of_match = nx_usbdown_ids,
	.probe = nx_usb_probe,
	.ops = &nx_usbdown_ops,
	.priv_auto_alloc_size = sizeof(struct nx_usbdown_priv),
};

int nx_usb_download(ulong buffer)
{
	struct udevice *dev, *devp;
	int ret = -EINVAL;

	for (ret = uclass_find_first_device(UCLASS_USB, &dev); dev;
		ret = uclass_find_next_device(&dev)) {
		if (ret)
			continue;

		/* compare device node name: usbdown@xxxxxxxx */
		if (strncmp(dev->name, "usbdown", strlen("usbdown")))
			continue;

		debug("usb down probe %s\n", dev->name);

		ret = uclass_get_device_tail(dev, 0, &devp);
		if (!ret)
			nx_usb_down(dev, buffer);
	}

	return ret;
}

