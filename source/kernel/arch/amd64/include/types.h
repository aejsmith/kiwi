/* Kiwi AMD64 type definitions
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
 * @brief		AMD64 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

#include <compiler.h>

/** Format character definitions for kprintf(). */
#define PRIu8		"u"		/**< Format for uint8_t. */
#define PRIu16		"u"		/**< Format for uint16_t. */
#define PRIu32		"u"		/**< Format for uint32_t. */
#define PRIu64		"llu"		/**< Format for uint64_t. */
#define PRIun		"lu"		/**< Format for unative_t. */
#define PRId8		"d"		/**< Format for int8_t. */
#define PRId16		"d"		/**< Format for int16_t. */
#define PRId32		"d"		/**< Format for int32_t. */
#define PRId64		"lld"		/**< Format for int64_t. */
#define PRIdn		"d"		/**< Format for native_t. */
#define PRIx8		"x"		/**< Format for (u)int8_t (hexadecimal). */
#define PRIx16		"x"		/**< Format for (u)int16_t (hexadecimal). */
#define PRIx32		"x"		/**< Format for (u)int32_t (hexadecimal). */
#define PRIx64		"llx"		/**< Format for (u)int64_t (hexadecimal). */
#define PRIxn		"lx"		/**< Format for (u)native_t (hexadecimal). */
#define PRIo8		"o"		/**< Format for (u)int8_t (octal). */
#define PRIo16		"o"		/**< Format for (u)int16_t (octal). */
#define PRIo32		"o"		/**< Format for (u)int32_t (octal). */
#define PRIo64		"llo"		/**< Format for (u)int64_t (octal). */
#define PRIon		"lo"		/**< Format for (u)native_t (octal). */
#define PRIpp		"llx"		/**< Format for phys_ptr_t. */
#define PRIs		"lu"		/**< Format for size_t. */

/** Unsigned data types. */
typedef unsigned char uint8_t;		/**< Unsigned 8-bit. */
typedef unsigned short uint16_t;	/**< Unsigned 16-bit. */
typedef unsigned int uint32_t;		/**< Unsigned 32-bit. */
typedef unsigned long long uint64_t;	/**< Unsigned 64-bit. */

/** Signed data types. */
typedef signed char int8_t;		/**< Signed 8-bit. */
typedef signed short int16_t;		/**< Signed 16-bit. */
typedef signed int int32_t;		/**< Signed 32-bit. */
typedef signed long long int64_t;	/**< Signed 64-bit. */

/** Native-sized types. */
typedef unsigned long unative_t;	/**< Unsigned native-sized type. */
typedef signed long native_t;		/**< Signed native-sized type. */

/** Integer type that can represent a virtual address. */
typedef unsigned long ptr_t;

/** Integer type that can represent a physical address. */
typedef uint64_t phys_ptr_t;

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t r15;			/**< R15. */
	unative_t r14;			/**< R14. */
	unative_t r13;			/**< R13. */
	unative_t r12;			/**< R12. */
	unative_t r11;			/**< R11. */
	unative_t r10;			/**< R10. */
	unative_t r9;			/**< R9. */
	unative_t r8;			/**< R8. */
	unative_t bp;			/**< RBP. */
	unative_t si;			/**< RSI. */
	unative_t di;			/**< RDI. */
	unative_t dx;			/**< RDX. */
	unative_t cx;			/**< RCX. */
	unative_t bx;			/**< RBX. */
	unative_t ax;			/**< RAX. */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< RIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< RFLAGS. */
	unative_t sp;			/**< RSP. */
	unative_t ss;			/**< SS. */
} __packed intr_frame_t;

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

/** Structure of a page table entry. */
typedef struct pte {
	unsigned present : 1;		/**< Present (P) flag. */
	unsigned writable : 1;		/**< Read/write (R/W) flag. */
	unsigned user : 1;		/**< User/supervisor (U/S) flag. */
	unsigned pwt : 1;		/**< Page-level write through (PWT) flag. */
	unsigned pcd : 1;		/**< Page-level cache disable (PCD) flag. */
	unsigned accessed : 1;		/**< Accessed (A) flag. */
	unsigned dirty : 1;		/**< Dirty (D) flag. */
	unsigned large : 1;		/**< Page size (PS) flag (page directory). */
	unsigned global : 1;		/**< Global (G) flag. */
	unsigned avail1 : 3;		/**< Available for use. */
	unsigned address : 28;		/**< Page base address. */
	unsigned reserved: 12;		/**< Reserved. */
	unsigned avail2 : 11;		/**< Available for use. */
	unsigned noexec : 1;		/**< No-Execute (NX) flag. */
} __packed pte_t;

/** Type that allows a page table entry to be accessed as a single value. */
typedef uint64_t pte_simple_t;

#endif /* __ARCH_TYPES_H */
