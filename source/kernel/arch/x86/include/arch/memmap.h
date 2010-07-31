/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		x86 memory layout definitions.
 *
 * This file contains definitions for the virtual memory layout. On AMD64, the
 * virtual memory layout is as follows:
 *  0x0000000000000000-0x00007FFFFFFFFFFF - 128TB - Userspace memory.
 *  0xFFFFFF8000000000-0xFFFFFFBFFFFFFFFF - 256GB - Mapped to physical memory.
 *  0xFFFFFFC000000000-0xFFFFFFFF7FFFFFFF - 254GB - Kernel heap.
 *  0xFFFFFFFF80000000-0xFFFFFFFFFFFFFFFF - 2GB   - Kernel image/modules.
 *
 * On IA32, it is as follows:
 *  0x00000000-0x7FFFFFFF - 2GB    - Userspace memory.
 *  0x80000000-0xBFFFFFFF - 1GB    - Mapped to the first GB of physical memory.
 *  0xC0000000-0xFFBFFFFF - 1020MB - Kernel heap.
 *  0xFFC00000-0xFFDFFFFF - 2MB    - Kernel image.
 *  0xFFE00000-0xFFFFFFFF - 2MB    - Fractal mapping of kernel page directory.
 */

#ifndef __ARCH_MEMMAP_H
#define __ARCH_MEMMAP_H

/** Memory layout definitions. */
#if __x86_64__
# define USER_MEMORY_BASE	0x0000000000000000	/**< User memory base. */
# define USER_MEMORY_SIZE	0x0000800000000000	/**< User memory size (128TB). */
# define LIBKERNEL_BASE		0x00007FFFF0000000	/**< Location of kernel library. */
# define LIBKERNEL_SIZE		0x0000000010000000	/**< Maximum size of kernel library. */
# define KERNEL_PMAP_BASE	0xFFFFFF8000000000	/**< Physical map area base. */
# define KERNEL_PMAP_SIZE	0x0000004000000000	/**< Physical map area size (256GB). */
# define KERNEL_HEAP_BASE	0xFFFFFFC000000000	/**< Kernel heap base. */
# define KERNEL_HEAP_SIZE	0x0000003F80000000	/**< Kernel heap size (254GB). */
# define KERNEL_VIRT_BASE	0xFFFFFFFF80000000	/**< Kernel virtual base address. */
# define KERNEL_MODULE_BASE	0xFFFFFFFFC0000000	/**< Module area base. */
# define KERNEL_MODULE_SIZE	0x0000000040000000	/**< Module area size (1GB). */
#else
# define USER_MEMORY_BASE	0x00000000		/**< User memory base. */
# define USER_MEMORY_SIZE	0x80000000		/**< User memory size (2GB). */
# define LIBKERNEL_BASE		0x7FFF0000		/**< Location of kernel library. */
# define LIBKERNEL_SIZE		0x00010000		/**< Maximum size of kernel library. */
# define KERNEL_PMAP_BASE	0x80000000		/**< Physical map area base. */
# define KERNEL_PMAP_SIZE	0x40000000		/**< Physical map area size (1GB). */
# define KERNEL_HEAP_BASE	0xC0000000		/**< Kernel heap base. */
# define KERNEL_HEAP_SIZE	0x3FC00000		/**< Kernel heap size (1020MB). */
# define KERNEL_VIRT_BASE	0xFFC00000		/**< Kernel virtual base address. */
# define KERNEL_PTBL_BASE	0xFFE00000		/**< Kernel page tables base. */
#endif

#endif /* __ARCH_MEMMAP_H */
