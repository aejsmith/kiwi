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

#include <types.h>

#ifndef LOADER

#include <arch/x86/descriptor.h>

struct cpu;

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Structure containing CPU feature information. */
typedef struct cpu_features {
	/** Standard CPUID Features (EDX). */
	bool fpu, vme, de, pse, tsc, msr, pae, mce, cx8, apic, sep, mtrr, pge;
	bool mca, cmov, pat, pse36, psn, clfsh, ds, acpi, mmx, fxsr, sse, sse2;
	bool ss, htt, tm, pbe;

	/** Standard CPUID Features (ECX). */
	bool sse3, pclmulqdq, dtes64, monitor, dscpl, vmx, smx, est, tm2, ssse3;
	bool cnxtid, fma, cmpxchg16b, xtpr, pdcm, pcid, dca, sse4_1, sse4_2;
	bool x2apic, movbe, popcnt, tscd, aes, xsave, osxsave, avx;

	/** Extended CPUID Features (EDX). */
	bool syscall, xd, lmode;

	/** Extended CPUID Features (ECX). */
	bool lahf;
} cpu_features_t;

/** Architecture-specific CPU structure. */
typedef struct cpu_arch {
	/** Pointer back to containing CPU structure.
	 * @note		The GS segment is pointed at the top of this
	 *			structure, which is used to get the current CPU
	 *			structure pointer. */
	struct cpu *cpu_ptr;
#ifdef __x86_64__
	/** SYSCALL/SYSRET data.
	 * @note		It is OK to store these on a per-CPU basis
	 *			rather than in per-thread data, as it is only
	 *			used during system call entry, not return. The
	 *			user RSP field is only used as a temporary
	 *			scratch space before pushing it on to the
	 *			kernel stack. */
	ptr_t kernel_rsp;			/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;				/**< Temporary storage for user RSP. */
#endif
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

	/** CPU information. */
	uint64_t cpu_freq;			/**< CPU frequency in Hz. */
	uint64_t lapic_freq;			/**< LAPIC timer frequency in Hz. */
	char model_name[64];			/**< CPU model name. */
	uint8_t family;				/**< CPU family. */
	uint8_t model;				/**< CPU model. */
	uint8_t stepping;			/**< CPU stepping. */
	int max_phys_bits;			/**< Maximum physical address bits. */
	int max_virt_bits;			/**< Maximum virtual address bits. */
	int cache_alignment;			/**< Cache line size. */
	cpu_features_t features;		/**< Features supported by the CPU. */
} cpu_arch_t;

/** Get the current CPU structure pointer.
 * @return		Pointer to current CPU structure. */
static inline struct cpu *cpu_get_pointer(void) {
	ptr_t addr;
	__asm__ volatile("mov %%gs:0, %0" : "=r"(addr));
	return (struct cpu *)addr;
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

#endif /* __ARCH_CPU_H */
