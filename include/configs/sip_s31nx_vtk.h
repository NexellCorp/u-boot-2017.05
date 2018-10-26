/*
 * (C) Copyright 2018 Nexell
 * JungHyun, Kim <jhkim@nexell.co.kr>
 *
 * SPDX-License-Identifier:      GPL-2.0+
 */

#ifndef __SIP_S31NX_VTK_H__
#define __SIP_S31NX_VTK_H__

#include <linux/sizes.h>

/* dram 1 bank num */
#define CONFIG_NR_DRAM_BANKS	1

/* Console I/O Buffer Size  */
#define CONFIG_SYS_CBSIZE	1024
/* max number of command args   */
#define CONFIG_SYS_MAXARGS	16

#define CONFIG_BOOTCOMMAND		"run autoboot"
#define CONFIG_DEFAULT_CONSOLE		"console=ttyS2,115200n8\0"

/* For SD/MMC */
#define CONFIG_BOUNCE_BUFFER
#define CONFIG_SUPPORT_EMMC_BOOT

#define MMC_BOOT_DEV		0
#define MMC_ROOT_DEV		0
#define MMC_BOOT_PART		2
#define MMC_MODULES_PART	4
#define MMC_FIRMWARE_PART	6
#define MMC_ROOTFS_PART		8
#define MMC_SEC_PART		10
#define MMC_DATA_PART		11

#define MMC_BOOT_PART_TYPE	"ext4"
#define MMC_MODULES_PART_TYPE	"ext4"
#define MMC_FIRMWARE_PART_TYPE	"ext4"
#define MMC_ROOTFS_PART_TYPE	"ext4"
#define MMC_SEC_PART_TYPE	"ext4"
#define MMC_DATA_PART_TYPE	"ext4"

/* Partition Size(Unit: MiB) */
#define BOOT_PART_ALIGN		8
#define BOOT_PART_SIZE		16
#define MODULES_PART_SIZE	20
#define FIRMWARE_PART_SIZE	8
#define ROOTFS_PART_SIZE	512
#define SEC_PART_SIZE		128

/*
 * Default environment organization
 */
#if !defined(CONFIG_ENV_IS_IN_MMC) && !defined(CONFIG_ENV_IS_IN_NAND) && \
	!defined(CONFIG_ENV_IS_IN_FLASH) && !defined(CONFIG_ENV_IS_IN_EEPROM)
	#define CONFIG_ENV_OFFSET	1024
	#define CONFIG_ENV_SIZE		(16 * 1024) /* env size */
	/* imls - list all images found in flash, default enable so disable */
	#undef CONFIG_CMD_IMLS
#endif

#define CONFIG_ENV_OVERWRITE
#define CONFIG_MISC_INIT_R
#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

/* USB Device Firmware Update support */
#define DFU_ALT_INFO_RAM \
	"dfu_alt_info_ram=" \
	"setenv dfu_alt_info " \
	"kernel ram 0x48000000 0xD80000\\\\;" \
	"fdt ram 0x49000000 0x80000\\\\;" \
	"ramdisk ram 0x49100000 0x4000000\0" \
	"dfu_ram=run dfu_alt_info_ram && dfu 0 ram 0\0" \
	"thor_ram=run dfu_ram_info && thordown 0 ram 0\0"

#define DFU_ALT_BOOT_EMMC \
	"bl-singleimage.bin.raw raw 0x0 0x1800 mmcpart 1;" \
	"bl1 raw 0x0 0x80 mmcpart 1;" \
	"bl2 raw 0xa2 0x80 mmcpart 1;" \
	"sss raw 0x122 0x40 mmcpart 1;" \
	"bl32 raw 0x162 0x1000 mmcpart 1;" \
	"u-boot raw 0x1162 0x800 mmcpart 1;"

#define DFU_ALT_BOOT_SD \
	"donotuse raw 0x22 0x1800;" \
	"bl1 raw 0x22 0x40;" \
	"bl2 raw 0xa2 0x40;" \
	"sss raw 0x122 0x40;" \
	"bl32 raw 0x162 0x1000;" \
	"u-boot raw 0x1162 0x800;"

