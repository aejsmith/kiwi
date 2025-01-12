/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX thread-specific storage.
 *
 * TODO:
 *  - Key reuse. This would need to make sure the values are all set to NULL.
 */

#include "pthread/pthread.h"

#include "libsystem.h"

#include <kernel/private/thread.h>
#include <kernel/status.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

/** Structure containing global data slot information. */
typedef struct pthread_specific {
    atomic_bool allocated;          /**< Whether this data slot is allocated. */
    void (*dtor)(void *);           /**< Destructor function. */
} pthread_specific_t;

/** Next available thread-specific data key. */
static volatile atomic_int next_pthread_key;

/** Global data slot information. */
static pthread_specific_t pthread_specific[PTHREAD_KEYS_MAX];

/** Per-thread data values. */
static __thread void *pthread_specific_values[PTHREAD_KEYS_MAX];

/** Number of currently registered keys with destructors. */
static atomic_uint specific_dtor_count = 0;

static void run_specific_dtors(void) {
    if (atomic_load_explicit(&specific_dtor_count, memory_order_acquire) > 0) {
        for (size_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
            if (atomic_load_explicit(&pthread_specific[i].allocated, memory_order_acquire) &&
                pthread_specific[i].dtor &&
                pthread_specific_values[i])
            {
                pthread_specific[i].dtor(pthread_specific_values[i]);
            }
        }
    }
}

static __sys_init_prio(LIBSYSTEM_INIT_PRIO_PTHREAD_SPECIFIC) void pthread_specific_init(void) {
    status_t ret = kern_thread_add_dtor(run_specific_dtors);
    libsystem_assert(ret == STATUS_SUCCESS);
}

/**
 * Creates a new thread-specific data key. The key can be used by all threads in
 * the process to store data local to that thread using pthread_getspecific()
 * and pthread_setspecific().
 *
 * When the key is first created, the value associated with the key will be
 * NULL in all threads. When a thread exits, if a key value is non-NULL, the
 * destructor function (if any) will be called on it. The order of destructor
 * calls is unspecified.
 *
 * @param _key          Where to store created key.
 * @param dtor          Destructor function (can be NULL).
 *
 * @return              0 on success, or EAGAIN if the maximum number of keys
 *                      per process has been exceeded.
 */
int pthread_key_create(pthread_key_t *_key, void (*dtor)(void *val)) {
    /* Try to allocate a new key. */
    pthread_key_t key;
    while (true) {
        key = atomic_load_explicit(&next_pthread_key, memory_order_relaxed);

        if (key >= PTHREAD_KEYS_MAX)
            return EAGAIN;

        if (atomic_compare_exchange_weak_explicit(
                &next_pthread_key, &key, key + 1, memory_order_relaxed,
                memory_order_relaxed))
            break;
    }

    assert(!atomic_load_explicit(&pthread_specific[key].allocated, memory_order_relaxed));

    pthread_specific[key].dtor = dtor;

    atomic_store_explicit(&pthread_specific[key].allocated, true, memory_order_release);

    if (dtor)
        atomic_fetch_add_explicit(&specific_dtor_count, 1, memory_order_acq_rel);

    *_key = key;
    return 0;
}

/**
 * Deletes the given thread-specific data key. The values associated with the
 * key need not be NULL at the time of deletion, but the destructor function
 * will not be called: it is the responsibility of the application to ensure
 * that data is freed.
 *
 * @param key           Key to delete.
 *
 * @return              Always returns 0.
 */
int pthread_key_delete(pthread_key_t key) {
    if (!atomic_load_explicit(&pthread_specific[key].allocated, memory_order_relaxed))
        return EINVAL;

    if (pthread_specific[key].dtor)
        atomic_fetch_sub_explicit(&specific_dtor_count, 1, memory_order_acq_rel);

    atomic_store_explicit(&pthread_specific[key].allocated, false, memory_order_release);
    return 0;
}

/** Get an item of thread-specific data.
 * @param key           Key for item to get.
 * @return              Value retrieved. If the key is invalid, NULL will be
 *                      returned. */
void *pthread_getspecific(pthread_key_t key) {
    if (!atomic_load_explicit(&pthread_specific[key].allocated, memory_order_relaxed))
        return NULL;

    return pthread_specific_values[key];
}

/** Set an item of thread-specific data.
 * @param key           Key for item to set.
 * @param val           Value to set.
 * @return              0 if value set successfully, EINVAL if key is invalid. */
int pthread_setspecific(pthread_key_t key, const void *val) {
    if (!atomic_load_explicit(&pthread_specific[key].allocated, memory_order_relaxed))
        return EINVAL;

    pthread_specific_values[key] = (void *)val;
    return 0;
}
