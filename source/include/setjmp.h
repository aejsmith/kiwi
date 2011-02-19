/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Non-local jump functions.
 */

#ifndef __SETJMP_H
#define __SETJMP_H

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Buffer for setjmp()/longjmp(). TODO: Seperate arch include */
#ifdef __i386__
/* jmp_buf[0] = Return address
 * jmp_buf[1] = ebp
 * jmp_buf[2] = esp
 * jmp_buf[3] = ebx
 * jmp_buf[4] = edi
 * jmp_buf[5] = esi
 */
typedef unsigned long jmp_buf[6];
#elif defined(__x86_64__)
/* jmp_buf[0] = Return address
 * jmp_buf[1] = rbp
 * jmp_buf[2] = rsp
 * jmp_buf[3] = rbx
 * jmp_buf[4] = r12
 * jmp_buf[5] = r13
 * jmp_buf[6] = r14
 * jmp_buf[7] = r15
 */
typedef unsigned long jmp_buf[8];
#endif

/** Buffer for sigsetjmp()/siglongjmp(). */
typedef struct _sigjmp_buf {
        jmp_buf buf;
        int restore_mask;
       	sigset_t mask;
} sigjmp_buf[1];

extern void longjmp(jmp_buf env, int val) __attribute__((noreturn));
extern int setjmp(jmp_buf env);
extern void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));
extern int sigsetjmp(sigjmp_buf env, int savemask);

#ifdef __cplusplus
}
#endif

#endif /* __SETJMP_H */
