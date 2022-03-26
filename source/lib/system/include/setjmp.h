/*
 * Copyright (C) 2009-2022 Alex Smith
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

#pragma once

#include <system/arch/setjmp.h>
#include <system/defs.h>

#include <signal.h>

__SYS_EXTERN_C_BEGIN

/** Type containing setjmp() saved state. */
typedef unsigned long jmp_buf[JMP_BUF_SIZE];

/** Buffer for sigsetjmp()/siglongjmp(). */
typedef struct _sigjmp_buf {
    jmp_buf buf;
    sigset_t mask;
    bool restore_mask;
} sigjmp_buf[1];

extern void longjmp(jmp_buf env, int val) __sys_noreturn;
extern int setjmp(jmp_buf env);
extern void siglongjmp(sigjmp_buf env, int val) __sys_noreturn;
extern int sigsetjmp(sigjmp_buf env, int save_mask);

__SYS_EXTERN_C_END
