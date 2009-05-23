/* Kiwi IA32 memory layout definitions
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
 * @brief		IA32 memory layout definitions.
 */

#ifndef __ARCH_MEMMAP_H
#define __ARCH_MEMMAP_H

/* Memory layout looks like this:
 *  0x00000000-0xBFFFFFFF - 3GB    - Userspace memory.
 *  0xC0000000-0xFFBFFFFF - 1020MB - Kernel heap.
 *  0xFFC00000-0xFFDFFFFF - 2MB    - Kernel image.
 *  0xFFE00000-0xFFFFFFFF - 2MB    - Fractal mapping of kernel page directory.
 */

/** Memory layout definitions. */
#define ASPACE_BASE		0x00000000	/**< User memory base. */
#define ASPACE_SIZE		0xC0000000	/**< User memory size (3GB). */
#define KERNEL_HEAP_BASE	0xC0000000	/**< Kernel heap base. */
#define KERNEL_HEAP_SIZE	0x3FC00000	/**< Kernel heap size (1020MB). */
#define KERNEL_VIRT_BASE	0xFFC00000	/**< Kernel virtual base address. */
#define KERNEL_PTBL_BASE	0xFFE00000	/**< Kernel page tables base. */
#define KERNEL_PHYS_BASE	0x00200000	/**< Kernel physical base address. */

/** Convert a kernel address to the equivalent physical address. */
#ifndef __ASM__
# define KA2PA(a)		(((phys_ptr_t)(ptr_t)(a) - KERNEL_VIRT_BASE) + KERNEL_PHYS_BASE)
#else
# define KA2PA(a)		(((a) - KERNEL_VIRT_BASE) + KERNEL_PHYS_BASE)
#endif

#endif /* __ARCH_MEMMAP_H */
