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
 * @brief		x86-specific thread functions.
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
#ifdef __x86_64__
	/** SYSCALL/SYSRET data. */
	ptr_t kernel_rsp;			/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;				/**< Temporary storage for user RSP. */
#endif
	struct intr_frame *user_iframe;		/**< Frame from last user-mode entry. */
	unative_t flags;			/**< Flags for the thread. */
	ptr_t tls_base;				/**< TLS base address. */
} __packed thread_arch_t;

#endif /* __ASM__ */

/** Flags for thread_arch_t. */
#ifdef __x86_64__
# define THREAD_ARCH_IFRAME_MODIFIED	(1<<0)	/**< Interrupt frame was modified. */
#endif

/** Offsets in thread_arch_t. */
#ifdef __x86_64__
# define THREAD_ARCH_OFF_KERNEL_RSP	0x8
# define THREAD_ARCH_OFF_USER_RSP	0x10
# define THREAD_ARCH_OFF_USER_IFRAME	0x18
# define THREAD_ARCH_OFF_FLAGS		0x20
#else
# define THREAD_ARCH_OFF_USER_IFRAME	0x4
# define THREAD_ARCH_OFF_FLAGS		0x8
#endif

#endif /* __ARCH_THREAD_H */
