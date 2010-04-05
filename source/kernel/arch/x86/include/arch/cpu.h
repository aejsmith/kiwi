/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 CPU management.
 */

#ifndef __ARCH_CPU_H
#define __ARCH_CPU_H

/** Flags in the CR0 Control Register. */
#define X86_CR0_PE		(1<<0)		/**< Protected Mode Enable. */
#define X86_CR0_MP		(1<<1)		/**< Monitor Coprocessor. */
#define X86_CR0_EM		(1<<2)		/**< Emulation. */
#define X86_CR0_TS		(1<<3)		/**< Task Switched. */
#define X86_CR0_ET		(1<<4)		/**< Extension Type. */
#define X86_CR0_NE		(1<<5)		/**< Numeric Error. */
#define X86_CR0_WP		(1<<16)		/**< Write Protect. */
#define X86_CR0_AM		(1<<18)		/**< Alignment Mask. */
#define X86_CR0_NW		(1<<29)		/**< Not Write-through. */
#define X86_CR0_CD		(1<<30)		/**< Cache Disable. */
#define X86_CR0_PG		(1<<31)		/**< Paging Enable. */

/** Flags in the CR4 Control Register. */
#define X86_CR4_VME		(1<<0)		/**< Virtual-8086 Mode Extensions. */
#define X86_CR4_PVI		(1<<1)		/**< Protected Mode Virtual Interrupts. */
#define X86_CR4_TSD		(1<<2)		/**< Time Stamp Disable. */
#define X86_CR4_DE		(1<<3)		/**< Debugging Extensions. */
#define X86_CR4_PSE		(1<<4)		/**< Page Size Extensions. */
#define X86_CR4_PAE		(1<<5)		/**< Physical Address Extension. */
#define X86_CR4_MCE		(1<<6)		/**< Machine Check Enable. */
#define X86_CR4_PGE		(1<<7)		/**< Page Global Enable. */
#define X86_CR4_PCE		(1<<8)		/**< Performance-Monitoring Counter Enable. */
#define X86_CR4_OSFXSR		(1<<9)		/**< OS Support for FXSAVE/FXRSTOR. */
#define X86_CR4_OSXMMEXCPT	(1<<10)		/**< OS Support for Unmasked SIMD FPU Exceptions. */
#define X86_CR4_VMXE		(1<<13)		/**< VMX-Enable Bit. */
#define X86_CR4_SMXE		(1<<14)		/**< SMX-Enable Bit. */

/** Flags in the debug status register (DR6). */
#define X86_DR6_B0		(1<<0)		/**< Breakpoint 0 condition detected. */
#define X86_DR6_B1		(1<<1)		/**< Breakpoint 1 condition detected. */
#define X86_DR6_B2		(1<<2)		/**< Breakpoint 2 condition detected. */
#define X86_DR6_B3		(1<<3)		/**< Breakpoint 3 condition detected. */
#define X86_DR6_BD		(1<<13)		/**< Debug register access. */
#define X86_DR6_BS		(1<<14)		/**< Single-stepped. */
#define X86_DR6_BT		(1<<15)		/**< Task switch. */

/** Flags in the debug control register (DR7). */
#define X86_DR7_G0		(1<<1)		/**< Global breakpoint 0 enable. */
#define X86_DR7_G1		(1<<3)		/**< Global breakpoint 1 enable. */
#define X86_DR7_G2		(1<<5)		/**< Global breakpoint 2 enable. */
#define X86_DR7_G3		(1<<7)		/**< Global breakpoint 3 enable. */

/** Definitions for bits in the EFLAGS/RFLAGS register. */
#define X86_FLAGS_CF		(1<<0)		/**< Carry Flag. */
#define X86_FLAGS_ALWAYS1	(1<<1)		/**< Flag that must always be 1. */
#define X86_FLAGS_PF		(1<<2)		/**< Parity Flag. */
#define X86_FLAGS_AF		(1<<4)		/**< Auxilary Carry Flag. */
#define X86_FLAGS_ZF		(1<<6)		/**< Zero Flag. */
#define X86_FLAGS_SF		(1<<7)		/**< Sign Flag. */
#define X86_FLAGS_TF		(1<<8)		/**< Trap Flag. */
#define X86_FLAGS_IF		(1<<9)		/**< Interrupt Enable Flag. */
#define X86_FLAGS_DF		(1<<10)		/**< Direction Flag. */
#define X86_FLAGS_OF		(1<<11)		/**< Overflow Flag. */
#define X86_FLAGS_NT		(1<<14)		/**< Nested Task Flag. */
#define X86_FLAGS_RF		(1<<16)		/**< Resume Flag. */
#define X86_FLAGS_VM		(1<<17)		/**< Virtual-8086 Mode. */
#define X86_FLAGS_AC		(1<<18)		/**< Alignment Check. */
#define X86_FLAGS_VIF		(1<<19)		/**< Virtual Interrupt Flag. */
#define X86_FLAGS_VIP		(1<<20)		/**< Virtual Interrupt Pending Flag. */
#define X86_FLAGS_ID		(1<<21)		/**< ID Flag. */

