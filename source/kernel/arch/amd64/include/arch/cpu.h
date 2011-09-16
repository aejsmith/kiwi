/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		AMD64 CPU management.
 */

#ifndef __ARCH_CPU_H
#define __ARCH_CPU_H

#include <x86/descriptor.h>

#include <types.h>

struct cpu;

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Structure containing CPU feature information. */
typedef struct cpu_features {
	/** Standard CPUID Features (EDX). */
	union {
		struct {
			unsigned fpu : 1;
			unsigned vme : 1;
			unsigned de : 1;
			unsigned pse : 1;
			unsigned tsc : 1;
			unsigned msr : 1;
			unsigned pae : 1;
			unsigned mce : 1;
			unsigned cx8 : 1;
			unsigned apic : 1;
			unsigned : 1;
			unsigned sep : 1;
			unsigned mtrr : 1;
			unsigned pge : 1;
			unsigned mca : 1;
			unsigned cmov : 1;
			unsigned pat : 1;
			unsigned pse36 : 1;
			unsigned psn : 1;
			unsigned clfsh : 1;
			unsigned : 1;
			unsigned ds : 1;
			unsigned acpi : 1;
			unsigned mmx : 1;
			unsigned fxsr : 1;
			unsigned sse : 1;
			unsigned sse2 : 1;
			unsigned ss : 1;
			unsigned htt : 1;
			unsigned tm : 1;
			unsigned : 1;
			unsigned pbe : 1;
		};
		uint32_t standard_edx;
	};

	/** Standard CPUID Features (ECX). */
	union {
		struct {
			unsigned sse3 : 1;
			unsigned pclmulqdq : 1;
			unsigned dtes64 : 1;
			unsigned monitor : 1;
			unsigned dscpl : 1;
			unsigned vmx : 1;
			unsigned smx : 1;
			unsigned est : 1;
			unsigned tm2 : 1;
			unsigned ssse3 : 1;
			unsigned cnxtid : 1;
			unsigned : 2;
			unsigned fma : 1;
			unsigned cmpxchg16b : 1;
			unsigned xtpr : 1;
			unsigned pdcm : 1;
			unsigned : 2;
			unsigned pcid : 1;
			unsigned dca : 1;
			unsigned sse4_1 : 1;
			unsigned sse4_2 : 1;
			unsigned x2apic : 1;
			unsigned movbe : 1;
			unsigned popcnt : 1;
			unsigned tscd : 1;
			unsigned aes : 1;
			unsigned xsave : 1;
			unsigned osxsave : 1;
			unsigned avx : 1;
			unsigned : 3;
		};
		uint32_t standard_ecx;
	};

	/** Extended CPUID Features (EDX). */
	union {
		struct {
			unsigned : 11;
			unsigned syscall : 1;
			unsigned : 8;
			unsigned xd : 1;
			unsigned : 8;
			unsigned lmode : 1;
		};
		uint32_t extended_edx;
	};

	/** Extended CPUID Features (ECX). */
	union {
		struct {
			unsigned lahf : 1;
			unsigned : 31;
		};
		uint32_t extended_ecx;
	};
} cpu_features_t;

/** Architecture-specific CPU structure. */
typedef struct arch_cpu {
	struct cpu *parent;			/**< Pointer back to CPU. */

	/** Time conversion factors. */
	uint64_t cycles_per_us;			/**< CPU cycles per µs. */
	uint64_t lapic_timer_cv;		/**< LAPIC timer conversion factor. */

	/** Per-CPU CPU structures. */
	gdt_entry_t gdt[GDT_ENTRY_COUNT];	/**< Array of GDT descriptors. */
	tss_t tss;				/**< Task State Segment (TSS). */
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

	/** Feature information. */
	uint32_t highest_standard;		/**< Highest standard function. */
	uint32_t highest_extended;		/**< Highest extended function. */
	cpu_features_t features;		/**< Features supported by the CPU. */
} arch_cpu_t;

/** Get the current CPU structure pointer.
 * @return		Pointer to current CPU structure. */
static inline struct cpu *cpu_get_pointer(void) {
	ptr_t addr;
	__asm__("mov %%gs:0, %0" : "=r"(addr));
	return (struct cpu *)addr;
}

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
