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
 * @brief               Mutex implementation.
 */

#ifndef __CORE_MUTEX_H
#define __CORE_MUTEX_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Type of a mutex. */
typedef int32_t core_mutex_t;

/** Initializer for a mutex. */
#define CORE_MUTEX_INITIALIZER       0

/** Statically defines a new mutex. */
#define CORE_MUTEX_DEFINE(_var) \
    core_mutex_t _var = CORE_MUTEX_INITIALIZER

extern bool core_mutex_held(core_mutex_t *mutex);
extern status_t core_mutex_lock(core_mutex_t *mutex, nstime_t timeout);
extern void core_mutex_unlock(core_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* __CORE_MUTEX_H */
