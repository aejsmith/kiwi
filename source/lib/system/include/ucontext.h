/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               User context definition.
 */

#pragma once

#include <signal.h>

__SYS_EXTERN_C_BEGIN

/* int getcontext(ucontext_t *); */
/* int setcontext(const ucontext_t *); */
/* void makecontext(ucontext_t *, (void *)(), int, ...); */
/* int swapcontext(ucontext_t *, const ucontext_t *); */

__SYS_EXTERN_C_END
