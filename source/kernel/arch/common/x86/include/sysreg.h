/* Kiwi x86 system register functions/definitions
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
 * @brief		x86 system register functions/definitions.
 *
 * This header contains a set of functions and definitions related to the
 * x86 CPU's system registers (See Section 2.1.6 in Intel Manual Volume 3A).
 */

#ifndef __ARCH_X86_SYSREG_H
#define __ARCH_X86_SYSREG_H

/** Macros to generate functions to access registers. */
#ifndef __ASM__
# include <types.h>
# define GEN_READ_REG(name, type)	\
	static inline type sysreg_ ## name ## _read(void) { \
		type r; \
		__asm__ volatile("mov %%" #name ", %0" : "=r"(r)); \
		return r; \
	}
# define GEN_WRITE_REG(name, type)	\
	static inline void sysreg_ ## name ## _write(type val) { \
		__asm__ volatile("mov %0, %%" #name :: "r"(val)); \
	}
#else
# define GEN_READ_REG(name, type)	
# define GEN_WRITE_REG(name, type)	
#endif

/** Flags in the CR0 Control Register. */
#define SYSREG_CR0_PE		(1<<0)		/**< Protected Mode Enable. */
#define SYSREG_CR0_MP		(1<<1)		/**< Monitor Coprocessor. */
#define SYSREG_CR0_EM		(1<<2)		/**< Emulation. */
#define SYSREG_CR0_TS		(1<<3)		/**< Task Switched. */
#define SYSREG_CR0_ET		(1<<4)		/**< Extension Type. */
#define SYSREG_CR0_NE		(1<<5)		/**< Numeric Error. */
#define SYSREG_CR0_WP		(1<<16)		/**< Write Protect. */
#define SYSREG_CR0_AM		(1<<18)		/**< Alignment Mask. */
#define SYSREG_CR0_NW		(1<<29)		/**< Not Write-through. */
#define SYSREG_CR0_CD		(1<<30)		/**< Cache Disable. */
#define SYSREG_CR0_PG		(1<<31)		/**< Paging Enable. */

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

/** Flags in the CR4 Control Register. */
#define SYSREG_CR4_VME		(1<<0)		/**< Virtual-8086 Mode Extensions. */
#define SYSREG_CR4_PVI		(1<<1)		/**< Protected Mode Virtual Interrupts. */
#define SYSREG_CR4_TSD		(1<<2)		/**< Time Stamp Disable. */
#define SYSREG_CR4_DE		(1<<3)		/**< Debugging Extensions. */
#define SYSREG_CR4_PSE		(1<<4)		/**< Page Size Extensions. */
#define SYSREG_CR4_PAE		(1<<5)		/**< Physical Address Extension. */
#define SYSREG_CR4_MCE		(1<<6)		/**< Machine Check Enable. */
#define SYSREG_CR4_PGE		(1<<7)		/**< Page Global Enable. */
#define SYSREG_CR4_PCE		(1<<8)		/**< Performance-Monitoring Counter Enable. */
#define SYSREG_CR4_OSFXSR	(1<<9)		/**< OS Support for FXSAVE/FXRSTOR. */
#define SYSREG_CR4_OSXMMEXCPT	(1<<10)		/**< OS Support for Unmasked SIMD FPU Exceptions. */
#define SYSREG_CR4_VMXE		(1<<13)		/**< VMX-Enable Bit. */
#define SYSREG_CR4_SMXE		(1<<14)		/**< SMX-Enable Bit. */

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

/** Flags in the debug status register (DR6). */
#define SYSREG_DR6_B0		(1<<0)		/**< Breakpoint 0 condition detected. */
#define SYSREG_DR6_B1		(1<<1)		/**< Breakpoint 1 condition detected. */
#define SYSREG_DR6_B2		(1<<2)		/**< Breakpoint 2 condition detected. */
#define SYSREG_DR6_B3		(1<<3)		/**< Breakpoint 3 condition detected. */
#define SYSREG_DR6_BD		(1<<13)		/**< Debug register access. */
#define SYSREG_DR6_BS		(1<<14)		/**< Single-stepped. */
#define SYSREG_DR6_BT		(1<<15)		/**< Task switch. */

/** Read the DR6 register.
 * @return		Value of the DR6 register. */
GEN_READ_REG(dr6, unative_t);

/** Write the DR6 register.
 * @param val		New value of the DR6 register. */
GEN_WRITE_REG(dr6, unative_t);

