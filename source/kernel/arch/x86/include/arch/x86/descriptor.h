/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#ifndef __ARCH_X86_DESCRIPTOR_H
#define __ARCH_X86_DESCRIPTOR_H

#ifndef LOADER
# if __x86_64__
#  define GDT_ENTRY_COUNT	7	/**< Total number of GDT entries. */
# else
#  define GDT_ENTRY_COUNT	9	/**< Total number of GDT entries. */
# endif
# define IDT_ENTRY_COUNT	256	/**< Total number of IDT entries. */
# if __x86_64__
#  define SEGMENT_K_CS		0x08	/**< Kernel code segment. */
#  define SEGMENT_K_DS		0x10	/**< Kernel data segment. */
#  define SEGMENT_U_DS		0x18	/**< User data segment. */
#  define SEGMENT_U_CS		0x20	/**< User code segment. */
#  define SEGMENT_TSS		0x28	/**< TSS segment (takes up 2 entries). */
# else
#  define SEGMENT_K_CS		0x08	/**< Kernel code segment. */
#  define SEGMENT_K_DS		0x10	/**< Kernel data segment. */
#  define SEGMENT_K_GS		0x18	/**< Kernel GS (CPU pointer) segment. */
#  define SEGMENT_U_CS		0x20	/**< User code segment. */
#  define SEGMENT_U_DS		0x28	/**< User data segment. */
#  define SEGMENT_U_GS		0x30	/**< User GS (TLS) segment. */
#  define SEGMENT_TSS		0x38	/**< TSS segment. */
#  define SEGMENT_DF_TSS	0x40	/**< Double fault TSS segment. */
# endif
#endif

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
#ifdef __x86_64__
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
#else
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
#endif
} __packed tss_t;

/** Structure of a GDT descriptor. */
typedef struct gdt_entry {
#ifdef __x86_64__
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
#else
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
#endif
} __packed gdt_entry_t;

#ifdef __x86_64__
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
#endif

/** Structure of an IDT entry. */
typedef struct idt_entry {
#ifdef __x86_64__
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned ist : 3;		/**< Interrupt Stack Table number. */
	unsigned unused : 5;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< Middle part of handler address. */
	unsigned base2 : 32;		/**< High part of handler address. */
	unsigned reserved : 32;		/**< Reserved. */
#else
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned unused : 8;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< High part of handler address. */
#endif
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
	gdt_pointer_t gdtp = { limit, base };
	__asm__ volatile("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base		Base address of IDT.
 * @param limit		Size of IDT. */
static inline void lidt(ptr_t base, uint16_t limit) {
	idt_pointer_t idtp = { limit, base };
	__asm__ volatile("lidt %0" :: "m"(idtp));
}

#ifndef LOADER
struct cpu;

#ifndef __x86_64__
extern void gdt_set_base(struct cpu *cpu, int sel, ptr_t base);
extern void gdt_set_limit(struct cpu *cpu, int sel, size_t limit);
#endif
extern void descriptor_init(struct cpu *cpu);
#endif
#endif /* __ASM__ */
#endif /* __ARCH_X86_DESCRIPTOR_H */
