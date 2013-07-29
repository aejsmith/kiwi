/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Mutex implementation.
 */

#ifndef __KERNEL_MUTEX_H
#define __KERNEL_MUTEX_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initializer for a mutex. */
#define MUTEX_INITIALIZER		0

#ifndef KERNEL

extern bool kern_mutex_held(int32_t *mutex);
extern status_t kern_mutex_lock(int32_t *lock, nstime_t timeout);
extern void kern_mutex_unlock(int32_t *lock);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_MUTEX_H */
