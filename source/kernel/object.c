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
 * @brief               Kernel object manager.
 *
 * TODO:
 *  - Make handle tables resizable, based on process limits or something (e.g.
 *    rlimit).
 *  - Multi-level array for handle tables? It's quite large at the moment.
 */

#include <lib/bitmap.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <kdb.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

/** Define to enable debug output on handle creation/close. */
//#define DEBUG_HANDLE

#ifdef DEBUG_HANDLE
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Maximum number of handles. */
#define HANDLE_TABLE_SIZE   512

/** Object waiter structure. */
typedef struct object_waiter {
    list_t header;                  /**< Link to waiters list. */
    spinlock_t lock;                /**< Lock for the structure. */
    thread_t *thread;               /**< Thread which is waiting. */
    size_t count;                   /**< Number of remaining events to be signalled. */
} object_waiter_t;

/** Object waiting internal data structure. */
typedef struct object_wait {
    object_event_t event;           /**< User-supplied event information. */
    object_handle_t *handle;        /**< Handle being waited on. */
    status_t status;                /**< Wait status. */

    /** Type of the wait. */
    enum {
        OBJECT_WAIT_NORMAL,         /**< Wait is a call to kern_object_wait(). */
        OBJECT_WAIT_CALLBACK,       /**< Wait is a callback. */
    } type;

    union {
        /** Normal wait data. */
        struct {
            /** Waiter calling kern_object_wait(). */
            object_waiter_t *waiter;

            list_t waits_link;      /**< Link to list of all wait structures. */
        };

        /** Callback data. */
        struct {
            thread_t *thread;       /**< Target thread. */
            list_t handle_link;     /**< Link to handle events list. */
            list_t thread_link;     /**< Link to thread events list. */

            /** Callback function. */
            object_callback_t callback;

            uint32_t priority;      /**< Callback priority. */

            /** Pre-allocated interrupt structure. */
            thread_interrupt_t *interrupt;

            /**
             * Interrupt state:
             *  - 0 = Not pending, active.
             *  - 1 = Pending.
             *  - 2 = Removing (used to arbitrate who frees the wait if the
             *        process attempts to remove the callback while it is
             *        pending).
             */
            atomic_uint32_t interrupt_state;
        };
    };
} object_wait_t;

/** Cache for handle structures. */
static slab_cache_t *object_handle_cache;

/** Cache for object wait structures. */
static slab_cache_t *object_wait_cache;

/** Object type names. */
static const char *object_type_names[] = {
    [OBJECT_TYPE_PROCESS]       = "OBJECT_TYPE_PROCESS",
    [OBJECT_TYPE_THREAD]        = "OBJECT_TYPE_THREAD",
    [OBJECT_TYPE_TOKEN]         = "OBJECT_TYPE_TOKEN",
    [OBJECT_TYPE_TIMER]         = "OBJECT_TYPE_TIMER",
    [OBJECT_TYPE_WATCHER]       = "OBJECT_TYPE_WATCHER",
    [OBJECT_TYPE_AREA]          = "OBJECT_TYPE_AREA",
    [OBJECT_TYPE_FILE]          = "OBJECT_TYPE_FILE",
    [OBJECT_TYPE_PORT]          = "OBJECT_TYPE_PORT",
    [OBJECT_TYPE_CONNECTION]    = "OBJECT_TYPE_CONNECTION",
    [OBJECT_TYPE_SEMAPHORE]     = "OBJECT_TYPE_SEMAPHORE",
    [OBJECT_TYPE_PROCESS_GROUP] = "OBJECT_TYPE_PROCESS_GROUP",
    [OBJECT_TYPE_CONDITION]     = "OBJECT_TYPE_CONDITION",
};

/**
 * Creates a new handle to a kernel object. The handle will have a single
 * reference on it. The handle must be closed with object_handle_release() when
 * it is no longer required.
 *
 * @param type          Type of the object.
 * @param private       Per-handle data pointer. This can be a pointer to the
 *                      object, or for object types that need per-handle state,
 *                      a pointer to a structure containing the object pointer
 *                      plus the required state.
 *
 * @return              Handle to the object.
 */
object_handle_t *object_handle_create(const object_type_t *type, void *private) {
    object_handle_t *handle = slab_cache_alloc(object_handle_cache, MM_WAIT);

    refcount_set(&handle->count, 1);

    handle->type    = type;
    handle->private = private;

    return handle;
}

/**
 * Increases the reference count of a handle, ensuring that it will not be
 * freed. When the handle is no longer required, you must call
 * object_handle_release() on it.
 * 
 * @param handle        Handle to increase reference count of.
 */
void object_handle_retain(object_handle_t *handle) {
    assert(handle);

    refcount_inc(&handle->count);
}

/**
 * Decreases the reference count of a handle. If no more references remain to
 * the handle, it will be closed.
 *
 * @param handle        Handle to release.
 */
void object_handle_release(object_handle_t *handle) {
    assert(handle);

    /* If there are no more references we can close it. */
    if (refcount_dec(&handle->count) == 0) {
        if (handle->type->close)
            handle->type->close(handle);

        slab_cache_free(object_handle_cache, handle);
    }
}

/** Looks up a handle with the table locked.
 * @param id            Handle ID to look up.
 * @param type          Required object type ID (if negative, no type checking
 *                      will be performed).
 * @param _handle       Where to store pointer to handle structure.
 * @return              Status code describing result of the operation. */
