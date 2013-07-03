/*
 * Copyright (C) 2009-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 memory layout definitions.
 *
 * This file contains definitions for the virtual memory layout. The virtual
 * memory layout is as follows:
 *  0x0000000000000000-0x00007FFFFFFFFFFF - 128TB - User address space.
 *  0xFFFFFF8000000000-0xFFFFFFBFFFFFFFFF - 256GB - Physical map area.
 *  0xFFFFFFC000000000-0xFFFFFFFF7FFFFFFF - 254GB - Kernel allocation area.
 *  0xFFFFFFFF80000000-0xFFFFFFFFFFFFFFFF - 2GB   - Kernel image/modules.
 */

#ifndef __ARCH_MEMORY_H
#define __ARCH_MEMORY_H

/** Memory layout definitions. */
#define USER_BASE		0x0000000000000000	/**< User address space base. */
#define USER_SIZE		0x0000800000000000	/**< User address space size (128TB). */
#define LIBKERNEL_BASE		0x00007FFFF0000000	/**< Location of kernel library. */
#define LIBKERNEL_SIZE		0x0000000010000000	/**< Maximum size of kernel library. */
#define KERNEL_BASE		0xFFFFFF8000000000	/**< Kernel address space base. */
#define KERNEL_SIZE		0x0000008000000000	/**< Kernel address space size (512GB). */
#define KERNEL_PMAP_BASE	0xFFFFFF8000000000	/**< Physical map area base. */
#define KERNEL_PMAP_SIZE	0x0000004000000000	/**< Physical map area size (256GB). */
#define KERNEL_PMAP_OFFSET	0x0000000000000000	/**< Physical map area offset. */
#define KERNEL_KMEM_BASE	0xFFFFFFC000000000	/**< Kernel allocation area base. */
#define KERNEL_KMEM_SIZE	0x0000003F80000000	/**< Kernel allocation area size (254GB). */
#define KERNEL_VIRT_BASE	0xFFFFFFFF80000000	/**< Kernel virtual base address. */
#define KERNEL_MODULE_BASE	0xFFFFFFFFC0000000	/**< Module area base. */
#define KERNEL_MODULE_SIZE	0x0000000040000000	/**< Module area size (1GB). */

/** Stack size definitions. */
#define KSTACK_SIZE		0x2000			/**< Kernel stack size (8KB). */
#define USTACK_SIZE		0x200000		/**< User stack size (2MB). */

/** Stack direction definition. */
#define STACK_GROWS_DOWN	1

#ifndef __ASM__
extern char __text_seg_start[], __text_seg_end[];
extern char __data_seg_start[], __data_seg_end[];
extern char __init_seg_start[], __init_seg_end[];
#endif

#endif /* __ARCH_MEMORY_H */
