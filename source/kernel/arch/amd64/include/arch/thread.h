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
 * @brief		AMD64-specific thread definitions.
 */

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#ifndef __ASM__

#include <types.h>

struct cpu;
struct intr_frame;

/** x86-specific thread structure.
 * @note		The GS register is pointed to the copy of this structure
 *			for the current thread. It is used to access per-CPU
 *			data, and also to easily access per-thread data from
 *			assembly code. If changing the layout of this structure,
 *			be sure to updated the offset definitions below. */
typedef struct thread_arch {
	struct cpu *cpu;			/** Current CPU pointer. */

	/** SYSCALL/SYSRET data. */
	ptr_t kernel_rsp;			/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;				/**< Temporary storage for user RSP. */

	struct intr_frame *user_iframe;		/**< Frame from last user-mode entry. */
	unative_t flags;			/**< Flags for the thread. */
	ptr_t tls_base;				/**< TLS base address. */
} __packed thread_arch_t;

#endif /* __ASM__ */

/** Flags for thread_arch_t. */
#define THREAD_ARCH_IFRAME_MODIFIED	(1<<0)	/**< Interrupt frame was modified. */

/** Offsets in thread_arch_t. */
#define THREAD_ARCH_OFF_KERNEL_RSP	0x8
#define THREAD_ARCH_OFF_USER_RSP	0x10
#define THREAD_ARCH_OFF_USER_IFRAME	0x18
#define THREAD_ARCH_OFF_FLAGS		0x20

#endif /* __ARCH_THREAD_H */
