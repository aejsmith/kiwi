/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		PC platform bootloader definitions.
 */

#ifndef __PLATFORM_BOOT_H
#define __PLATFORM_BOOT_H

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

#define platform_early_init()	
extern void platform_video_init(void);
extern void platform_video_enable(void);

#endif /* __ASM__ */
#endif /* __PLATFORM_BOOT_H */
