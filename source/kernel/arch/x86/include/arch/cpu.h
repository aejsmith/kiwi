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

#include <arch/descriptor.h>
#include <arch/memmap.h>
#include <arch/stack.h>

#include <types.h>

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Architecture-specific CPU structure. */
typedef struct cpu_arch {
	/** Per-CPU CPU structures. */
	gdt_entry_t gdt[GDT_ENTRY_COUNT];	/**< Array of GDT descriptors. */
	tss_t tss;				/**< Task State Segment (TSS). */

	/** Time conversion factors. */
	uint64_t cycles_per_us;			/**< CPU cycles per µs. */
	uint64_t lapic_timer_cv;		/**< LAPIC timer conversion factor. */

	/** Basic CPU information. */
	uint64_t cpu_freq;			/**< CPU frequency in Hz. */
	uint64_t lapic_freq;		/**< LAPIC timer frequency in Hz. */
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

/** Halt the current CPU. */
static inline __noreturn void cpu_halt(void) {
	while(true) {
		__asm__ volatile("cli; hlt");
	}
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