static status_t lookup_handle(handle_t id, int type, object_handle_t **_handle) {
    assert(_handle);

    if (id < 0 || id >= HANDLE_TABLE_SIZE)
        return STATUS_INVALID_HANDLE;

    handle_table_t *table   = &curr_proc->handles;
    object_handle_t *handle = table->handles[id];

    if (!handle)
        return STATUS_INVALID_HANDLE;

    /* Check if the type is the type the caller wants. */
    if (type >= 0 && handle->type->id != (unsigned)type)
        return STATUS_INVALID_HANDLE;

    object_handle_retain(handle);

    *_handle = handle;
    return STATUS_SUCCESS;
}

/** Attaches a handle to the current process' handle table.
 * @param handle        Handle to attach.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 * @param retain        Whether to retain the handle.
 * @return              Status code describing result of the operation. */
static status_t attach_handle(object_handle_t *handle, handle_t *_id, handle_t *_uid, bool retain) {
    assert(handle);
    assert(_id || _uid);

    handle_table_t *table = &curr_proc->handles;

    /* Find a handle ID in the table. */
    handle_t id = bitmap_ffz(table->bitmap, HANDLE_TABLE_SIZE);
    if (id < 0)
        return STATUS_NO_HANDLES;

    if (_id)
        *_id = id;

    if (_uid) {
        status_t ret = write_user(_uid, id);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    if (retain)
        object_handle_retain(handle);

    if (handle->type->attach)
        handle->type->attach(handle, curr_proc);

    table->handles[id] = handle;
    table->flags[id] = 0;
    bitmap_set(table->bitmap, id);

    dprintf(
        "object: allocated handle %" PRId32 " in process %" PRId32 " (type: %u, private: %p)\n",
        id, curr_proc->id, handle->type->id, handle->private);

    return STATUS_SUCCESS;
}

static void free_callback(object_wait_t *wait) {
    object_handle_release(wait->handle);
    kfree(wait->interrupt);
    slab_cache_free(object_wait_cache, wait);
}

/** Removes a callback (handle table must be locked). */
static void remove_callback(object_wait_t *wait) {
    wait->handle->type->unwait(wait->handle, &wait->event);

    list_remove(&wait->handle_link);
    list_remove(&wait->thread_link);

    /* Only free if it wasn't pending, otherwise the interrupt will clear up. */
    uint32_t prev_state = atomic_exchange(&wait->interrupt_state, 2);
    if (prev_state == 0)
        free_callback(wait);
}

/** Detaches a handle from the current process' handle table. */
static status_t detach_handle(handle_t id) {
    handle_table_t *table = &curr_proc->handles;

    if (id < 0 || id >= HANDLE_TABLE_SIZE || !table->handles[id])
        return STATUS_INVALID_HANDLE;

    object_handle_t *handle = table->handles[id];

    if (handle->type->detach)
        handle->type->detach(handle, curr_proc);

    /* Unregister any callbacks registered. */
    while (!list_empty(&table->callbacks[id])) {
        object_wait_t *wait = list_first(&table->callbacks[id], object_wait_t, handle_link);
        remove_callback(wait);
    }

    dprintf(
        "object: detached handle %" PRId32 " from process %" PRId32 " (count: %d)\n",
        id, curr_proc->id, refcount_get(&handle->count));

    object_handle_release(handle);
    table->handles[id] = NULL;
    table->flags[id] = 0;
    bitmap_clear(table->bitmap, id);
    return STATUS_SUCCESS;
}

/**
 * Looks up the handle with the given ID in the current process' handle table,
 * optionally checking that the object it refers to is a certain type. The
 * returned handle will be referenced: when it is no longer needed, it should
 * be released with object_handle_release().
 *
 * @param id            Handle ID to look up.
 * @param type          Required object type ID (if negative, no type checking
 *                      will be performed).
 * @param _handle       Where to store pointer to handle structure.
 *
 * @return              Status code describing result of the operation.
 */
status_t object_handle_lookup(handle_t id, int type, object_handle_t **_handle) {
    rwlock_read_lock(&curr_proc->handles.lock);
    status_t ret = lookup_handle(id, type, _handle);
    rwlock_unlock(&curr_proc->handles.lock);
    return ret;
}

/**
 * Allocates a handle ID for the current process and adds a handle to its
 * handle table. On success, the handle will have an extra reference on it.
 *
 * @param handle        Handle to attach.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 *
 * @return              Status code describing result of the operation.
 */
status_t object_handle_attach(object_handle_t *handle, handle_t *_id, handle_t *_uid) {
    rwlock_write_lock(&curr_proc->handles.lock);
    status_t ret = attach_handle(handle, _id, _uid, true);
    rwlock_unlock(&curr_proc->handles.lock);
    return ret;
}

/**
 * Removes the specified handle ID from the current process' handle table and
 * releases the handle.
 *
 * @param id            ID of handle to detach.
 * @param _uid          If not NULL, a user location that the handle was written
 *                      to. This will be set back to INVALID_HANDLE. This is
 *                      important to maintain the guarantee made for all kernel
 *                      APIs that if a call that returns a handle fails, we do
 *                      not leave a valid but closed handle value in the handle
 *                      return location. Kiwi::Core::Handle::attach() relies on
 *                      this behaviour.
 *
 * @return              Status code describing result of the operation.
 */
status_t object_handle_detach(handle_t id, handle_t *_uid) {
    rwlock_write_lock(&curr_proc->handles.lock);
    status_t ret = detach_handle(id);
    rwlock_unlock(&curr_proc->handles.lock);

    if (_uid)
        write_user(_uid, INVALID_HANDLE);

    return ret;
}

/**
 * Allocates a handle ID in the current process and creates a new handle in its
 * handle table. This is a shortcut for creating a new handle with
 * object_handle_create() and then attaching it with object_handle_attach().
 * The behaviour of this function also differs slightly from doing that: if
 * attaching the handle fails, the object type's close method will not be
 * called. Note that as soon as this function succeeds, it is possible for the
 * process to close the handle and cause it to be released.
 *
 * @param type          Type of the object.
 * @param private       Per-handle data pointer. This can be a pointer to the
 *                      object, or for object types that need per-handle state,
 *                      a pointer to a structure containing the object pointer
 *                      plus the required state.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 *
 * @return              Status code describing result of the operation.
 */
status_t object_handle_open(const object_type_t *type, void *private, handle_t *_id, handle_t *_uid) {
    object_handle_t *handle = object_handle_create(type, private);

    rwlock_write_lock(&curr_proc->handles.lock);
    status_t ret = attach_handle(handle, _id, _uid, false);
    rwlock_unlock(&curr_proc->handles.lock);

    if (ret != STATUS_SUCCESS)
        slab_cache_free(object_handle_cache, handle);

    return ret;
}

/** Initializes a process' handle table.
 * @param process       Process to initialize. */
void object_process_init(process_t *process) {
    handle_table_t *table = &process->handles;

    rwlock_init(&table->lock, "handle_table_lock");

    table->handles   = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->handles[0]), MM_KERNEL);
    table->flags     = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->flags[0]), MM_KERNEL);
    table->callbacks = kmalloc(HANDLE_TABLE_SIZE * sizeof(table->callbacks[0]), MM_KERNEL);
    table->bitmap    = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);

    for (size_t i = 0; i < HANDLE_TABLE_SIZE; i++)
        list_init(&table->callbacks[i]);
}

