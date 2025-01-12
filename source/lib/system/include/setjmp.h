/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
