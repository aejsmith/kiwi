/* Kiwi x86 CPU feature check macros
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
 * @brief		x86 CPU feature check macros.
 */

#ifndef __ARCH_FEATURES_H
#define __ARCH_FEATURES_H

#include <cpu/cpu.h>

/** Check for a standard feature (ECX). */
#define CPU_FEAT_STD_ECX(cpu, bit)	\
	((cpu)->arch.features.feat_ecx & (1<<(bit)))

/** Check for a standard feature (EDX). */
#define CPU_FEAT_STD_EDX(cpu, bit)	\
	((cpu)->arch.features.feat_edx & (1<<(bit)))

/** Check for an extended feature (ECX). */
#define CPU_FEAT_EXT_ECX(cpu, bit)	\
	((cpu)->arch.features.ext_ecx & (1<<(bit)))

/** Check for an extended feature (EDX). */
#define CPU_FEAT_EXT_EDX(cpu, bit)	\
	((cpu)->arch.features.ext_edx & (1<<(bit)))

/** Feature check macros - Standard CPUID Features (EDX). */
#define CPU_HAS_FPU(c)		CPU_FEAT_STD_EDX(c, 0)	/**< FPU. */
#define CPU_HAS_VME(c)		CPU_FEAT_STD_EDX(c, 1)	/**< VME. */
#define CPU_HAS_DE(c)		CPU_FEAT_STD_EDX(c, 2)	/**< DE. */
#define CPU_HAS_PSE(c)		CPU_FEAT_STD_EDX(c, 3)	/**< PSE. */
#define CPU_HAS_TSC(c)		CPU_FEAT_STD_EDX(c, 4)	/**< TSC. */
#define CPU_HAS_MSR(c)		CPU_FEAT_STD_EDX(c, 5)	/**< MSR. */
#define CPU_HAS_PAE(c)		CPU_FEAT_STD_EDX(c, 6)	/**< PAE. */
#define CPU_HAS_MCE(c)		CPU_FEAT_STD_EDX(c, 7)	/**< MCE. */
#define CPU_HAS_CX8(c)		CPU_FEAT_STD_EDX(c, 8)	/**< CX8. */
#define CPU_HAS_APIC(c)		CPU_FEAT_STD_EDX(c, 9)	/**< APIC. */
#define CPU_HAS_SEP(c)		CPU_FEAT_STD_EDX(c, 11)	/**< SEP. */
#define CPU_HAS_MTRR(c)		CPU_FEAT_STD_EDX(c, 12)	/**< MTRR. */
#define CPU_HAS_PGE(c)		CPU_FEAT_STD_EDX(c, 13)	/**< PGE. */
#define CPU_HAS_MCA(c)		CPU_FEAT_STD_EDX(c, 14)	/**< MCA. */
#define CPU_HAS_CMOV(c)		CPU_FEAT_STD_EDX(c, 15)	/**< CMOV. */
#define CPU_HAS_PAT(c)		CPU_FEAT_STD_EDX(c, 16)	/**< PAT. */
#define CPU_HAS_PSE36(c)	CPU_FEAT_STD_EDX(c, 17)	/**< PSE36. */
#define CPU_HAS_PSN(c)		CPU_FEAT_STD_EDX(c, 18)	/**< PSN. */
#define CPU_HAS_CLFSH(c)	CPU_FEAT_STD_EDX(c, 19)	/**< CLFSH. */
#define CPU_HAS_DS(c)		CPU_FEAT_STD_EDX(c, 21)	/**< DS. */
#define CPU_HAS_ACPI(c)		CPU_FEAT_STD_EDX(c, 22)	/**< ACPI. */
#define CPU_HAS_MMX(c)		CPU_FEAT_STD_EDX(c, 23)	/**< MMX. */
#define CPU_HAS_FXSR(c)		CPU_FEAT_STD_EDX(c, 24)	/**< FXSR. */
#define CPU_HAS_SSE(c)		CPU_FEAT_STD_EDX(c, 25)	/**< SSE. */
#define CPU_HAS_SSE2(c)		CPU_FEAT_STD_EDX(c, 26)	/**< SSE2. */
#define CPU_HAS_SS(c)		CPU_FEAT_STD_EDX(c, 27)	/**< SS. */
#define CPU_HAS_HTT(c)		CPU_FEAT_STD_EDX(c, 28)	/**< HTT. */
#define CPU_HAS_TM(c)		CPU_FEAT_STD_EDX(c, 29)	/**< TM. */
#define CPU_HAS_PBE(c)		CPU_FEAT_STD_EDX(c, 31)	/**< PBE. */

/** Feature check macros - Standard CPUID Features (ECX). */
#define CPU_HAS_SSE3(c)		CPU_FEAT_STD_ECX(c, 0)	/**< SSE3. */
#define CPU_HAS_MONITOR(c)	CPU_FEAT_STD_ECX(c, 3)	/**< MONITOR. */
#define CPU_HAS_DSCPL(c)	CPU_FEAT_STD_ECX(c, 4)	/**< DSCPL. */
#define CPU_HAS_VMX(c)		CPU_FEAT_STD_ECX(c, 5)	/**< VMX. */
#define CPU_HAS_SMX(c)		CPU_FEAT_STD_ECX(c, 6)	/**< SMX. */
#define CPU_HAS_EST(c)		CPU_FEAT_STD_ECX(c, 7)	/**< EST. */
#define CPU_HAS_TM2(c)		CPU_FEAT_STD_ECX(c, 8)	/**< TM2. */
#define CPU_HAS_SSSE3(c)	CPU_FEAT_STD_ECX(c, 9)	/**< SSSE3. */
#define CPU_HAS_CNXTID(c)	CPU_FEAT_STD_ECX(c, 10)	/**< CNXTID. */
#define CPU_HAS_CMPXCHG16B(c)	CPU_FEAT_STD_ECX(c, 13)	/**< CMPXCHG16B. */
#define CPU_HAS_PDCM(c)		CPU_FEAT_STD_ECX(c, 15)	/**< PDCM. */
#define CPU_HAS_DCA(c)		CPU_FEAT_STD_ECX(c, 18)	/**< DCA. */
#define CPU_HAS_SSE4_1(c)	CPU_FEAT_STD_ECX(c, 19)	/**< SSE4_1. */
#define CPU_HAS_SSE4_2(c)	CPU_FEAT_STD_ECX(c, 20)	/**< SSE4_2. */
#define CPU_HAS_POPCNT(c)	CPU_FEAT_STD_ECX(c, 23)	/**< POPCNT. */

/** Feature check macros - Extended CPUID Features (EDX). */
#define CPU_HAS_SYSCALL(c)	CPU_FEAT_EXT_EDX(c, 11)	/**< SYSCALL/SYSRET. */
#define CPU_HAS_XD(c)		CPU_FEAT_EXT_EDX(c, 20)	/**< NX/XD Bit. */
#define CPU_HAS_LMODE(c)	CPU_FEAT_EXT_EDX(c, 29)	/**< Long Mode. */

/** Feature check macros - Extended CPUID Features (ECX). */
#define CPU_HAS_LAHF(c)		CPU_FEAT_EXT_ECX(c, 0)	/**< LAHF/SAHF. */

#endif /* __ARCH_FEATURES_H */
