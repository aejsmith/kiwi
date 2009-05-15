/* Kiwi AMD64 segment definitions
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
 * @brief		AMD64 segment definitions.
 */

#ifndef __ARCH_GDT_H
#define __ARCH_GDT_H

/** Total number of GDT descriptors. */
#define GDT_SEG_COUNT	9

/** Segment definitions. Do not change without looking at SYSCALL stuff. */
#define SEG_K_CS	0x08		/**< Kernel code segment. */
#define SEG_K_DS	0x10		/**< Kernel data segment. */
#define SEG_U_DS	0x18		/**< User data segment. */
#define SEG_U_CS	0x20		/**< User code segment. */
#define SEG_K_CS32	0x28		/**< 32-bit kernel code segment. */
#define SEG_K_DS32	0x30		/**< 32-bit kernel data segment. */
#define SEG_TSS		0x38		/**< TSS segment. */

#ifndef __ASM__

#include <types.h>

extern gdt_pointer_t __boot_gdtp;

extern void gdt_set_base(int sel, ptr_t base);
extern void gdt_set_limit(int sel, size_t limit);

extern void gdt_init(void);

#endif /* __ASM__ */
#endif /* __ARCH_GDT_H */