/** Destroys a process' handle table.
 * @param process       Process to clean up. */
void object_process_cleanup(process_t *process) {
    handle_table_t *table = &process->handles;

    for (size_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        object_handle_t *handle = table->handles[i];

        if (handle) {
            if (handle->type->detach)
                handle->type->detach(handle, process);

            dprintf(
                "object: detached handle %" PRId32 " from process %" PRId32 " (count: %d)\n",
                i, process->id, refcount_get(&handle->count));

            object_handle_release(handle);
        }

        /* Callback list should be empty, by this point all threads should have
         * been cleaned up and therefore removed their callbacks. */
        assert(list_empty(&table->callbacks[i]));
    }

    kfree(table->bitmap);
    kfree(table->callbacks);
    kfree(table->flags);
    kfree(table->handles);
}

/** Inherits a handle from one table to another.
 * @param table         Table to duplicate to.
 * @param dest          Destination handle ID.
 * @param parent        Parent table.
 * @param source        Source handle ID.
 * @param process       Process to attach to, if any.
 * @return              Status code describing result of the operation. */
static status_t inherit_handle(
    handle_table_t *table, handle_t dest, handle_table_t *parent,
    handle_t source, process_t *process)
{
    if (source < 0 || source >= HANDLE_TABLE_SIZE) {
        return STATUS_INVALID_HANDLE;
    } else if (dest < 0 || dest >= HANDLE_TABLE_SIZE) {
        return STATUS_INVALID_HANDLE;
    } else if (!parent->handles[source]) {
        return STATUS_INVALID_HANDLE;
    } else if (table->handles[dest]) {
        return STATUS_ALREADY_EXISTS;
    }

    object_handle_t *handle = parent->handles[source];

    /* When using a map, the inheritable flag is ignored so we must check
     * whether transferring handles is allowed. */
    if (!(handle->type->flags & OBJECT_TRANSFERRABLE))
        return STATUS_NOT_SUPPORTED;

    object_handle_retain(handle);

    if (process && handle->type->attach)
        handle->type->attach(handle, process);

    table->handles[dest] = handle;
    table->flags[dest]   = parent->flags[source];

    bitmap_set(table->bitmap, dest);

    return STATUS_SUCCESS;
}

/** Duplicates handles to a new process.
 * @param process       Newly created process.
 * @param parent        Parent process.
 * @param map           An array specifying handles to add to the new table.
 *                      The first ID of each entry specifies the handle in the
 *                      parent, and the second specifies the ID to give it in
 *                      the new table. Can be NULL if count <= 0.
 * @param count         The number of handles in the array. If negative, the
 *                      map will be ignored and all handles with the inheritable
 *                      flag set will be duplicated. If 0, no handles will be
 *                      duplicated.
 * @return              Status code describing result of the operation. */
status_t object_process_create(process_t *process, process_t *parent, handle_t map[][2], ssize_t count) {
    status_t ret;

    if (!count)
        return STATUS_SUCCESS;

    rwlock_read_lock(&parent->handles.lock);

    if (count > 0) {
        assert(map);

        for (handle_t i = 0; i < count; i++) {
            ret = inherit_handle(&process->handles, map[i][1], &parent->handles, map[i][0], process);
            if (ret != STATUS_SUCCESS)
                goto out;
        }
    } else {
        /* Inherit all inheritable handles in the parent table. */
        for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
            /* Flag can only be set if handle is not NULL and the type allows
             * transferring. */
            if (parent->handles.flags[i] & HANDLE_INHERITABLE)
                inherit_handle(&process->handles, i, &parent->handles, i, process);
        }
    }

    ret = STATUS_SUCCESS;

out:
    /* We don't need to clean up on failure - this will be done when the process
     * gets destroyed. */
    rwlock_unlock(&parent->handles.lock);
    return ret;
}

