/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Condition object.
 */

#include <kernel/condition.h>

#include <lib/notifier.h>

#include <mm/malloc.h>

#include <assert.h>
#include <object.h>
#include <status.h>

/** Condition object. */
typedef struct condition {
    mutex_t lock;                   /**< Lock for the condition. */
    bool state;                     /**< Current state. */
    notifier_t notifier;            /**< Notifier to wait on. */
} condition_t;

static void condition_object_close(object_handle_t *handle) {
    condition_t *condition = handle->private;

    assert(notifier_empty(&condition->notifier));

    kfree(condition);
}

static status_t condition_object_wait(object_handle_t *handle, object_event_t *event) {
    condition_t *condition = handle->private;
    status_t ret;

    MUTEX_SCOPED_LOCK(lock, &condition->lock);

    switch (event->event) {
        case CONDITION_EVENT_SET:
            if (!(event->flags & OBJECT_EVENT_EDGE) && condition->state) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&condition->notifier, object_event_notifier, event);
            }

            ret = STATUS_SUCCESS;
            break;
        default:
            ret = STATUS_INVALID_EVENT;
            break;
    }

    return ret;
}

static void condition_object_unwait(object_handle_t *handle, object_event_t *event) {
    condition_t *condition = handle->private;

    switch (event->event) {
        case CONDITION_EVENT_SET:
            notifier_unregister(&condition->notifier, object_event_notifier, event);
            break;
    }
}

/** Condition object type. */
static const object_type_t condition_object_type = {
    .id     = OBJECT_TYPE_CONDITION,
    .flags  = OBJECT_TRANSFERRABLE,
    .close  = condition_object_close,
    .wait   = condition_object_wait,
    .unwait = condition_object_unwait,
};

/**
 * Sets a condition object's state. If the new state is true, any pending waits
 * on it will be signalled.
 *
 * @param handle        Handle to condition object.
 * @param state         New state for the object.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_condition_set(handle_t handle, bool state) {
    status_t ret;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONDITION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    condition_t *condition = khandle->private;

    MUTEX_SCOPED_LOCK(lock, &condition->lock);

    condition->state = state;

    if (state)
        notifier_run(&condition->notifier, NULL, false);

    return STATUS_SUCCESS;
}

/**
 * Creates a new condition object. A condition object is essentially a boolean
 * flag, which can be waited on to become true. The initial state is false.
 *
 * @param _handle       Where to return handle to object.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_condition_create(handle_t *_handle) {
    if (!_handle)
        return STATUS_INVALID_ARG;

    condition_t *condition = kmalloc(sizeof(*condition), MM_KERNEL);

    mutex_init(&condition->lock, "condition_lock", 0);
    notifier_init(&condition->notifier, NULL);

    condition->state = false;

    status_t ret = object_handle_open(&condition_object_type, condition, NULL, _handle);
    if (ret != STATUS_SUCCESS)
        kfree(condition);

    return ret;
}
