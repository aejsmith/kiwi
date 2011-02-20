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
 * @brief		PC platform loader definitions.
 */

#ifndef __PLATFORM_LOADER_H
#define __PLATFORM_LOADER_H

/** Memory layout definitions. */
#define LOADER_LOAD_ADDR	0x10000		/**< Load address of the boot loader. */
#define MULTIBOOT_LOAD_ADDR	0x100000	/**< Load address for Multiboot. */
#define MULTIBOOT_LOAD_OFFSET	0xF0000		/**< Load offset for Multiboot. */

/** Segment defintions. */
#define SEGMENT_CS		0x08		/**< Code segment. */
#define SEGMENT_DS		0x10		/**< Data segment. */
#define SEGMENT_CS16		0x18		/**< 16-bit code segment. */
#define SEGMENT_CS64		0x20		/**< 64-bit code segment. */
#define SEGMENT_DS64		0x28		/**< 64-bit data segment. */

#ifndef __ASM__

struct disk;

extern uint8_t bios_disk_id(struct disk *disk);

extern void platform_early_init(void);
extern void platform_reboot(void);

#endif /* __ASM__ */

#endif /* __PLATFORM_LOADER_H */
