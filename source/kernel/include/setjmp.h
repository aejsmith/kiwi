/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Non-local jump functions.
 */

#pragma once

#include <arch/setjmp.h>
#include <types.h>

/** Save the current execution state.
 * @param buf           Buffer to save to.
 * @return              Non-zero if returning through longjmp(), 0 otherwise. */
extern int setjmp(jmp_buf buf);

/** Restore a saved execution state.
 * @param buf           Buffer to restore.
 * @param val           Value to return from setjmp(). */
extern void longjmp(jmp_buf buf, int val) __noreturn;
