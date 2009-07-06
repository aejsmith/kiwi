/* Kiwi AMD64 system call wrappers
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
 * @brief		AMD64 system call wrappers.
 */

/** Macro to define a system call. */
.macro __syscall num, name
.global \name
.type \name, @function
\name:
        movq    %rcx, %r10
        movq    $\num, %rax
       	syscall
        ret
.size \name, .-\name
.endm

/** Wrappers for above macro expected by generic code. */
#define SYSCALL0(num, name)	__syscall num, name
#define SYSCALL1(num, name)	__syscall num, name
#define SYSCALL2(num, name)	__syscall num, name
#define SYSCALL3(num, name)	__syscall num, name
#define SYSCALL4(num, name)	__syscall num, name
#define SYSCALL5(num, name)	__syscall num, name
#define SYSCALL6(num, name)	__syscall num, name
