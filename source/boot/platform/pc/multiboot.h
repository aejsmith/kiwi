/*
 * Copyright (C) 2010-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

/** Size of the Multiboot information structure. */
#define MB_INFO_SIZE		88

/** Maximum length of the Multiboot command line. */
#define MB_CMDLINE_MAX		256

/** Offsets into the structure required in assembly code. */
#define MB_INFO_OFFSET_BOOTDEV	12		/**< Offset of the boot device field. */
#define MB_INFO_OFFSET_CMDLINE	16		/**< Offset of the command line field. */

#ifndef __ASM__

#include <types.h>

/** Multiboot information structure. */
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

extern unsigned long multiboot_magic;
extern char multiboot_cmdline[];

#endif /* __ASM__ */
#endif /* __PLATFORM_MULTIBOOT_H */
