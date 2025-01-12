/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Futex functions.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

extern status_t kern_futex_wait(int32_t *addr, int32_t val, nstime_t timeout);
extern status_t kern_futex_wake(int32_t *addr, size_t count, size_t *_woken);
extern status_t kern_futex_requeue(
    int32_t *addr1, int32_t val, size_t count, int32_t *addr2, size_t *_woken);

__KERNEL_EXTERN_C_END
