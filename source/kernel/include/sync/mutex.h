/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Mutex implementation.
 */

#pragma once

#include <lib/list.h>

#include <sync/spinlock.h>

struct thread;

/** Structure containing a mutex. */
typedef struct mutex {
    atomic_uint value;              /**< Lock count. */
    uint32_t flags;                 /**< Behaviour flags for the mutex. */
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t threads;                 /**< List of waiting threads. */
    struct thread *holder;          /**< Thread holding the lock. */
    const char *name;               /**< Name of the lock. */
    #if CONFIG_DEBUG
        void *caller;               /**< Return address of lock call. */
    #endif
} mutex_t;

/** Initializes a statically defined mutex. */
#define MUTEX_INITIALIZER(_var, _name, _flags) \
    { \
        .value = 0, \
        .flags = _flags, \
        .lock = SPINLOCK_INITIALIZER("mutex_lock"), \
        .threads = LIST_INITIALIZER(_var.threads), \
        .holder = NULL, \
        .name = _name, \
    }

/** Statically defines a new mutex. */
#define MUTEX_DEFINE(_var, _flags) \
    mutex_t _var = MUTEX_INITIALIZER(_var, #_var, _flags)

/** Mutex behaviour flags. */
#define MUTEX_RECURSIVE     (1<<0)  /**< Allow recursive locking by a thread. */

/** Check if a mutex is held.
 * @param lock          Mutex to check.
 * @return              Whether the mutex is held. */
static inline bool mutex_held(mutex_t *lock) {
    return atomic_load_explicit(&lock->value, memory_order_relaxed) != 0;
}

/** Get the current recursion count of a mutex.
 * @param lock          Mutex to check.
 * @return              Current recursion count of mutex. */
static inline int mutex_recursion(mutex_t *lock) {
    return atomic_load_explicit(&lock->value, memory_order_relaxed);
}

extern status_t mutex_lock_etc(mutex_t *lock, nstime_t timeout, uint32_t flags);
extern void mutex_lock(mutex_t *lock);
extern void mutex_unlock(mutex_t *lock);
extern void mutex_init(mutex_t *lock, const char *name, uint32_t flags);

static inline void __mutex_unlockp(void *p) {
    mutex_t *mutex = *(mutex_t **)p;

    if (mutex)
        mutex_unlock(mutex);
}

/**
 * RAII-style scoped lock. Locks the mutex, and defines a variable with a
 * cleanup attribute which will unlock the mutex once it goes out of scope.
 *
 * @param name          Scoped lock variable name.
 * @param mutex         Mutex to lock.
 */
#define MUTEX_SCOPED_LOCK(name, mutex) \
    mutex_lock(mutex); \
    mutex_t *name __unused __cleanup(__mutex_unlockp) = mutex;

/**
 * Explicit unlock for MUTEX_SCOPED_LOCK. If used, the lock will not be unlocked
 * again once the lock variable goes out of scope.
 *
 * @param name          Scoped lock variable name.
 */
#define MUTEX_SCOPED_UNLOCK(name) \
    mutex_unlock(name); \
    name = NULL;
