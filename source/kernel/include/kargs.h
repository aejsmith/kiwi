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
 * @brief		Kernel arguments structure.
 */

#ifndef __BOOT_KARGS_H
#define __BOOT_KARGS_H

#include <arch/kargs.h>
#include <types.h>

/** Size of arrays in the arguments structure. */
#define KERNEL_ARGS_RANGES_MAX	64
#define KERNEL_ARGS_UUID_LEN	64

/** Structure describing a physical memory range. */
typedef struct kernel_args_memory {
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

		/** Memory used by the bootloader. */
		PHYS_MEMORY_INTERNAL,
	} type;

	phys_ptr_t start;		/**< Start of the memory range. */
	phys_ptr_t end;			/**< End of the memory range. */
} __packed kernel_args_memory_t;

/** Structure containing details of a CPU passed to the kernel. */
typedef struct kernel_args_cpu {
	phys_ptr_t next;		/**< Pointer to next CPU (or NULL if last). */
	uint32_t id;			/**< ID of the CPU. */
	kernel_args_cpu_arch_t arch;	/**< Architecture data. */
} __packed kernel_args_cpu_t;

/** Structure containing arguments passed to the kernel. */
typedef struct kernel_args {
	/** Physical memory information. */
	kernel_args_memory_t phys_ranges[KERNEL_ARGS_RANGES_MAX];
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

	/** Boot filesystem UUID. */
	char boot_fs_uuid[KERNEL_ARGS_UUID_LEN];

	/** Kernel options. */
	bool smp_disabled;		/**< Whether SMP is disabled. */
	bool splash_disabled;		/**< Whether the boot splash is disabled. */

	/** Architecture-specific arguments. */
	kernel_args_arch_t arch;
} __packed kernel_args_t;

#ifdef LOADER
extern kernel_args_t *g_kernel_args;

extern kernel_args_cpu_t *kargs_cpu_add(uint32_t id);
extern void kargs_init(void);
#endif

#endif /* __BOOT_KARGS_H */