/** Closes handles when executing a new program.
 * @param map           An array specifying handles to add to the new table.
 *                      The first ID of each entry specifies the handle in the
 *                      parent, and the second specifies the ID to give it in
 *                      the new table. Can be NULL if count <= 0.
 * @param count         The number of handles in the array. If negative, the
 *                      map will be ignored and all handles with the inheritable
 *                      flag set will be duplicated. If 0, no handles will be
 *                      duplicated.
 * @return              Status code describing result of the operation. */
status_t object_process_exec(handle_t map[][2], ssize_t count) {
    status_t ret;

    /*
     * The attach and detach callbacks are used by IPC code to track when ports
     * are no longer referenced by their owning process. When we exec a process,
     * that should count as giving up ownership of ports. Therefore, this
     * function has to do a somewhat complicated dance to ensure that this
     * happens: we first populate a new table without calling attach on any
     * handles. If that succeeds, we then call detach on all handles in the old
     * table, and *then* call attach on all in the new table. This ensures that
     * the IPC code correctly sees that all references from the process to any
     * ports it owns are dropped and disowns the ports, before we re-add any
     * references.
     */

    handle_table_t new;
    new.handles   = kcalloc(HANDLE_TABLE_SIZE, sizeof(new.handles[0]), MM_KERNEL);
    new.flags     = kcalloc(HANDLE_TABLE_SIZE, sizeof(new.flags[0]), MM_KERNEL);
    new.callbacks = kmalloc(HANDLE_TABLE_SIZE * sizeof(new.callbacks[0]), MM_KERNEL);
    new.bitmap    = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);

    /* We don't inherit any callbacks. */
    for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++)
        list_init(&new.callbacks[i]);

    if (count > 0) {
        assert(map);

        for (handle_t i = 0; i < count; i++) {
            ret = inherit_handle(&new, map[i][1], &curr_proc->handles, map[i][0], NULL);
            if (ret != STATUS_SUCCESS) {
                while (i--) {
                    object_handle_t *handle = new.handles[map[i][1]];
                    object_handle_release(handle);
                }

                goto out;
            }
        }
    } else if (count < 0) {
        for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
            if (curr_proc->handles.flags[i] & HANDLE_INHERITABLE)
                inherit_handle(&new, i, &curr_proc->handles, i, NULL);
        }
    }

    /* Clean up all callbacks for the current thread. */
    object_thread_cleanup(curr_thread);

    /* Now we can detach and release all handles in the old table. */
    for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        object_handle_t *handle = curr_proc->handles.handles[i];

        if (handle) {
            if (handle->type->detach)
                handle->type->detach(handle, curr_proc);

            object_handle_release(handle);
        }

        /* At this point there should be no callbacks, all other threads are
         * terminated and we cleaned up the current thread's callbacks above. */
        assert(list_empty(&curr_proc->handles.callbacks[i]));
    }

    swap(curr_proc->handles.handles, new.handles);
    swap(curr_proc->handles.flags, new.flags);
    swap(curr_proc->handles.callbacks, new.callbacks);
    swap(curr_proc->handles.bitmap, new.bitmap);

    /* Finally, attach all handles in the new table. */
    for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        object_handle_t *handle = curr_proc->handles.handles[i];

        if (handle && handle->type->attach)
            handle->type->attach(handle, curr_proc);
    }

    ret = STATUS_SUCCESS;

out:
    kfree(new.bitmap);
    kfree(new.callbacks);
    kfree(new.flags);
    kfree(new.handles);
    return ret;
}

/** Clones handles from a new process' parent.
 * @param process       New process.
 * @param parent        Parent process. */
void object_process_clone(process_t *process, process_t *parent) {
    rwlock_read_lock(&parent->handles.lock);

    for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        /* inherit_handle() ignores non-transferrable handles. */
        if (parent->handles.handles[i])
            inherit_handle(&process->handles, i, &parent->handles, i, process);
    }

    rwlock_unlock(&parent->handles.lock);
}

/** Cleans up callbacks registered by a thread.
 * @param thread        Thread to clean up. */
void object_thread_cleanup(thread_t *thread) {
    rwlock_write_lock(&thread->owner->handles.lock);

    while (!list_empty(&thread->callbacks)) {
        object_wait_t *wait = list_first(&thread->callbacks, object_wait_t, thread_link);
        remove_callback(wait);
    }

    rwlock_unlock(&thread->owner->handles.lock);
}

static void post_object_event_interrupt(thread_interrupt_t *interrupt) {
    object_wait_t *wait = interrupt->cb_data;

    assert(atomic_load(&wait->interrupt_state) > 0);

    if (wait->event.flags & OBJECT_EVENT_ONESHOT) {
        rwlock_write_lock(&curr_proc->handles.lock);

        /* The wait is one-shot. If the callback hasn't already been removed,
         * remove it. This won't free since we're still marked as pending. */
        if (atomic_load(&wait->interrupt_state) != 2)
            remove_callback(wait);

        rwlock_unlock(&curr_proc->handles.lock);
    }

    uint32_t expected = 1;
    if (!atomic_compare_exchange_strong(&wait->interrupt_state, &expected, 0)) {
        /* Must have been removed. We must take care of freeing it. */
        assert(expected == 2);

        free_callback(wait);
    }
}

/**
 * Signals that an event being waited for has occurred or resulted in an error.
 * Error status should only be used where it is not possible to determine
 * error conditions at the point where the wait is first set up, e.g. user_file.
 *
 * This is safe to call in interrupt context.
 *
 * @param event         Object event structure.
 * @param data          Event data to return to waiter.
 * @param status        Error status to return to the waiter. If this is not
 *                      STATUS_SUCCESS then the event is marked as ERROR rather
 *                      than SIGNALLED.
 */
