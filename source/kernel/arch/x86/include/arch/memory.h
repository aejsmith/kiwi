/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		x86 memory layout definitions.
 *
 * This file contains definitions for the virtual memory layout. On AMD64, the
 * virtual memory layout is as follows:
 *  0x0000000000000000-0x00007FFFFFFFFFFF - 128TB - Userspace memory.
 *  0xFFFFFF8000000000-0xFFFFFFBFFFFFFFFF - 256GB - Mapped to physical memory.
 *  0xFFFFFFC000000000-0xFFFFFFDFFFFFFFFF - 128GB - Kernel heap.
 *  0xFFFFFFE000000000-0xFFFFFFFF7FFFFFFF - 126GB - Kernel VM region.
 *  0xFFFFFFFF80000000-0xFFFFFFFFFFFFFFFF - 2GB   - Kernel image/modules.
 *
 * On IA32, it is as follows:
 *  0x00000000-0x7FFFFFFF - 2GB    - Userspace memory.
 *  0x80000000-0xBFFFFFFF - 1GB    - Mapped to the first GB of physical memory.
 *  0xC0000000-0xEFFFFFFF - 768MB  - Kernel heap.
 *  0xF0000000-0xFFBFFFFF - 254MB  - Kernel VM region.
 *  0xFFC00000-0xFFDFFFFF - 2MB    - Kernel image.
 *  0xFFE00000-0xFFFFFFFF - 2MB    - Fractal mapping of kernel page directory.
 */

#ifndef __ARCH_MEMORY_H
#define __ARCH_MEMORY_H

/** Memory layout definitions. */
#if __x86_64__
# define USER_MEMORY_BASE	0x0000000000000000	/**< User memory base. */
# define USER_MEMORY_SIZE	0x0000800000000000	/**< User memory size (128TB). */
# define LIBKERNEL_BASE		0x00007FFFF0000000	/**< Location of kernel library. */
# define LIBKERNEL_SIZE		0x0000000010000000	/**< Maximum size of kernel library. */
# define KERNEL_PMAP_BASE	0xFFFFFF8000000000	/**< Physical map area base. */
# define KERNEL_PMAP_SIZE	0x0000004000000000	/**< Physical map area size (256GB). */
# define KERNEL_PMAP_OFFSET	0x0000000000000000	/**< Physical map area offset. */
# define KERNEL_HEAP_BASE	0xFFFFFFC000000000	/**< Kernel heap base. */
# define KERNEL_HEAP_SIZE	0x0000002000000000	/**< Kernel heap size (128GB). */
# define KERNEL_VM_BASE		0xFFFFFFE000000000	/**< Kernel VM region base. */
# define KERNEL_VM_SIZE		0x0000001F80000000	/**< Kernel VM region size (126GB). */
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
# define KERNEL_PMAP_OFFSET	0x00000000		/**< Physical map area offset. */
# define KERNEL_HEAP_BASE	0xC0000000		/**< Kernel heap base. */
# define KERNEL_HEAP_SIZE	0x30000000		/**< Kernel heap size (768MB). */
# define KERNEL_VM_BASE		0xF0000000		/**< Kernel VM region base. */
# define KERNEL_VM_SIZE		0x0FC00000		/**< Kernel VM region size (254MB). */
# define KERNEL_VIRT_BASE	0xFFC00000		/**< Kernel virtual base address. */
# define KERNEL_PTBL_BASE	0xFFE00000		/**< Kernel page tables base. */
#endif

/** Stack size definitions. */
#define KSTACK_SIZE		0x2000			/**< Kernel stack size (8KB). */
#define USTACK_SIZE		0x200000		/**< Userspace stack size (2MB). */

#endif /* __ARCH_MEMORY_H */
