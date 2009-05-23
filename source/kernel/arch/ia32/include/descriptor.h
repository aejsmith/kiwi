/* Kiwi IA32 descriptor table functions
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
 * @brief		IA32 descriptor table functions.
 */

#ifndef __ARCH_DESCRIPTOR_H
#define __ARCH_DESCRIPTOR_H

/** Total number of GDT descriptors. */
#define GDT_ENTRY_COUNT	7

/** Total number of IDT descriptors. */
#define IDT_ENTRY_COUNT	256

/** Segment definitions. Do not change without looking at SYSCALL stuff. */
#define SEG_K_CS	0x08		/**< Kernel code segment. */
#define SEG_K_DS	0x10		/**< Kernel data segment. */
#define SEG_U_CS	0x18		/**< User code segment. */
#define SEG_U_DS	0x20		/**< User data segment. */
#define SEG_TSS		0x28		/**< TSS segment. */
#define SEG_DF_TSS	0x30		/**< Double fault TSS segment. */

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
	uint16_t backlink, __blh;
	uint32_t esp0;
	uint16_t ss0, __ss0h;
	uint32_t esp1;
	uint16_t ss1, __ss1h;
	uint32_t esp2;
	uint16_t ss2, __ss2h;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax, ecx, edx, ebx;
	uint32_t esp, ebp, esi, edi;
	uint16_t es, __esh;
	uint16_t cs, __csh;
	uint16_t ss, __ssh;
	uint16_t ds, __dsh;
	uint16_t fs, __fsh;
	uint16_t gs, __gsh;
	uint16_t ldt, __ldth;
	uint16_t trace, io_bitmap;
} __packed tss_t;

/** Structure of a GDT descriptor. */
typedef struct gdt_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 16;		/**< Low part of base. */
	unsigned base1 : 8;		/**< Middle part of base. */
	unsigned access : 8;		/**< Access flags. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned available : 1;		/**< Spare bit. */
	unsigned unused : 1;		/**< Unused. */
	unsigned special : 1;		/**< Special. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base2 : 8;		/**< High part of base. */
} __packed gdt_entry_t;

/** Structure of an IDT descriptor. */
typedef struct idt_entry {
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned unused : 8;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< High part of handler address. */
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
