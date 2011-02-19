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
 * @brief		Kernel arguments structure.
 */

#ifndef __KARGS_H
#define __KARGS_H

#include <arch/kargs.h>
#include <types.h>

/** Size of arrays in the arguments structure. */
#define KERNEL_ARGS_UUID_LEN	64

/** Structure describing a physical memory range. */
typedef struct kernel_args_memory {
	phys_ptr_t next;		/**< Pointer to next range structure (0 if last). */

	/** Type of the memory range. */
	enum {
		/** Free, usable memory. */
		PHYS_MEMORY_FREE,

		/** Allocated memory. */
		PHYS_MEMORY_ALLOCATED,

		/** Reclaimable memory. */
		PHYS_MEMORY_RECLAIMABLE,

		/** Reserved memory, never usable. */
		PHYS_MEMORY_RESERVED,

		/** Memory used by the bootloader (never reaches the kernel). */
		PHYS_MEMORY_INTERNAL,
	} type;

	phys_ptr_t start;		/**< Start of the memory range. */
	phys_ptr_t end;			/**< End of the memory range. */
} __packed kernel_args_memory_t;

/** Structure containing details of a CPU passed to the kernel. */
typedef struct kernel_args_cpu {
	phys_ptr_t next;		/**< Pointer to next CPU (0 if last). */
	uint32_t id;			/**< ID of the CPU. */
	kernel_args_cpu_arch_t arch;	/**< Architecture data. */
} __packed kernel_args_cpu_t;

/** Structure describing a boot module. */
typedef struct kernel_args_module {
	phys_ptr_t next;		/**< Pointer to next module structure (0 if last). */
	phys_ptr_t base;		/**< Address of the module. */
	uint32_t size;			/**< Size of the module. */
} __packed kernel_args_module_t;

/** Structure containing arguments passed to the kernel. */
typedef struct kernel_args {
	/** Physical memory information. */
	phys_ptr_t phys_ranges;		/**< Linked list of physical range structures. */
	uint32_t phys_range_count;	/**< Number of physical memory ranges. */
	phys_ptr_t kernel_phys;		/**< Physical base address of the kernel. */

	/** CPU information. */
	phys_ptr_t cpus;		/**< Linked list of CPU structures (boot is first). */
	uint32_t boot_cpu;		/**< ID of the boot CPU. */
	uint32_t cpu_count;		/**< Number of CPUs. */
	uint32_t highest_cpu_id;	/**< The highest CPU ID. */

	/** Video mode information. */
	uint16_t fb_width;		/**< Width of the display. */
	uint16_t fb_height;		/**< Height of the display. */
	uint8_t fb_depth;		/**< Bits per pixel. */
	phys_ptr_t fb_addr;		/**< Physical address of the framebuffer. */

	/** Module information. */
	phys_ptr_t modules;		/**< Linked list of module structures. */
	uint32_t module_count;		/**< Number of modules. */

	/** Boot filesystem UUID. */
	char boot_fs_uuid[KERNEL_ARGS_UUID_LEN];

	/** Kernel options. */
	bool smp_disabled;		/**< Whether SMP is disabled. */
	bool splash_disabled;		/**< Whether the boot splash is disabled. */
	bool force_fsimage;		/**< Whether to force FS image usage. */

	/** Architecture-specific arguments. */
	kernel_args_arch_t arch;
} __packed kernel_args_t;

#ifdef LOADER
extern kernel_args_t *kernel_args;

extern kernel_args_cpu_t *kargs_cpu_add(uint32_t id);
extern kernel_args_module_t *kargs_module_add(phys_ptr_t base, uint32_t size);
extern void kargs_init(void);
#endif

#endif /* __KARGS_H */
