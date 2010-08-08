/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		x86-specific kernel arguments structures.
 */

#ifndef __ARCH_KARGS_H
#define __ARCH_KARGS_H

#include <types.h>

/** x86-specific kernel arguments. */
typedef struct kernel_args_arch {
	phys_ptr_t lapic_address;	/**< Address of LAPIC mapping. */
	bool lapic_disabled;		/**< Whether to disable LAPIC usage. */
	int cache_alignment;		/**< Highest cache line size of all CPUs. */

	/** Features present on all CPUs. */
	uint32_t standard_ecx;		/**< ECX value of function 01h. */
	uint32_t standard_edx;		/**< EDX value of function 01h. */
	uint32_t extended_ecx;		/**< ECX value of function 80000000h. */
	uint32_t extended_edx;		/**< ECX value of function 80000000h. */
} __packed kernel_args_arch_t;

/** x86-specific CPU information structure. */
typedef struct kernel_args_cpu_arch {
	uint64_t cpu_freq;		/**< CPU frequency in Hz. */
	uint64_t lapic_freq;		/**< LAPIC timer frequency in Hz. */
	char model_name[64];		/**< CPU model name. */
	uint8_t family;			/**< CPU family. */
	uint8_t model;			/**< CPU model. */
	uint8_t stepping;		/**< CPU stepping. */
	int cache_alignment;		/**< Cache line size. */

	/** Feature information. */
	uint32_t highest_standard;	/**< Highest standard function. */
	uint32_t standard_ecx;		/**< ECX value of function 01h. */
	uint32_t standard_edx;		/**< EDX value of function 01h. */
	uint32_t highest_extended;	/**< Highest extended function. */
	uint32_t extended_ecx;		/**< ECX value of function 80000000h. */
	uint32_t extended_edx;		/**< ECX value of function 80000000h. */
} __packed kernel_args_cpu_arch_t;

#endif /* __ARCH_KARGS_H */