void object_event_signal_etc(object_event_t *event, unsigned long data, status_t status) {
    object_wait_t *wait = container_of(event, object_wait_t, event);
    
    wait->status = status;

    event->data   = data;
    event->flags |= (status == STATUS_SUCCESS) ? OBJECT_EVENT_SIGNALLED : OBJECT_EVENT_ERROR;

    switch (wait->type) {
        case OBJECT_WAIT_NORMAL: {
            spinlock_lock(&wait->waiter->lock);

            /* Don't decrement the count if its already 0, only wake if we
             * haven't already woken it. */
            if (wait->waiter->count && --wait->waiter->count == 0 && wait->waiter->thread) {
                thread_wake(wait->waiter->thread);
                wait->waiter->thread = NULL;
            }

            spinlock_unlock(&wait->waiter->lock);
            break;
        }
        case OBJECT_WAIT_CALLBACK: {
            /*
             * We only allow one pending interrupt at a time. There are 2
             * reasons for this:
             *  - It prevents pending interrupts from piling up if an event is
             *    repeatedly firing but the thread's IPL currently blocks the
             *    interrupt.
             *  - It means we only need to have one thread_interrupt_t allocated
             *    at a time, which allows us to pre-allocate it with the
             *    object_wait_t. Otherwise, we would have to allocate here,
             *    which would prevent usage of this function from interrupt
             *    context.
             */
            unsigned expected = 0;
            if (atomic_compare_exchange_strong(&wait->interrupt_state, &expected, 1)) {
                wait->interrupt->priority   = wait->priority;
                wait->interrupt->post_cb    = post_object_event_interrupt;
                wait->interrupt->cb_data    = wait;
                wait->interrupt->handler    = (ptr_t)wait->callback;
                wait->interrupt->stack.base = NULL;
                wait->interrupt->stack.size = 0;
                wait->interrupt->size       = sizeof(*event);

                /* Manually copy member-wise, the structure has padding. */
                object_event_t *dest_event = (object_event_t *)(wait->interrupt + 1);
                dest_event->handle = event->handle;
                dest_event->event  = event->event;
                dest_event->flags  = event->flags;
                dest_event->data   = event->data;
                dest_event->udata  = event->udata;

                thread_interrupt(wait->thread, wait->interrupt);
            }

            break;
        }
    }
}

/**
 * Signals that an event being waited for has occurred. This is safe to call in
 * interrupt context.
 *
 * @param event         Object event structure.
 * @param data          Event data to return to waiter.
 */
void object_event_signal(object_event_t *event, unsigned long data) {
    object_event_signal_etc(event, data, STATUS_SUCCESS);
}

/** Notifier function to use for object waiting.
 * @param arg1          Unused.
 * @param arg2          Event structure pointer.
 * @param arg3          Data for the event (unsigned long cast to void *). */
void object_event_notifier(void *arg1, void *arg2, void *arg3) {
    object_event_signal(arg2, (unsigned long)arg3);
}

static char kdb_handle_name_buf[512];

