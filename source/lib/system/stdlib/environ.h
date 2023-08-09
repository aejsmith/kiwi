/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Environment helper functions.
 */

#pragma once

#include <core/mutex.h>

/** Environment structure. */
typedef struct environ {
    core_mutex_t lock;
    char ***array_ptr;
    bool alloced;
} environ_t;

static inline __sys_hidden void environ_init(environ_t *env, char ***array_ptr, bool alloced) {
    env->lock      = CORE_MUTEX_INITIALIZER;
    env->array_ptr = array_ptr;
    env->alloced   = alloced;
}

extern char *environ_get(environ_t *env, const char *name) __sys_hidden;
extern bool environ_set(environ_t *env, const char *name, const char *value, bool overwrite) __sys_hidden;
extern bool environ_unset(environ_t *env, const char *name) __sys_hidden;
extern void environ_free(environ_t *env) __sys_hidden;