/* eMMC DFU info(Use eMMC Boot Partition) */
#define DFU_ALT_INFO \
	"dfu_alt_info=" \
	DFU_ALT_BOOT_EMMC \
	"/zImage ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"/fitImage ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"/uInitrd ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"sip-s31nx-vtk.dtb ext4 0 " __stringify(MMC_BOOT_PART) ";" \
	"boot.img part 0 " __stringify(MMC_BOOT_PART) ";" \
	"modules.img part 0 " __stringify(MMC_MODULES_PART) ";" \
	"firmware.img part 0 " __stringify(MMC_FIRMWARE_PART) ";" \
	"rootfs.img part 0 " __stringify(MMC_ROOTFS_PART) ";" \
	"sec.img part 0 " __stringify(MMC_SEC_PART) ";" \
	"data.img part 0 " __stringify(MMC_DATA_PART) "\0"

#define CONFIG_DFU_ALT_BOOT_SD \
	DFU_ALT_BOOT_SD \
	"/zImage ext4 1 " __stringify(MMC_BOOT_PART) ";" \
	"/fitImage ext4 1 " __stringify(MMC_BOOT_PART) ";" \
	"/uInitrd ext4 1 " __stringify(MMC_BOOT_PART) ";" \
	"sip-s31nx-vtk.dtb ext4 1 " __stringify(MMC_BOOT_PART) ";" \
	"boot.img part 1 " __stringify(MMC_BOOT_PART) ";" \
	"modules.img part 1 " __stringify(MMC_MODULES_PART) ";" \
	"firmware.img part 1 " __stringify(MMC_FIRMWARE_PART) ";" \
	"rootfs.img part 1 " __stringify(MMC_ROOTFS_PART) ";" \
	"sec.img part 1 " __stringify(MMC_SEC_PART) ";" \
	"data.img part 1 " __stringify(MMC_DATA_PART) "\0"

#define PARTS_DEFAULT \
	"uuid_disk=${uuid_gpt_disk};" \
	"name=flag,start=4MiB,size=128KiB,uuid=${uuid_gpt_flag};" \
	"name=boot_a,start=" __stringify(BOOT_PART_ALIGN) "MiB,size=" \
		__stringify(BOOT_PART_SIZE) "MiB,uuid=${uuid_gpt_boot_a};" \
	"name=boot_b,size=" __stringify(BOOT_PART_SIZE) "MiB,uuid=${uuid_gpt_boot_b};" \
	"name=modules_a,size=" __stringify(MODULES_PART_SIZE) "MiB,uuid=${uuid_gpt_modules_a};" \
	"name=modules_b,size=" __stringify(MODULES_PART_SIZE) "MiB,uuid=${uuid_gpt_modules_b};" \
	"name=firmware_a,size=" __stringify(FIRMWARE_PART_SIZE) "MiB,uuid=${uuid_gpt_firmware_a};" \
	"name=firmware_b,size=" __stringify(FIRMWARE_PART_SIZE) "MiB,uuid=${uuid_gpt_firmware_b};" \
	"name=rootfs_a,size=" __stringify(ROOTFS_PART_SIZE) "MiB,uuid=${uuid_gpt_rootfs_a};" \
	"name=rootfs_b,size=" __stringify(ROOTFS_PART_SIZE) "MiB,uuid=${uuid_gpt_rootfs_b};" \
	"name=sec,size=" __stringify(SEC_PART_SIZE) "MiB,uuid=${uuid_gpt_sec};" \
	"name=data,size=-,uuid=${uuid_gpt_data}\0"

/* System memory Configuration */
#define	CONFIG_SYS_TEXT_BASE	0x43C00000
#define	CONFIG_SYS_INIT_SP_ADDR	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_MONITOR_BASE	CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_SDRAM_BASE	0x40000000
#define CONFIG_SYS_SDRAM_SIZE	0x1F000000
#define CONFIG_SYS_MALLOC_LEN	(64 * SZ_1M)

/* Memory Test (up to 0x3C00000:60MB) */
#define MEMTEST_SIZE			(32 * SZ_1M)
#define CONFIG_SYS_MEMTEST_START	(CONFIG_SYS_SDRAM_BASE + 0)
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + MEMTEST_SIZE)