/** Prints a list of a process' handles. */
static kdb_status_t kdb_cmd_handles(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s <process ID>\n\n", argv[0]);

        kdb_printf("Prints out a list of all currently open handles in a process.\n");
        return KDB_SUCCESS;
    } else if (argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    uint64_t id;
    if (kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    process_t *process = process_lookup_unsafe(id);
    if (!process) {
        kdb_printf("Invalid process ID.\n");
        return KDB_FAILURE;
    }

    kdb_printf("ID   Flags  Type                           Count Private            Name\n");
    kdb_printf("==   =====  ====                           ===== =======            ====\n");

    for (handle_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        object_handle_t *handle = process->handles.handles[i];
        if (!handle)
            continue;

        const char *name = NULL;
        if (handle->type->name_unsafe)
            name = handle->type->name_unsafe(handle, kdb_handle_name_buf, sizeof(kdb_handle_name_buf));
        if (!name)
            name = "<unnamed>";

        kdb_printf(
            "%-4" PRId32 " 0x%-4" PRIx32 " %-2u - %-25s %-5" PRId32 " %-18p %s\n",
            i, process->handles.flags[i], handle->type->id,
            (handle->type->id < array_size(object_type_names))
                ? object_type_names[handle->type->id]
                : "<UNKNOWN>",
            refcount_get(&handle->count), handle->private, name);
    }

    return KDB_SUCCESS;
}

/** Initializes the object manager. */
__init_text void object_init(void) {
    object_handle_cache = object_cache_create(
        "object_handle_cache",
        object_handle_t, NULL, NULL, NULL, 0, MM_BOOT);
    object_wait_cache = object_cache_create(
        "object_wait_cache",
        object_wait_t, NULL, NULL, NULL, 0, MM_BOOT);

    kdb_register_command("handles", "Inspect a process' handle table.", kdb_cmd_handles);
}

/** Gets the type of an object referred to by a handle.
 * @param handle        Handle to object.
 * @param _type         Where to store object type.
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_INVALID_HANDLE if handle is invalid. */
status_t kern_object_type(handle_t handle, unsigned *_type) {
    object_handle_t *khandle;
    status_t ret = object_handle_lookup(handle, -1, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = write_user(_type, khandle->type->id);
    object_handle_release(khandle);
    return ret;
}

/**
 * Waits until one or all of the specified events occurs on one or more kernel
 * objects, or until the timeout period expires. Note that this function is
 * better suited for waiting on small numbers of objects. For frequent waits
 * on a large number of objects, using the watcher API will yield better
 * performance.
 *
 * If the OBJECT_WAIT_ALL flag is specified, then the function will wait until
 * all of the given events occur, rather than just one of them. If a wait with
 * OBJECT_WAIT_ALL times out or is interrupted, some of the events may have
 * fired, so the events array will be updated with the status of each event.
 *
 * If an event has the OBJECT_EVENT_EDGE flag set, it will only be signalled
 * upon a change of the event condition from false to true - if it is already
 * true when this function is called, it will not be signalled. Otherwise, if
 * the event condition is true when this function is called, it will be
 * signalled immediately. The use of edge-triggered mode with this function is
 * somewhat racy: it is easy to miss edges and then potentially block
 * indefinitely. For reliable edge-triggered event tracking, use a watcher.
 *
 * @param events        Array of structures describing events to wait for. Upon
 *                      return, the flags field of each event will be updated
 *                      according to the result of the function. If an error
 *                      occurred while processing an event, it will have the
 *                      OBJECT_EVENT_ERROR flag set. If the event was signalled,
 *                      it will have the OBJECT_EVENT_SIGNALLED flag set.
 * @param count         Number of array entries.
 * @param flags         Behaviour flags for waiting.
 * @param timeout       Maximum time to wait in nanoseconds. A value of 0 will
 *                      cause the function to return immediately if no events
 *                      are signalled, and a value of -1 will block indefinitely
 *                      until one of events is signalled.
 *
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_INVALID_ARG if count is 0 or too big, or if
 *                      events is NULL.
 *                      STATUS_INVALID_HANDLE if a handle does not exist.
 *                      STATUS_INVALID_EVENT if an invalid event ID is used.
 *                      STATUS_WOULD_BLOCK if the timeout is 0 and no events
 *                      have already occurred.
 *                      STATUS_TIMED_OUT if the timeout expires.
 *                      STATUS_INTERRUPTED if the sleep was interrupted.
 */
status_t kern_object_wait(object_event_t *events, size_t count, uint32_t flags, nstime_t timeout) {
    status_t ret;

    /* TODO: Is this a sensible limit to impose? Do we even need one? If it gets
     * removed, should change MM_KERNEL to MM_USER below. */
    if (!count || count > 1024 || !events)
        return STATUS_INVALID_ARG;

    object_waiter_t waiter;

    spinlock_init(&waiter.lock, "object_waiter_lock");

    /* Thread is set to NULL initially so that object_event_signal() does not
     * try to wake us if an event is signalled while setting up the waits. */
    waiter.thread = NULL;
    waiter.count  = (flags & OBJECT_WAIT_ALL) ? count : 1;

    list_t waits;
    list_init(&waits);

    /* Copy across all event information and set up waits. */
    for (size_t i = 0; i < count; i++) {
        object_wait_t *wait = slab_cache_alloc(object_wait_cache, MM_KERNEL);

        ret = memcpy_from_user(&wait->event, &events[i], sizeof(wait->event));
        if (ret != STATUS_SUCCESS) {
            slab_cache_free(object_wait_cache, wait);
            goto out;
        }

        /* These are set by us on return so clear out existing flags. */
        wait->event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);

        wait->handle = NULL;
        wait->status = STATUS_SUCCESS;
        wait->type   = OBJECT_WAIT_NORMAL;
        wait->waiter = &waiter;

        list_init(&wait->waits_link);
        list_append(&waits, &wait->waits_link);

        ret = object_handle_lookup(wait->event.handle, -1, &wait->handle);
        if (ret != STATUS_SUCCESS) {
            wait->event.flags |= OBJECT_EVENT_ERROR;
            goto out;
        } else if (!wait->handle->type->wait || !wait->handle->type->unwait) {
            wait->event.flags |= OBJECT_EVENT_ERROR;
            ret = STATUS_INVALID_EVENT;
            goto out;
        }

        ret = wait->handle->type->wait(wait->handle, &wait->event);
        if (ret != STATUS_SUCCESS) {
            wait->event.flags |= OBJECT_EVENT_ERROR;
            goto out;
        }
    }

    spinlock_lock(&waiter.lock);

    /* Now we wait for the events. If all the events required have already been
     * signalled, don't sleep. */
    if (waiter.count == 0) {
        ret = STATUS_SUCCESS;
        spinlock_unlock(&waiter.lock);
    } else {
        waiter.thread = curr_thread;
        ret = thread_sleep(&waiter.lock, timeout, "object_wait", SLEEP_INTERRUPTIBLE | __SLEEP_NO_RELOCK);
    }

out:
    /* Cancel all waits which have been set up. */
    for (size_t i = 0; !list_empty(&waits); i++) {
        object_wait_t *wait = list_first(&waits, object_wait_t, waits_link);

        if (wait->handle) {
            /* ERROR and !STATUS_SUCCESS indicates that the error was set
             * later. Otherwise, we didn't finish setting up the wait. */
            if (!(wait->event.flags & OBJECT_EVENT_ERROR) || wait->status != STATUS_SUCCESS)
                wait->handle->type->unwait(wait->handle, &wait->event);

            object_handle_release(wait->handle);
        }

        /* Return the first error that was set later, if any. */
        if (ret == STATUS_SUCCESS)
            ret = wait->status;

        /* Write back the updated flags and event data. */
        status_t err = write_user(&events[i].flags, wait->event.flags);
        if (err != STATUS_SUCCESS) {
            ret = err;
        } else if (wait->event.flags & OBJECT_EVENT_SIGNALLED) {
            err = write_user(&events[i].data, wait->event.data);
            if (err != STATUS_SUCCESS)
                ret = err;
        }

        list_remove(&wait->waits_link);
        slab_cache_free(object_wait_cache, wait);
    }

    return ret;
}

/**
 * Registers a callback function to be called asynchronously via a thread
 * interrupt when the specified object event occurs. This function only
 * supports edge-triggered events: the OBJECT_EVENT_EDGE flag must be set. The
 * callback will be executed every time the event condition changes to become
 * true. If the OBJECT_EVENT_ONESHOT flag is set, the callback function will be
 * removed the first time the event occurs.
 *
 * Callbacks are per-thread, i.e. will be delivered to the thread that
 * registered it, and per-handle table entry, i.e. will be removed when the
 * handle table entry it was registered on is closed, rather than when all
 * entries in the process referring to the same underlying open handle are
 * closed. There can only be one callback registered at a time per handle ID/
 * event ID pair in a thread. If a callback for the event is already registered
 * in the current thread, it will be replaced. Passing NULL as the callback
 * function causes any callback registered in the current thread for the
 * specified event to be removed.
 *
 * The callback is registered with a priority which the callback interrupt will
 * be delivered with. Raising the IPL to above this priority will cause the
 * callback to be temporarily blocked. If the event occurs while the interrupt
 * is blocked, the callback will be executed as soon as the IPL is lowered to
 * unblock it. While the callback is executing, the IPL is raised to 1 above
 * it's priority. It is be restored to its previous value upon return from the
 * callback. If the callback function lowers the IPL to unblock itself and the
 * event occurs again before it returns, it will be re-entered. The nesting of
 * interrupt handlers that occurs in this case may result in a stack overflow,
 * and for this reason, it is recommended that callbacks do not lower the IPL
 * and instead let it be restored by the kernel after returning.
 *
 * @param event         Event description.
 * @param callback      Callback function to register, NULL to remove callback.
 * @param priority      Priority to deliver callback interrupt with. This must
 *                      be lower than THREAD_IPL_EXCEPTION.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if event is NULL or priority is
 *                      invalid.
 *                      STATUS_INVALID_HANDLE if a handle does not exist.
 *                      STATUS_INVALID_EVENT if an invalid event ID is used.
 *                      STATUS_NOT_SUPPORTED if OBJECT_EVENT_EDGE is not set.
 */
status_t kern_object_callback(object_event_t *event, object_callback_t callback, uint32_t priority) {
    status_t ret;

    if (!event || priority >= THREAD_IPL_EXCEPTION) {
        return STATUS_INVALID_ARG;
    } else if (callback && !is_user_address(callback)) {
        return STATUS_INVALID_ADDR;
    }

    object_event_t kevent;
    ret = memcpy_from_user(&kevent, event, sizeof(kevent));
    if (ret != STATUS_SUCCESS)
        return ret;

    kevent.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);
    if (!(kevent.flags & OBJECT_EVENT_EDGE))
        return STATUS_NOT_SUPPORTED;

    rwlock_write_lock(&curr_proc->handles.lock);

    /* See if we have a callback already registered to update. */
    list_foreach(&curr_thread->callbacks, iter) {
        object_wait_t *wait = list_entry(iter, object_wait_t, thread_link);

        if (wait->event.handle == kevent.handle && wait->event.event == kevent.event) {
            if (callback) {
                /* Wait is already set up - simply update the callback and
                 * priority. */
                wait->callback = callback;
                wait->priority = priority;
            } else {
                /* We're removing the callback. */
                remove_callback(wait);
            }

            rwlock_unlock(&curr_proc->handles.lock);
            return STATUS_SUCCESS;
        }
    }

    object_wait_t *wait = slab_cache_alloc(object_wait_cache, MM_KERNEL);

    list_init(&wait->handle_link);
    list_init(&wait->thread_link);
    memcpy(&wait->event, &kevent, sizeof(wait->event));

    wait->type            = OBJECT_WAIT_CALLBACK;
    wait->status          = STATUS_SUCCESS;
    wait->thread          = curr_thread;
    wait->callback        = callback;
    wait->priority        = priority;
    wait->interrupt       = kcalloc(1, sizeof(*wait->interrupt) + sizeof(kevent), MM_KERNEL);
    wait->interrupt_state = 0;

    /* Table is already locked. */
    ret = lookup_handle(wait->event.handle, -1, &wait->handle);
    if (ret != STATUS_SUCCESS) {
        goto err_free;
    } else if (!wait->handle->type->wait || !wait->handle->type->unwait) {
        ret = STATUS_INVALID_EVENT;
        goto err_release_handle;
    }

    ret = wait->handle->type->wait(wait->handle, &wait->event);
    if (ret != STATUS_SUCCESS)
        goto err_release_handle;

    list_append(&curr_proc->handles.callbacks[wait->event.handle], &wait->handle_link);
    list_append(&curr_thread->callbacks, &wait->thread_link);

    rwlock_unlock(&curr_proc->handles.lock);
    return STATUS_SUCCESS;

err_release_handle:
    object_handle_release(wait->handle);

err_free:
    kfree(wait->interrupt);
    slab_cache_free(object_wait_cache, wait);

    rwlock_unlock(&curr_proc->handles.lock);

    /* Not strictly necessary - there's only one event that could have had an
     * error, but let's be consistent. */
    kevent.flags |= OBJECT_EVENT_ERROR;
    status_t err = write_user(&event->flags, kevent.flags);
    if (err != STATUS_SUCCESS)
        ret = err;

    return ret;
}

