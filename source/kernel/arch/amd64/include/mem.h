/* Kiwi AMD64 memory layout definitions
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
 * @brief		AMD64 memory layout definitions.
 */

#ifndef __ARCH_MEM_H
#define __ARCH_MEM_H

#include <arch/page.h>

/** Stack size definitions. */
#define KSTACK_SIZE	PAGE_SIZE			/**< Kernel stack size. */
#define USTACK_SIZE	0x400000			/**< Userspace stack size. */
#define STACK_DELTA	16				/**< Stack delta. */

/* Memory layout looks like this:
 *  0x0000000000000000-0x00007FFFFFFFFFFF - 128TB - Userspace memory.
 *  0x0000800000000000-0xFFFFFF7FFFFFFFFF - Unused/unusable (not canonical).
 *  0xFFFFFF8000000000-0xFFFFFFBFFFFFFFFF - 256GB - Mapped to physical memory.
 *  0xFFFFFFC000000000-0xFFFFFFFF7FFFFFFF - 254GB - Kernel heap.
 *  0xFFFFFFFF80000000-0xFFFFFFFFFFFFFFFF - 2GB   - Kernel image/modules.
 */

/** Memory layout definitions. */
#define USPACE_BASE		0x0000000000000000	/**< User memory base. */
#define USPACE_SIZE		0x0000800000000000	/**< User memory size (128TB). */
#define KERNEL_PMAP_BASE	0xFFFFFF8000000000	/**< Physical map area base. */
#define KERNEL_PMAP_SIZE	0x0000004000000000	/**< Physical map area size (256GB). */
#define KERNEL_HEAP_BASE	0xFFFFFFC000000000	/**< Kernel heap base. */
#define KERNEL_HEAP_SIZE	0x0000003F80000000	/**< Kernel heap size (254GB). */
#define KERNEL_VIRT_BASE	0xFFFFFFFF80000000	/**< Kernel virtual base address. */
#define KERNEL_PHYS_BASE	0x0000000000200000	/**< Kernel physical base address. */

/** Convert a kernel address to the equivalent physical address. */
#ifndef __ASM__
# define KA2PA(a)		(((phys_ptr_t)(ptr_t)(a) - KERNEL_VIRT_BASE) + KERNEL_PHYS_BASE)
#else
# define KA2PA(a)		(((a) - KERNEL_VIRT_BASE) + KERNEL_PHYS_BASE)
#endif

#endif /* __ARCH_MEM_H */
