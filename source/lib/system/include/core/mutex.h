/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Mutex implementation.
 */

#pragma once

#include <system/defs.h>

#include <kernel/types.h>

__SYS_EXTERN_C_BEGIN

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

static inline void __core_mutex_unlockp(void *p) {
    core_mutex_t *mutex = *(core_mutex_t **)p;

    if (mutex)
        core_mutex_unlock(mutex);
}

/**
 * RAII-style scoped lock for use in C code. Locks the mutex, and defines a
 * variable with a cleanup attribute which will unlock the mutex once it goes
 * out of scope.
 *
 * @param name          Scoped lock variable name.
 * @param mutex         Mutex to lock.
 */
#define CORE_MUTEX_SCOPED_LOCK(name, mutex) \
    core_mutex_lock(mutex, -1); \
    core_mutex_t *name __sys_unused __sys_cleanup(__core_mutex_unlockp) = mutex;

/**
 * Explicit unlock for CORE_MUTEX_SCOPED_LOCK. If used, the lock will not be
 * unlocked again once the lock variable goes out of scope.
 *
 * @param name          Scoped lock variable name.
 */
#define CORE_MUTEX_SCOPED_UNLOCK(name) \
    core_mutex_unlock(name); \
    name = NULL;

__SYS_EXTERN_C_END