/** Gets the flags set on a handle table entry.
 * @see                 kern_handle_set_flags().
 * @param handle        Handle to get flags for.
 * @param _flags        Where to store handle table entry flags.
 * @return              Status code describing result of the operation. */
status_t kern_handle_flags(handle_t handle, uint32_t *_flags) {
    if (handle < 0 || handle >= HANDLE_TABLE_SIZE)
        return STATUS_INVALID_HANDLE;

    handle_table_t *table = &curr_proc->handles;

    rwlock_read_lock(&table->lock);

    if (!table->handles[handle]) {
        rwlock_unlock(&table->lock);
        return STATUS_INVALID_HANDLE;
    }

    status_t ret = write_user(_flags, table->flags[handle]);

    rwlock_unlock(&table->lock);
    return ret;
}

/**
 * Sets the flags set on a handle table entry. Note that these flags affect the
 * handle table entry, not the actual open handle. Multiple handle table entries
 * across multiple processes can refer to the same handle, for example handles
 * inherited by new processes refer to the same underlying handle. Any flags
 * that can be set on the underlying handle are manipulated using an object
 * type-specific API.
 *
 * Only one flag is currently defined: HANDLE_INHERITABLE. This determines
 * whether the handle will be duplicated when creating a new process.
 *
 * @param handle        Handle to get flags for.
 * @param flags         New flags to set.
 *
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_INVALID_HANDLE if the handle does not exist.
 *                      STATUS_NOT_SUPPORTED if attempting to set
 *                      HANDLE_INHERITABLE on a handle to a non-transferrable
 *                      object.
 */
