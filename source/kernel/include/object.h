/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel object manager.
 */

#pragma once

#include <kernel/object.h>

#include <lib/list.h>
#include <lib/refcount.h>

#include <sync/rwlock.h>

struct object_handle;
struct process;
struct thread;
struct vm_region;

/** Kernel object type structure. */
typedef struct object_type {
    uint32_t id;                        /**< ID number for the type. */
    uint32_t flags;                     /**< Flags for objects of this type. */

    /** Close a handle to an object.
     * @param handle        Handle to the object. */
    void (*close)(struct object_handle *handle);

    /** Get the name of an object.
     * @param handle        Handle to the object.
     * @return              Pointer to allocated name string, or NULL if not
     *                      available. */
    char *(*name)(struct object_handle *handle);

    /**
     * Get the name of an object in KDB context. This should not take any locks
     * or allocate any memory. The name should be built in place in the given
     * buffer.
     *
     * @param handle        Handle to the object.
     * @param buf           Buffer to write into.
     * @param size          Size of the buffer.
     *
     * @return              Pointer to start of name string, or NULL if not
     *                      available.
     */
    char *(*name_unsafe)(struct object_handle *handle, char *buf, size_t size);

    /** Called when a handle is attached to a process.
     * @param handle        Handle to the object.
     * @param process       Process the handle is being attached to. */
    void (*attach)(struct object_handle *handle, struct process *process);

    /** Called when a handle is detached from a process.
     * @param handle        Handle to the object.
     * @param process       Process the handle is being detached from. */
    void (*detach)(struct object_handle *handle, struct process *process);

    /**
     * Start waiting for an object event.
     *
     * This function is called when a thread starts waiting for an event on an
     * object. It should check that the specified event is valid, and then
     * arrange for object_event_signal() to be called when the event occurs. If
     * waiting in level-triggered mode and the event being waited for has
     * occurred already, this function should call object_event_signal()
     * immediately. Do NOT call it for edge-triggered mode.
     *
     * @param handle        Handle to object.
     * @param event         Event that is being waited for.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*wait)(struct object_handle *handle, object_event_t *event);

    /**
     * Stop waiting for an object event.
     *
     * Stop a wait previously set up with wait(). Note that this function may
     * be called from object_event_signal() so be careful with regard to
     * locking. If using notifiers, these handle recursive locking properly.
     *
     * @param handle        Handle to object.
     * @param event         Event that is being waited for.
     */
    void (*unwait)(struct object_handle *handle, object_event_t *event);

    /**
     * Map an object into memory.
     *
     * This function is called when an object is to be mapped into memory. It
     * should check whether the current thread has permission to perform the
     * mapping with the access flags set in the region. It should then either
     * map the entire region up front, or set the region's operations structure
     * pointer to allow the region content to be demand paged.
     *
     * @param handle        Handle to object.
     * @param region        Region being mapped.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*map)(struct object_handle *handle, struct vm_region *region);
} object_type_t;

/** Properties of an object type. */
#define OBJECT_TRANSFERRABLE    (1<<0)  /**< Objects can be inherited or transferred over IPC. */

/** Structure containing a kernel object handle. */
typedef struct object_handle {
    const object_type_t *type;          /**< Type of the object. */
    void *private;                      /**< Per-handle data pointer. */
    refcount_t count;                   /**< References to the handle. */
} object_handle_t;

/** Table that maps IDs to handles (handle_t -> object_handle_t). */
typedef struct handle_table {
    rwlock_t lock;                      /**< Lock to protect table. */
    object_handle_t **handles;          /**< Array of allocated handles. */
    uint32_t *flags;                    /**< Array of entry flags. */
    list_t *callbacks;                  /**< Array of callback lists for each entry. */
    unsigned long *bitmap;              /**< Bitmap for tracking free handle IDs. */
} handle_table_t;

extern object_handle_t *object_handle_create(const object_type_t *type, void *private);
extern void object_handle_retain(object_handle_t *handle);
extern void object_handle_release(object_handle_t *handle);

/** Helper for __cleanup_object_handle. */
static inline void __object_handle_releasep(void *p) {
    object_handle_release(*(object_handle_t **)p);
}

/** Attribute to release a handle when it goes out of scope. */
#define __cleanup_object_handle __cleanup(__object_handle_releasep)

extern status_t object_handle_lookup(handle_t id, int type, object_handle_t **_handle);
extern status_t object_handle_attach(object_handle_t *handle, handle_t *_id, handle_t *_uid);
extern status_t object_handle_detach(handle_t id, handle_t *_uid);
extern status_t object_handle_open(const object_type_t *type, void *private, handle_t *_id, handle_t *_uid);

extern void object_process_init(struct process *process);
extern void object_process_cleanup(struct process *process);
extern status_t object_process_create(
    struct process *process, struct process *parent, handle_t map[][2],
    ssize_t count);
extern status_t object_process_exec(handle_t map[][2], ssize_t count);
extern void object_process_clone(struct process *process, struct process *parent);

extern void object_thread_cleanup(struct thread *thread);

extern void object_event_signal_etc(object_event_t *event, uint64_t data, status_t status);
extern void object_event_signal(object_event_t *event, uint64_t data);

extern void object_event_notifier(void *arg1, void *arg2, void *arg3);

extern void object_init(void);
