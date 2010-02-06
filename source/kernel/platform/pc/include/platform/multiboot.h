/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Multiboot header.
 */

#ifndef __PLATFORM_MULTIBOOT_H
#define __PLATFORM_MULTIBOOT_H

/** Flags for the multiboot header. */
#define MB_HFLAG_MODALIGN	(1<<0)		/**< Align loaded modules on page boundaries. */
#define MB_HFLAG_MEMINFO	(1<<1)		/**< Kernel wants a memory map. */
#define MB_HFLAG_KLUDGE		(1<<16)		/**< Use a.out kludge. */

/** Multiboot magic values. */
#define MB_LOADER_MAGIC		0x2BADB002	/**< Magic value passed by the bootloader. */
#define MB_HEADER_MAGIC		0x1BADB002	/**< Magic value in the multiboot header. */

/** Flags passed by the bootloader. */
#define	MB_FLAG_MEMINFO		(1<<0)		/**< Bootloader provided memory info. */
#define MB_FLAG_BOOTDEV		(1<<1)		/**< Bootloader provided boot device. */
#define MB_FLAG_CMDLINE		(1<<2)		/**< Bootloader provided command line. */
#define MB_FLAG_MODULES		(1<<3)		/**< Bootloader provided module info. */
#define MB_FLAG_AOUTSYMS	(1<<4)		/**< Bootloader provided a.out symbols. */
#define MB_FLAG_ELFSYMS		(1<<5)		/**< Bootloader provided ELF symbols. */
#define MB_FLAG_MMAP		(1<<6)		/**< Bootloader provided memory map. */
#define MB_FLAG_DRIVES		(1<<7)		/**< Bootloader provided drive information. */
#define MB_FLAG_CONFTABLE	(1<<8)		/**< Bootloader provided config table. */
#define MB_FLAG_LDRNAME		(1<<9)		/**< Bootloader provided its name. */
#define MB_FLAG_APMTABLE	(1<<10)		/**< Bootloader provided APM table. */
#define MB_FLAG_VBEINFO		(1<<11)		/**< Bootloader provided VBE info. */

/** Memory map type values. */
#define E820_TYPE_FREE		1		/**< Usable memory. */
#define E820_TYPE_RESERVED	2		/**< Reserved memory. */
#define E820_TYPE_ACPI_RECLAIM	3		/**< ACPI reclaimable. */
#define E820_TYPE_ACPI_NVS	4		/**< ACPI NVS. */
#define E820_TYPE_BAD		5		/**< Bad memory. */

#ifndef __ASM__

#include <types.h>

/** Multiboot information structure provided by bootloader. */
typedef struct multiboot_info {
	uint32_t flags;				/**< Flags. */
	uint32_t mem_lower;			/**< Bytes of lower memory. */
	uint32_t mem_upper;			/**< Bytes of upper memory. */
	uint32_t boot_device;			/**< Boot device. */
	uint32_t cmdline;			/**< Address of kernel command line. */
	uint32_t mods_count;			/**< Module count. */
	uint32_t mods_addr;			/**< Address of module structures. */
	uint32_t elf_sec[4];			/**< ELF section headers. */
	uint32_t mmap_length;			/**< Memory map length. */
	uint32_t mmap_addr;			/**< Address of memory map. */
	uint32_t drives_length;			/**< Drive information length. */
	uint32_t drives_addr;			/**< Address of drive information. */
	uint32_t config_table;			/**< Configuration table. */
	uint32_t boot_loader_name;		/**< Boot loader name. */
	uint32_t apm_table;			/**< APM table. */
	uint32_t vbe_control_info;		/**< VBE control information. */
	uint32_t vbe_mode_info;			/**< VBE mode information. */
	uint16_t vbe_mode;			/**< VBE mode. */
	uint16_t vbe_interface_seg;		/**< VBE interface segment. */
	uint16_t vbe_interface_off;		/**< VBE interface offset. */
	uint16_t vbe_interface_len;		/**< VBE interface length. */
} __packed multiboot_info_t;

/** Multiboot module information structure. */
typedef struct multiboot_module {
	uint32_t mod_start;			/**< Address of module. */
	uint32_t mod_end;			/**< End address of module. */
	uint32_t string;			/**< Name of module. */
	uint32_t reserved;			/**< Reserved. */
} __packed multiboot_module_t;

/** Multiboot memory map structure. */
typedef struct multiboot_memmap {
	uint32_t size;				/**< Size of entry. */
	uint64_t base_addr;			/**< Address. */
	uint64_t length;			/**< Length. */
	uint32_t type;				/**< Type of entry. */
} __packed multiboot_memmap_t;

extern void multiboot_premm_init(multiboot_info_t *info);
extern void multiboot_postmm_init(void);

#endif /* __ASM__ */
#endif /* __PLATFORM_MULTIBOOT_H */
