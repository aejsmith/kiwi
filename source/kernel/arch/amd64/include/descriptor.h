/* Kiwi AMD64 descriptor table functions
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
 * @brief		AMD64 descriptor table functions.
 */

#ifndef __ARCH_DESCRIPTOR_H
#define __ARCH_DESCRIPTOR_H

/** Total number of GDT descriptors. */
#define GDT_ENTRY_COUNT	9

/** Total number of IDT descriptors. */
#define IDT_ENTRY_COUNT	256

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

/** GDT pointer loaded into the GDTR register. */
typedef struct gdt_pointer {
	uint16_t limit;			/**< Total size of GDT. */
	ptr_t base;			/**< Virtual address of GDT. */
} __packed gdt_pointer_t;

/** IDT pointer loaded into the IDTR register. */
typedef struct idt_pointer {
	uint16_t limit;			/**< Total size of IDT. */
	ptr_t base;			/**< Virtual address of IDT. */
} __packed idt_pointer_t;

/** Task State Segment structure. */
typedef struct tss {
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
} __packed tss_t;

/** Structure of a GDT descriptor. */
typedef struct gdt_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 16;		/**< Low part of base. */
	unsigned base1 : 8;		/**< Middle part of base. */
	unsigned access : 8;		/**< Access flags. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned available : 1;		/**< Spare bit. */
	unsigned longmode : 1;		/**< 64-bit code segment. */
	unsigned special : 1;		/**< Special. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base2 : 8;		/**< High part of base. */
} __packed gdt_entry_t;

/** Structure of a TSS GDT entry. */
typedef struct gdt_tss_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 16;		/**< Part 1 of base. */
	unsigned base1 : 8;		/**< Part 2 of base. */
	unsigned type : 4;		/**< Type flag. */
	unsigned : 1;			/**< Unused. */
	unsigned dpl : 2;		/**< Descriptor privilege level. */
	unsigned present : 1;		/**< Present. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned available : 1;		/**< Spare bit. */
	unsigned : 2;			/**< Unused. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base2 : 8;		/**< Part 3 of base. */
	unsigned base3 : 32;		/**< Part 4 of base. */
	unsigned : 32;			/**< Unused. */
} __packed gdt_tss_entry_t;

/** Structure of an IDT entry. */
typedef struct idt_entry {
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned ist : 3;		/**< Interrupt Stack Table number. */
	unsigned unused : 5;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< Middle part of handler address. */
	unsigned base2 : 32;		/**< High part of handler address. */
	unsigned reserved : 32;		/**< Reserved. */
} __packed idt_entry_t;

/** Load a value into TR (Task Register).
 * @param sel		Selector to load. */
static inline void ltr(uint32_t sel) {
	__asm__ volatile("ltr %%ax" :: "a"(sel));
}

/** Set the GDTR register.
 * @param base		Virtual address of GDT.
 * @param limit		Size of GDT. */
static inline void lgdt(ptr_t base, uint16_t limit) {
	gdt_pointer_t gdtp;

	gdtp.limit = limit;
	gdtp.base = base;

	__asm__ volatile("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base		Base address of IDT.
 * @param limit		Size of IDT. */
static inline void lidt(ptr_t base, uint16_t limit) {
	idt_pointer_t idtp;

	idtp.limit = limit;
	idtp.base = base;

	__asm__ volatile("lidt %0" :: "m"(idtp));
}

extern gdt_pointer_t __boot_gdtp;

extern void descriptor_init(void);
extern void descriptor_ap_init(void);

#endif /* __ASM__ */
#endif /* __ARCH_DESCRIPTOR_H */
