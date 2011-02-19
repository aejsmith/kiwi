/*
 * Copyright (C) 2010 Alex Smith
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
	int max_phys_bits;		/**< Maximum physical address bits. */
	int max_virt_bits;		/**< Maximum virtual address bits. */
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
