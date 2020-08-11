/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Non-local jump functions.
 */

#ifndef __SETJMP_H
#define __SETJMP_H

#include <arch/setjmp.h>
#include <types.h>

/** Initialize a jump buffer.
 * @param buf           Buffer to initialize.
 * @param func          Function that should be called.
 * @param stack         Base of stack to use.
 * @param size          Size of stack. */
extern void initjmp(jmp_buf buf, void (*func)(void), void *stack, size_t size);

/** Save the current execution state.
 * @param buf           Buffer to save to.
 * @return              Non-zero if returning through longjmp(), 0 otherwise. */
extern int setjmp(jmp_buf buf);

/** Restore a saved execution state.
 * @param buf           Buffer to restore.
 * @param val           Value to return from setjmp(). */
extern void longjmp(jmp_buf buf, int val) __noreturn;

#endif /* __SETJMP_H */
