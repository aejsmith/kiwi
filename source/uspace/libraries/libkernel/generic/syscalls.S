/* Kiwi system call wrappers
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
 * @brief		System call wrappers.
 */

/** Get the architecture wrappers. */
#if CONFIG_ARCH_AMD64
# include "../arch/amd64/syscalls.S.inc"
#else
# error "Unknown system call architecture!"
#endif

SYSCALL1(message, 0)
