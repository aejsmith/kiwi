/* Kiwi x86 CPU definitions
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		x86 CPU definitions.
 */

#ifndef __ARCH_DEFS_H
#define __ARCH_DEFS_H

/*
 * FLAGS register.
 */

/** Flags in the FLAGS register. */
#define X86_FLAGS_CF		(1<<0)			/**< Carry Flag. */
#define X86_FLAGS_ALWAYS1	(1<<1)			/**< Flag that must always be 1. */
#define X86_FLAGS_PF		(1<<2)			/**< Parity Flag. */
#define X86_FLAGS_AF		(1<<4)			/**< Auxilary Carry Flag. */
#define X86_FLAGS_ZF		(1<<6)			/**< Zero Flag. */
#define X86_FLAGS_SF		(1<<7)			/**< Sign Flag. */
#define X86_FLAGS_TF		(1<<8)			/**< Trap Flag. */
#define X86_FLAGS_IF		(1<<9)			/**< Interrupt Enable Flag. */
#define X86_FLAGS_DF		(1<<10)			/**< Direction Flag. */
#define X86_FLAGS_OF		(1<<11)			/**< Overflow Flag. */
#define X86_FLAGS_NT		(1<<14)			/**< Nested Task Flag. */
#define X86_FLAGS_RF		(1<<16)			/**< Resume Flag. */
#define X86_FLAGS_VM		(1<<17)			/**< Virtual-8086 Mode. */
#define X86_FLAGS_AC		(1<<18)			/**< Alignment Check. */
#define X86_FLAGS_VIF		(1<<19)			/**< Virtual Interrupt Flag. */
#define X86_FLAGS_VIP		(1<<20)			/**< Virtual Interrupt Pending Flag. */
#define X86_FLAGS_ID		(1<<21)			/**< ID Flag. */

/*
 * Control registers.
 */

/** Flags in the CR0 Control Register. */
#define X86_CR0_PE		(1<<0)			/**< Protected Mode Enable. */
#define X86_CR0_MP		(1<<1)			/**< Monitor Coprocessor. */
#define X86_CR0_EM		(1<<2)			/**< Emulation. */
#define X86_CR0_TS		(1<<3)			/**< Task Switched. */
#define X86_CR0_ET		(1<<4)			/**< Extension Type. */
#define X86_CR0_NE		(1<<5)			/**< Numeric Error. */
#define X86_CR0_WP		(1<<16)			/**< Write Protect. */
#define X86_CR0_AM		(1<<18)			/**< Alignment Mask. */
#define X86_CR0_NW		(1<<29)			/**< Not Write-through. */
#define X86_CR0_CD		(1<<30)			/**< Cache Disable. */
#define X86_CR0_PG		(1<<31)			/**< Paging Enable. */

/** Flags in the CR4 Control Register. */
#define X86_CR4_VME		(1<<0)			/**< Virtual-8086 Mode Extensions. */
#define X86_CR4_PVI		(1<<1)			/**< Protected Mode Virtual Interrupts. */
#define X86_CR4_TSD		(1<<2)			/**< Time Stamp Disable. */
#define X86_CR4_DE		(1<<3)			/**< Debugging Extensions. */
#define X86_CR4_PSE		(1<<4)			/**< Page Size Extensions. */
#define X86_CR4_PAE		(1<<5)			/**< Physical Address Extension. */
#define X86_CR4_MCE		(1<<6)			/**< Machine Check Enable. */
#define X86_CR4_PGE		(1<<7)			/**< Page Global Enable. */
#define X86_CR4_PCE		(1<<8)			/**< Performance-Monitoring Counter Enable. */
#define X86_CR4_OSFXSR		(1<<9)			/**< OS Support for FXSAVE/FXRSTOR. */
#define X86_CR4_OSXMMEXCPT	(1<<10)			/**< OS Support for Unmasked SIMD FPU Exceptions. */
#define X86_CR4_VMXE		(1<<13)			/**< VMX-Enable Bit. */
#define X86_CR4_SMXE		(1<<14)			/**< SMX-Enable Bit. */

/*
 * Debug registers.
 */

