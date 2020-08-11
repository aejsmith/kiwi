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
 * @brief               Event notification system.
 */

#include <mm/malloc.h>

#include <lib/notifier.h>

/** Structure defining a callback function on a notifier. */
typedef struct notifier_func {
    list_t header;                  /**< Link to notifier. */

    notifier_func_t func;           /**< Function to call. */
    void *data;                     /**< Second data argument for function. */
} notifier_entry_t;

/** Initialize a notifier.
 * @param notifier      Notifier to initialize.
 * @param data          Pointer to pass as first argument to functions. */
void notifier_init(notifier_t *notifier, void *data) {
    /* We use a recursive lock as when we're used for object events, a call
     * to object_event_signal() from notifier_run() can call the unwait
     * function which would call back into notifier_unregister(). */
    mutex_init(&notifier->lock, "notifier_lock", MUTEX_RECURSIVE);
    list_init(&notifier->functions);
    notifier->data = data;
}

/** Remove all functions from a notifier.
 * @param notifier      Notifier to destroy. */
void notifier_clear(notifier_t *notifier) {
    notifier_entry_t *entry;

    mutex_lock(&notifier->lock);

    while (!list_empty(&notifier->functions)) {
        entry = list_first(&notifier->functions, notifier_entry_t, header);
        list_remove(&entry->header);
        kfree(entry);
    }

    mutex_unlock(&notifier->lock);
}

/** Runs all functions on a notifier without taking the lock.
 * @param notifier      Notifier to run.
 * @param data          Pointer to pass as third argument to functions.
 * @param destroy       Whether to remove functions after calling them.
 * @return              Whether any handlers were called. */
bool notifier_run_unsafe(notifier_t *notifier, void *data, bool destroy) {
    notifier_entry_t *entry;
    bool called = false;

    /* The callback function may call into notifier_unregister(). */
    list_foreach_safe(&notifier->functions, iter) {
        entry = list_entry(iter, notifier_entry_t, header);

        entry->func(notifier->data, entry->data, data);
        if (destroy) {
            list_remove(&entry->header);
            kfree(entry);
        }

        called = true;
    }

    return called;
}

/** Runs all functions on a notifier.
 * @param notifier      Notifier to run.
 * @param data          Pointer to pass as third argument to functions.
 * @param destroy       Whether to remove functions after calling them.
 * @return              Whether any handlers were called. */
bool notifier_run(notifier_t *notifier, void *data, bool destroy) {
    bool ret;

    mutex_lock(&notifier->lock);
    ret = notifier_run_unsafe(notifier, data, destroy);
    mutex_unlock(&notifier->lock);

    return ret;
}

/** Add a function to a notifier.
 * @param notifier      Notifier to add to.
 * @param func          Function to add.
 * @param data          Pointer to pass as second argument to function. */
void notifier_register(notifier_t *notifier, notifier_func_t func, void *data) {
    notifier_entry_t *entry;

    entry = kmalloc(sizeof(notifier_entry_t), MM_KERNEL);
    list_init(&entry->header);
    entry->func = func;
    entry->data = data;

    mutex_lock(&notifier->lock);
    list_append(&notifier->functions, &entry->header);
    mutex_unlock(&notifier->lock);
}

/** Remove a function from a notifier.
 * @param notifier      Notifier to remove from.
 * @param func          Function to remove.
 * @param data          Data argument function was registered with. */
void notifier_unregister(notifier_t *notifier, notifier_func_t func, void *data) {
    notifier_entry_t *entry;

    mutex_lock(&notifier->lock);

    list_foreach_safe(&notifier->functions, iter) {
        entry = list_entry(iter, notifier_entry_t, header);

        if (entry->func == func && entry->data == data) {
            list_remove(&entry->header);
            kfree(entry);
        }
    }

    mutex_unlock(&notifier->lock);
}
