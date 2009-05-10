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

/** Register structure offsets. */
#define REGS_OFF_GS		0	/**< GS. */
#define REGS_OFF_FS		8	/**< FS. */
#define REGS_OFF_R15		16	/**< R15. */
#define REGS_OFF_R14		24	/**< R14. */
#define REGS_OFF_R13		32	/**< R13. */
#define REGS_OFF_R12		40	/**< R12. */
#define REGS_OFF_R11		48	/**< R11. */
#define REGS_OFF_R10		56	/**< R10. */
#define REGS_OFF_R9		64	/**< R9. */
#define REGS_OFF_R8		72	/**< R8. */
#define REGS_OFF_BP		80	/**< RBP. */
#define REGS_OFF_SI		88	/**< RSI. */
#define REGS_OFF_DI		96	/**< RDI. */
#define REGS_OFF_DX		104	/**< RDX. */
#define REGS_OFF_CX		112	/**< RCX. */
#define REGS_OFF_BX		120	/**< RBX. */
#define REGS_OFF_AX		128	/**< RAX. */
#define REGS_OFF_INT_NO		136	/**< Interrupt number. */
#define REGS_OFF_ERR_CODE	144	/**< Error code (if applicable). */
#define REGS_OFF_IP		152	/**< RIP. */
#define REGS_OFF_CS		160	/**< CS. */
#define REGS_OFF_FLAGS		168	/**< RFLAGS. */
#define REGS_OFF_SP		176	/**< RSP. */
#define REGS_OFF_SS		184	/**< SS. */

#ifndef __ASM__

#include <compiler.h>

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
typedef unsigned long unative_t;	/**< Unsigned native-size type. */
typedef signed long native_t;		/**< Signed native-size type. */

/** Integer type that can represent a virtual address. */
typedef unsigned long ptr_t;

/** Integer type that can represent a physical address (64-bit for PAE). */
typedef uint64_t phys_ptr_t;

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

/** Contains a copy of all the registers upon an interrupt. */
typedef struct regs {
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
} __packed regs_t;

/** GDT pointer loaded into the GDTR register. */
typedef struct gdt_ptr {
	uint16_t limit;			/**< Total size of GDT. */
	ptr_t base;			/**< Virtual address of GDT. */
} __packed gdt_ptr_t;

/** IDT pointer loaded into the IDTR register. */
typedef struct idt_ptr {
	uint16_t limit;			/**< Total size of IDT. */
	ptr_t base;			/**< Virtual address of IDT. */
} __packed idt_ptr_t;

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
typedef struct gdt_desc {
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
} __packed gdt_desc_t;

/** Structure of a TSS GDT descriptor. */
typedef struct gdt_tss_desc {
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
} __packed gdt_tss_desc_t;

/** Structure of an IDT descriptor. */
typedef struct idt_desc {
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned ist : 3;		/**< Interrupt Stack Table number. */
	unsigned unused : 5;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< Middle part of handler address. */
	unsigned base2 : 32;		/**< High part of handler address. */
	unsigned reserved : 32;		/**< Reserved. */
} __packed idt_desc_t;

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

#endif /* __ASM__ */
#endif /* __ARCH_TYPES_H */