status_t kern_handle_set_flags(handle_t handle, uint32_t flags) {
    if (handle < 0 || handle >= HANDLE_TABLE_SIZE)
        return STATUS_INVALID_HANDLE;

    handle_table_t *table = &curr_proc->handles;

    /* Don't need to write lock just to set flags, it's atomic. */
    rwlock_read_lock(&table->lock);

    object_handle_t *khandle = table->handles[handle];
    if (!khandle) {
        rwlock_unlock(&table->lock);
        return STATUS_INVALID_HANDLE;
    }

    /* To set the inheritable flag, the object type must be transferrable. */
    if (flags & HANDLE_INHERITABLE) {
        if (!(khandle->type->flags & OBJECT_TRANSFERRABLE)) {
            rwlock_unlock(&table->lock);
            return STATUS_NOT_SUPPORTED;
        }
    }

    table->flags[handle] = flags;

    rwlock_unlock(&table->lock);
    return STATUS_SUCCESS;
}

/**
 * Duplicates an entry in the calling process' handle table. The new handle ID
 * will refer to the same underlying handle as the source ID, i.e. they will
 * share the same state, for example for file handles they will share the same
 * file offset, etc. The new table entry's flags will be set to 0.
 *
 * @param handle        Handle ID to duplicate.
 * @param dest          Destination handle ID. If INVALID_HANDLE is specified,
 *                      then a new handle ID is allocated. Otherwise, this
 *                      exact ID will be used and any existing handle referred
 *                      to by that ID will be closed.
 * @param _new          Where to store new handle ID. Can be NULL if dest is
 *                      not INVALID_HANDLE.
 *
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_INVALID_HANDLE if handle does not exist.
 *                      STATUS_INVALID_ARG if dest is invalid, or if dest is
 *                      INVALID_HANDLE and _new is NULL.
 *                      STATUS_NO_HANDLES if allocating a handle ID and the
 *                      handle table is full.
 */
status_t kern_handle_duplicate(handle_t handle, handle_t dest, handle_t *_new) {
    if (handle < 0 || handle >= HANDLE_TABLE_SIZE)
        return STATUS_INVALID_HANDLE;

    if (dest == INVALID_HANDLE) {
        if (!_new)
            return STATUS_INVALID_ARG;
    } else if (dest < 0 || dest >= HANDLE_TABLE_SIZE) {
        return STATUS_INVALID_ARG;
    }

    handle_table_t *table = &curr_proc->handles;

    rwlock_write_lock(&table->lock);

    object_handle_t *khandle = table->handles[handle];
    if (!khandle) {
        rwlock_unlock(&table->lock);
        return STATUS_INVALID_HANDLE;
    }

    if (dest != INVALID_HANDLE) {
        /* Close any existing handle in the slot. */
        detach_handle(dest);
    } else {
        /* Try to allocate a new ID. */
        dest = bitmap_ffz(table->bitmap, HANDLE_TABLE_SIZE);
        if (dest < 0) {
            rwlock_unlock(&table->lock);
            return STATUS_NO_HANDLES;
        }
    }

    status_t ret = write_user(_new, dest);
    if (ret != STATUS_SUCCESS) {
        rwlock_unlock(&table->lock);
        return ret;
    }

    if (khandle->type->attach)
        khandle->type->attach(khandle, curr_proc);

    object_handle_retain(khandle);

    table->handles[dest] = khandle;
    table->flags[dest]   = 0;

    bitmap_set(table->bitmap, dest);

    dprintf(
        "object: duplicated handle %" PRId32 " to %" PRId32 " in process %" PRId32 " (type: %u, private: %p)\n",
        handle, dest, curr_proc->id, khandle->type->id, khandle->private);

    rwlock_unlock(&table->lock);
    return STATUS_SUCCESS;
}

/** Closes a handle.
 * @param handle        Handle ID to close.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle does not exist. */
status_t kern_handle_close(handle_t handle) {
    return object_handle_detach(handle, NULL);
}
