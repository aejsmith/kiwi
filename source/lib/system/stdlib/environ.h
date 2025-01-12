/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