/** Flags in the debug control register (DR7). */
#define SYSREG_DR7_G0		(1<<1)		/**< Global breakpoint 0 enable. */
#define SYSREG_DR7_G1		(1<<3)		/**< Global breakpoint 1 enable. */
#define SYSREG_DR7_G2		(1<<5)		/**< Global breakpoint 2 enable. */
#define SYSREG_DR7_G3		(1<<7)		/**< Global breakpoint 3 enable. */

/** Read the DR7 register.
 * @return		Value of the DR7 register. */
GEN_READ_REG(dr7, unative_t);

/** Write the DR7 register.
 * @param val		New value of the DR7 register. */
GEN_WRITE_REG(dr7, unative_t);

/** Definitions for bits in the EFLAGS/RFLAGS register. */
#define SYSREG_FLAGS_CF		(1<<0)		/**< Carry Flag. */
#define SYSREG_FLAGS_ALWAYS1	(1<<1)		/**< Flag that must always be 1. */
#define SYSREG_FLAGS_PF		(1<<2)		/**< Parity Flag. */
#define SYSREG_FLAGS_AF		(1<<4)		/**< Auxilary Carry Flag. */
#define SYSREG_FLAGS_ZF		(1<<6)		/**< Zero Flag. */
#define SYSREG_FLAGS_SF		(1<<7)		/**< Sign Flag. */
#define SYSREG_FLAGS_TF		(1<<8)		/**< Trap Flag. */
#define SYSREG_FLAGS_IF		(1<<9)		/**< Interrupt Enable Flag. */
#define SYSREG_FLAGS_DF		(1<<10)		/**< Direction Flag. */
#define SYSREG_FLAGS_OF		(1<<11)		/**< Overflow Flag. */
#define SYSREG_FLAGS_NT		(1<<14)		/**< Nested Task Flag. */
#define SYSREG_FLAGS_RF		(1<<16)		/**< Resume Flag. */
#define SYSREG_FLAGS_VM		(1<<17)		/**< Virtual-8086 Mode. */
#define SYSREG_FLAGS_AC		(1<<18)		/**< Alignment Check. */
#define SYSREG_FLAGS_VIF	(1<<19)		/**< Virtual Interrupt Flag. */
#define SYSREG_FLAGS_VIP	(1<<20)		/**< Virtual Interrupt Pending Flag. */
#define SYSREG_FLAGS_ID		(1<<21)		/**< ID Flag. */

#ifndef __ASM__
/** Get current value of EFLAGS/RFLAGS.
 * @return		Current value of EFLAGS/RFLAGS. */
static inline unative_t sysreg_flags_read(void) {
	unative_t val;

	__asm__ volatile("pushf; pop %0" : "=rm"(val));
	return val;
}

/** Set value of EFLAGS/RFLAGS.
 * @param val		New value for EFLAGS/RFLAGS. */
static inline void sysreg_flags_write(unative_t val) {
	__asm__ volatile("push %0; popf" :: "rm"(val));
}
#endif /* __ASM__ */

/** Model Specific Registers. */
#define SYSREG_MSR_APIC_BASE	0x1b		/**< LAPIC base address. */
#define SYSREG_MSR_EFER		0xc0000080	/**< Extended Feature Enable register. */
#define SYSREG_MSR_STAR		0xc0000081	/**< System Call Target Address. */
#define SYSREG_MSR_LSTAR	0xc0000082	/**< 64-bit System Call Target Address. */
#define SYSREG_MSR_FMASK	0xc0000084	/**< System Call Flag Mask. */
#define SYSREG_MSR_GS_BASE	0xc0000101	/**< GS segment base register. */

/** EFER MSR flags. */
#define SYSREG_EFER_SCE		(1<<0)		/**< System Call Enable. */
#define SYSREG_EFER_LME		(1<<8)		/**< Long Mode (IA-32e) Enable. */
#define SYSREG_EFER_LMA		(1<<10)		/**< Long Mode (IA-32e) Active. */
#define SYSREG_EFER_NXE		(1<<11)		/**< Execute Disable (XD/NX) Bit Enable. */

#ifndef __ASM__
/** Read an MSR.
 * @param msr		MSR to read.
 * @return		Value read from the MSR. */
static inline uint64_t sysreg_msr_read(uint32_t msr) {
	uint32_t low, high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((uint64_t)high) << 32) | low;
}

/** Write an MSR.
 * @param msr		MSR to write to.
 * @param value		Value to write to the MSR. */
static inline void sysreg_msr_write(uint32_t msr, uint64_t value) {
	__asm__ volatile("wrmsr" :: "a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}
#endif /* __ASM__ */

#undef GEN_READ_REG
#undef GEN_WRITE_REG
#endif /* __ARCH_X86_SYSREG_H */
