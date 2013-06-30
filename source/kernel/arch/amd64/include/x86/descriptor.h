/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		x86 descriptor table functions.
 */

#ifndef __X86_DESCRIPTOR_H
#define __X86_DESCRIPTOR_H

/** Descriptor table sizes. */
#define GDT_ENTRY_COUNT		7	/**< Total number of GDT entries. */
#define IDT_ENTRY_COUNT		256	/**< Total number of IDT entries. */

/** Segment definitions.
 * @note		The ordering of these is important to SYSCALL/SYSRET. */
#define KERNEL_CS		0x08	/**< Kernel code segment. */
#define KERNEL_DS		0x10	/**< Kernel data segment. */
#define USER_DS			0x18	/**< User data segment. */
#define USER_CS			0x20	/**< User code segment. */
#define KERNEL_TSS		0x28	/**< TSS segment (takes up 2 GDT entries). */

#ifndef __ASM__

#include <types.h>

struct cpu;

/** Task State Segment structure. */
typedef struct __packed tss {
	uint32_t _reserved1;		/**< Reserved. */
	uint64_t rsp0;			/**< Ring 0 RSP. */
	uint64_t rsp1;			/**< Ring 1 RSP. */
	uint64_t rsp2;			/**< Ring 2 RSP. */
	uint64_t _reserved2;		/**< Reserved. */
	uint64_t ist1;			/**< IST1. */
	uint64_t ist2;			/**< IST2. */
	uint64_t ist3;			/**< IST3. */
	uint64_t ist4;			/**< IST4. */
	uint64_t ist5;			/**< IST5. */
	uint64_t ist6;			/**< IST6. */
	uint64_t ist7;			/**< IST7. */
	uint64_t _reserved3;		/**< Reserved. */
	uint16_t _reserved4;		/**< Reserved. */
	uint16_t io_bitmap;		/**< I/O map base address. */
} tss_t;

/** Structure of a GDT descriptor. */
typedef struct __packed gdt_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 24;		/**< Low part of base. */
	unsigned type : 4;		/**< Type flag. */
	unsigned s : 1;			/**< S (descriptor type) flag. */
	unsigned dpl : 2;		/**< Descriptor privilege level. */
	unsigned present : 1;		/**< Present. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned : 1;			/**< Spare bit. */
	unsigned longmode : 1;		/**< 64-bit code segment. */
	unsigned special : 1;		/**< Special. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base1 : 8;		/**< High part of base. */
} gdt_entry_t;

/** Structure of a TSS GDT entry. */
typedef struct __packed gdt_tss_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 24;		/**< Part 1 of base. */
	unsigned type : 4;		/**< Type flag. */
	unsigned : 1;			/**< Unused. */
	unsigned dpl : 2;		/**< Descriptor privilege level. */
	unsigned present : 1;		/**< Present. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned available : 1;		/**< Spare bit. */
	unsigned : 2;			/**< Unused. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base1 : 8;		/**< Part 2 of base. */
	unsigned base2 : 32;		/**< Part 3 of base. */
	unsigned : 32;			/**< Unused. */
} gdt_tss_entry_t;

/** Structure of an IDT entry. */
typedef struct __packed idt_entry {
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned ist : 3;		/**< Interrupt Stack Table number. */
	unsigned unused : 5;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< Middle part of handler address. */
	unsigned base2 : 32;		/**< High part of handler address. */
	unsigned reserved : 32;		/**< Reserved. */
} idt_entry_t;

/** Load a value into TR (Task Register).
 * @param sel		Selector to load. */
static inline void x86_ltr(uint32_t sel) {
	__asm__ volatile("ltr %%ax" :: "a"(sel));
}

/** Set the GDTR register.
 * @param base		Virtual address of GDT.
 * @param limit		Size of GDT. */
static inline void x86_lgdt(gdt_entry_t *base, uint16_t limit) {
	struct { uint16_t limit; ptr_t base; } __packed gdtp = {
		limit, (ptr_t)base
	};

	__asm__ volatile("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base		Base address of IDT.
 * @param limit		Size of IDT. */
static inline void x86_lidt(idt_entry_t *base, uint16_t limit) {
	struct { uint16_t limit; ptr_t base; } __packed idtp = {
		limit, (ptr_t)base
	};

	__asm__ volatile("lidt %0" :: "m"(idtp));
}

extern void descriptor_init(struct cpu *cpu);
extern void idt_init(void);

#endif /* __ASM__ */
#endif /* __X86_DESCRIPTOR_H */