/** Model Specific Registers. */
#define X86_MSR_TSC		0x10		/**< Time Stamp Counter (TSC). */
#define X86_MSR_APIC_BASE	0x1b		/**< LAPIC base address. */
#define X86_MSR_EFER		0xc0000080	/**< Extended Feature Enable register. */
#define X86_MSR_STAR		0xc0000081	/**< System Call Target Address. */
#define X86_MSR_LSTAR		0xc0000082	/**< 64-bit System Call Target Address. */
#define X86_MSR_FMASK		0xc0000084	/**< System Call Flag Mask. */
#define X86_MSR_GS_BASE		0xc0000101	/**< GS segment base register. */
#define X86_MSR_K_GS_BASE	0xc0000102	/**< GS base to switch to with SWAPGS. */

/** EFER MSR flags. */
#define X86_EFER_SCE		(1<<0)		/**< System Call Enable. */
#define X86_EFER_LME		(1<<8)		/**< Long Mode (IA-32e) Enable. */
#define X86_EFER_LMA		(1<<10)		/**< Long Mode (IA-32e) Active. */
#define X86_EFER_NXE		(1<<11)		/**< Execute Disable (XD/NX) Bit Enable. */

#ifndef __ASM__

#include <types.h>

#ifndef LOADER

#include <arch/descriptor.h>
#include <arch/memmap.h>
#include <arch/stack.h>

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Architecture-specific CPU structure. */
typedef struct cpu_arch {
	/** Time conversion factors. */
	uint64_t cycles_per_us;			/**< CPU cycles per µs. */
	uint64_t lapic_timer_cv;		/**< LAPIC timer conversion factor. */

	/** Per-CPU CPU structures. */
	gdt_entry_t gdt[GDT_ENTRY_COUNT];	/**< Array of GDT descriptors. */
	tss_t tss;				/**< Task State Segment (TSS). */
#ifndef __x86_64__
	tss_t double_fault_tss;			/**< Double fault TSS. */
#endif
	void *double_fault_stack;		/**< Pointer to the stack for double faults. */

	/** Basic CPU information. */
	uint64_t cpu_freq;			/**< CPU frequency in Hz. */
	uint64_t lapic_freq;			/**< LAPIC timer frequency in Hz. */
	char model_name[64];			/**< CPU model name. */
	uint8_t family;				/**< CPU family. */
	uint8_t model;				/**< CPU model. */
	uint8_t stepping;			/**< CPU stepping. */
	int cache_alignment;			/**< Cache line size. */

	/** Feature information. */
	uint32_t largest_standard;		/**< Largest standard function. */
	uint32_t feat_ecx;			/**< ECX value of function 01h. */
	uint32_t feat_edx;			/**< EDX value of function 01h. */
	uint32_t largest_extended;		/**< Largest extended function. */
	uint32_t ext_ecx;			/**< ECX value of function 80000000h. */
	uint32_t ext_edx;			/**< ECX value of function 80000000h. */
} cpu_arch_t;

/** Get the current CPU structure pointer from the base of the stack.
 * @return		Pointer to current CPU structure. */
static inline ptr_t cpu_get_pointer(void) {
	return *(ptr_t *)stack_get_base();
}

/** Set the current CPU structure pointer at the base of the stack.
 * @param addr		Pointer to new CPU structure. */
static inline void cpu_set_pointer(ptr_t addr) {
	*(ptr_t *)stack_get_base() = addr;
}

#endif /* LOADER */

/** Halt the current CPU. */
static inline __noreturn void cpu_halt(void) {
	while(true) {
		__asm__ volatile("cli; hlt");
	}
}

/** Place the CPU in an idle state until an interrupt occurs. */
static inline void cpu_idle(void) {
	__asm__ volatile("sti; hlt; cli");
}

