/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Setjmp/longjmp functions.
 */

#ifndef __SETJMP_H
#define __SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

/** Buffer for setjmp/longjmp. TODO: Seperate arch include */
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

extern void longjmp(jmp_buf env, int val);
extern int setjmp(jmp_buf env);

#ifdef __cplusplus
}
#endif

#endif /* __SETJMP_H */
