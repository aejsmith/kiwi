/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief               POSIX thread-specific storage.
 *
 * TODO:
 *  - Key reuse. This would need to make sure the values are all set to NULL.
 *  - Call destructors when threads are actually implemented.
 */

#include <kernel/mutex.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

/** Structure containing global data slot information. */
typedef struct pthread_specific {
    bool allocated;                 /**< Whether this data slot is allocated. */
    void (*dtor)(void *);           /**< Destructor function. */
} pthread_specific_t;

/** Next available thread-specific data key. */
static volatile int32_t next_pthread_key;

/** Global data slot information. */
static pthread_specific_t pthread_specific[PTHREAD_KEYS_MAX];

/** Per-thread data values. */
static __thread void *pthread_specific_values[PTHREAD_KEYS_MAX];

/**
 * Create a thread-specific data key.
 *
 * Creates a new thread-specific data key that can be used by all threads in
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
    pthread_key_t key;

    /* Try to allocate a new key. */
    while (true) {
        key = next_pthread_key;

        if (key >= PTHREAD_KEYS_MAX)
            return EAGAIN;

        if (__sync_bool_compare_and_swap(&next_pthread_key, key, key + 1))
            break;
    }

    assert(!pthread_specific[key].allocated);
    pthread_specific[key].allocated = true;
    pthread_specific[key].dtor = dtor;

    *_key = key;
    return 0;
}

/**
 * Delete a thread-specific data key.
 *
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
    if (!pthread_specific[key].allocated)
        return EINVAL;

    pthread_specific[key].allocated = false;
    return 0;
}

/** Get an item of thread-specific data.
 * @param key           Key for item to get.
 * @return              Value retrieved. If the key is invalid, NULL will be
 *                      returned. */
void *pthread_getspecific(pthread_key_t key) {
    if (!pthread_specific[key].allocated)
        return NULL;

    return pthread_specific_values[key];
}

/** Set an item of thread-specific data.
 * @param key           Key for item to set.
 * @param val           Value to set.
 * @return              0 if value set successfully, EINVAL if key is invalid. */
int pthread_setspecific(pthread_key_t key, const void *val) {
    if (!pthread_specific[key].allocated)
        return EINVAL;

    pthread_specific_values[key] = (void *)val;
    return 0;
}