/** Spin loop hint using the PAUSE instruction.
 * @note		See PAUSE instruction in Intel 64 and IA-32
 *			Architectures Software Developer's Manual, Volume 2B:
 *			Instruction Set Reference N-Z for more information as
 *			to why this function is necessary. */
static inline void spin_loop_hint(void) {
	__asm__ volatile("pause");
}

/** Macros to generate functions to access registers. */
#define GEN_READ_REG(name, type)	\
	static inline type x86_read_ ## name (void) { \
		type r; \
		__asm__ volatile("mov %%" #name ", %0" : "=r"(r)); \
		return r; \
	}
#define GEN_WRITE_REG(name, type)	\
	static inline void x86_write_ ## name (type val) { \
		__asm__ volatile("mov %0, %%" #name :: "r"(val)); \
	}

/** Read the CR0 register.
 * @return		Value of the CR0 register. */
GEN_READ_REG(cr0, unative_t);

/** Write the CR0 register.
 * @param val		New value of the CR0 register. */
GEN_WRITE_REG(cr0, unative_t);

/** Read the CR2 register.
 * @return		Value of the CR2 register. */
GEN_READ_REG(cr2, unative_t);

/** Read the CR3 register.
 * @return		Value of the CR3 register. */
GEN_READ_REG(cr3, unative_t);

/** Write the CR3 register.
 * @param val		New value of the CR3 register. */
GEN_WRITE_REG(cr3, unative_t);

/** Read the CR4 register.
 * @return		Value of the CR4 register. */
GEN_READ_REG(cr4, unative_t);

/** Write the CR4 register.
 * @param val		New value of the CR4 register. */
GEN_WRITE_REG(cr4, unative_t);

/** Read the DR0 register.
 * @return		Value of the DR0 register. */
GEN_READ_REG(dr0, unative_t);

/** Write the DR0 register.
 * @param val		New value of the DR0 register. */
GEN_WRITE_REG(dr0, unative_t);

/** Read the DR1 register.
 * @return		Value of the DR1 register. */
GEN_READ_REG(dr1, unative_t);

/** Write the DR1 register.
 * @param val		New value of the DR1 register. */
GEN_WRITE_REG(dr1, unative_t);

/** Read the DR2 register.
 * @return		Value of the DR2 register. */
GEN_READ_REG(dr2, unative_t);

/** Write the DR2 register.
 * @param val		New value of the DR2 register. */
GEN_WRITE_REG(dr2, unative_t);

/** Read the DR3 register.
 * @return		Value of the DR3 register. */
GEN_READ_REG(dr3, unative_t);

/** Write the DR3 register.
 * @param val		New value of the DR3 register. */
GEN_WRITE_REG(dr3, unative_t);

/** Read the DR6 register.
 * @return		Value of the DR6 register. */
GEN_READ_REG(dr6, unative_t);

/** Write the DR6 register.
 * @param val		New value of the DR6 register. */
GEN_WRITE_REG(dr6, unative_t);

/** Read the DR7 register.
 * @return		Value of the DR7 register. */
GEN_READ_REG(dr7, unative_t);

/** Write the DR7 register.
 * @param val		New value of the DR7 register. */
GEN_WRITE_REG(dr7, unative_t);

#undef GEN_READ_REG
#undef GEN_WRITE_REG

/** Get current value of EFLAGS/RFLAGS.
 * @return		Current value of EFLAGS/RFLAGS. */
static inline unative_t x86_read_flags(void) {
	unative_t val;

	__asm__ volatile("pushf; pop %0" : "=rm"(val));
	return val;
}

/** Set value of EFLAGS/RFLAGS.
 * @param val		New value for EFLAGS/RFLAGS. */
static inline void x86_write_flags(unative_t val) {
	__asm__ volatile("push %0; popf" :: "rm"(val));
}
/** Read an MSR.
 * @param msr		MSR to read.
 * @return		Value read from the MSR. */
static inline uint64_t x86_read_msr(uint32_t msr) {
	uint32_t low, high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((uint64_t)high) << 32) | low;
}

/** Write an MSR.
 * @param msr		MSR to write to.
 * @param value		Value to write to the MSR. */
static inline void x86_write_msr(uint32_t msr, uint64_t value) {
	__asm__ volatile("wrmsr" :: "a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

#endif /* __ASM__ */
#endif /* __ARCH_CPU_H */