/* Flags in the debug status register. */
#define X86_DR6_B0		(1<<0)			/**< Breakpoint 0 condition detected. */
#define X86_DR6_B1		(1<<1)			/**< Breakpoint 1 condition detected. */
#define X86_DR6_B2		(1<<2)			/**< Breakpoint 2 condition detected. */
#define X86_DR6_B3		(1<<3)			/**< Breakpoint 3 condition detected. */
#define X86_DR6_BD		(1<<13)			/**< Debug register access. */
#define X86_DR6_BS		(1<<14)			/**< Single-stepped. */
#define X86_DR6_BT		(1<<15)			/**< Task switch. */

/* Flags in the debug control register. */
#define X86_DR7_G0		(1<<1)			/**< Global breakpoint 0 enable. */
#define X86_DR7_G1		(1<<3)			/**< Global breakpoint 1 enable. */
#define X86_DR7_G2		(1<<5)			/**< Global breakpoint 2 enable. */
#define X86_DR7_G3		(1<<7)			/**< Global breakpoint 3 enable. */

/*
 * Model specific registers.
 */

/** Model Specific Registers. */
#define X86_MSR_IA32_APIC_BASE	0x1b			/**< LAPIC base address. */
#define X86_MSR_IA32_EFER	0xc0000080		/**< Extended Feature Enable register. */
#define X86_MSR_IA32_STAR	0xc0000081		/**< System Call Target Address. */
#define X86_MSR_IA32_LSTAR	0xc0000082		/**< 64-bit System Call Target Address. */
#define X86_MSR_IA32_FMASK	0xc0000084		/**< System Call Flag Mask. */
#define X86_MSR_IA32_GS_BASE	0xc0000101		/**< GS segment base register. */

/** EFER MSR flags. */
#define X86_EFER_SCE		(1<<0)			/**< System Call Enable. */
#define X86_EFER_LME		(1<<8)			/**< Long Mode (IA-32e) Enable. */
#define X86_EFER_LMA		(1<<10)			/**< Long Mode (IA-32e) Active. */
#define X86_EFER_NXE		(1<<11)			/**< Execute Disable (XD/NX) Bit Enable. */

/*
 * CPUID definitions.
 */

/** Standard CPUID function definitions. */
#define CPUID_VENDOR_ID		0x00000000		/**< Vendor ID/Highest Standard Function. */
#define CPUID_FEATURE_INFO	0x00000001		/**< Feature Information. */
#define CPUID_CACHE_DESC	0x00000002		/**< Cache Descriptors. */
#define CPUID_SERIAL_NUM	0x00000003		/**< Processor Serial Number. */
#define CPUID_CACHE_PARMS	0x00000004		/**< Deterministic Cache Parameters. */
#define CPUID_MONITOR_MWAIT	0x00000005		/**< MONITOR/MWAIT Parameters. */
#define CPUID_DTS_POWER		0x00000006		/**< Digital Thermal Sensor and Power Management Parameters. */
#define CPUID_DCA		0x00000009		/**< Direct Cache Access (DCA) Parameters. */
#define CPUID_PERFMON		0x0000000A		/**< Architectural Performance Monitor Features. */
#define CPUID_X2APIC		0x0000000B		/**< x2APIC Features/Processor Topology. */
#define CPUID_XSAVE		0x0000000D		/**< XSAVE Features. */

/** Extended CPUID function definitions. */
#define CPUID_EXT_MAX		0x80000000		/**< Largest Extended Function. */
#define CPUID_EXT_FEATURE	0x80000001		/**< Extended Feature Bits. */
#define CPUID_BRAND_STRING1	0x80000002		/**< Processor Name/Brand String (Part 1). */
#define CPUID_BRAND_STRING2	0x80000003		/**< Processor Name/Brand String (Part 2). */
#define CPUID_BRAND_STRING3	0x80000004		/**< Processor Name/Brand String (Part 3). */
#define CPUID_L2_CACHE		0x80000006		/**< Extended L2 Cache Features. */
#define CPUID_ADVANCED_PM	0x80000007		/**< Advanced Power Management. */
#define CPUID_ADDRESS_SIZE	0x80000008		/**< Virtual/Physical Address Sizes. */

#endif /* __ARCH_DEFS_H */