/* kernel load address */
#define CONFIG_SYS_LOAD_ADDR	0x48000000
#define FDT_ADDR		0x49000000
#define INITRD_ADDR		0x49100000

/* environments */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoboot=run boot_initrd\0" \
	"bootdelay=" __stringify(CONFIG_BOOTDELAY) "\0" \
	"boot_initrd=" \
		"run load_kernel;" \
		"run load_fdt;" \
		"run load_initrd;" \
		"run load_args;" \
		"bootz ${kernel_addr} ${initrd_addr} ${fdt_addr}\0" \
	"console=" CONFIG_DEFAULT_CONSOLE \
	"dfu_bufsiz=0x2000000\0" \
	DFU_ALT_INFO \
	DFU_ALT_INFO_RAM \
	"fdt_addr=" __stringify(FDT_ADDR) "\0" \
	"fdt_file=sip-s31nx-vtk.dtb\0" \
	"format_emmc=" \
		"gpt write mmc 0 $partitions;" \
		"mmc rescan;" \
		"mmc partconf 0 0 1 1;" \
		"mmc bootbus 0 2 0 0;" \
		"mmc rst-function 0 1;" \
		"mmc rescan\0" \
	"initrd_addr=" __stringify(INITRD_ADDR) "\0" \
	"initrd_file=uInitrd\0" \
	"load_args=setenv bootargs \"" \
		"root=/dev/mmcblk${mmc_boot_dev}p${mmc_rootfs_part} " \
		"rootfstype=${mmc_rootfs_part_type} ${root_rw} " \
		"bootpart=/dev/mmcblk${mmc_boot_dev}p${mmc_boot_part} " \
		"bootpart_type=${mmc_boot_part_type} " \
		"modulespart=/dev/mmcblk${mmc_boot_dev}p${mmc_modules_part} " \
		"modulespart_type=${mmc_modules_part_type} " \
		"firmwarepart=/dev/mmcblk${mmc_boot_dev}p${mmc_firmware_part} " \
		"firmwarespart_type=${mmc_firmware_part_type} " \
		"secpart=/dev/mmcblk${mmc_boot_dev}p${mmc_sec_part} " \
		"secpart_type=${mmc_sec_part_type} " \
		"datapart=/dev/mmcblk${mmc_boot_dev}p${mmc_data_part} " \
		"datapart_type=${mmc_data_part_type} " \
		"${console} ${log_msg} ${opts}" \
		"\"\0" \
	"load_kernel=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${kernel_addr} " \
		"${kernel_file}\0" \
	"load_initrd=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${initrd_addr} " \
		"${initrd_file}\0" \
	"load_fdt=load mmc ${mmc_boot_dev}:${mmc_boot_part} ${fdt_addr} " \
		"${fdt_file}\0" \
	"log_msg=loglevel=7 printk.time=1 earlyprintk\0" \
	"kernel_addr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_file=zImage\0" \
	"mmc_boot_dev=" __stringify(MMC_BOOT_DEV) "\0" \
	"mmc_boot_part=" __stringify(MMC_BOOT_PART) "\0" \
	"mmc_boot_part_type=" MMC_BOOT_PART_TYPE "\0" \
	"mmc_modules_part=" __stringify(MMC_MODULES_PART) "\0" \
	"mmc_modules_part_type=" MMC_MODULES_PART_TYPE "\0" \
	"mmc_firmware_part=" __stringify(MMC_FIRMWARE_PART) "\0" \
	"mmc_firmware_part_type=" MMC_FIRMWARE_PART_TYPE "\0" \
	"mmc_rootfs_part=" __stringify(MMC_ROOTFS_PART) "\0" \
	"mmc_rootfs_part_type=" MMC_ROOTFS_PART_TYPE "\0" \
	"mmc_sec_part=" __stringify(MMC_SEC_PART) "\0" \
	"mmc_sec_part_type=" MMC_SEC_PART_TYPE "\0" \
	"mmc_data_part=" __stringify(MMC_DATA_PART) "\0" \
	"mmc_data_part_type=" MMC_DATA_PART_TYPE "\0" \
	"partitions=" PARTS_DEFAULT \
	"root_rw=rw\0" \

#endif
