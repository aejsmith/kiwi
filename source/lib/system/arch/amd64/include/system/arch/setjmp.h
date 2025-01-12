/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 setjmp definitions.
 */

#pragma once

/**
 * Jump buffer size.
 *
 * On AMD64 we have the following layout:
 *  jmp_buf[0] = Return address
 *  jmp_buf[1] = rsp
 *  jmp_buf[2] = rbp
 *  jmp_buf[3] = rbx
 *  jmp_buf[4] = r12
 *  jmp_buf[5] = r13
 *  jmp_buf[6] = r14
 *  jmp_buf[7] = r15
 */
#define JMP_BUF_SIZE 8
