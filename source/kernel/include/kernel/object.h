/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel object management.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

struct thread_context;

/**
 * Value used to refer to an invalid handle. This is used to mean various
 * things, for example with thread/process functions it refers to the current
 * thread/process rather than one referred to by a handle.
 */
#define INVALID_HANDLE          (-1)

/** Object type ID definitions. */
enum {
    OBJECT_TYPE_PROCESS         = 1,        /**< Process (transferrable). */
    OBJECT_TYPE_THREAD          = 2,        /**< Thread (transferrable). */
    OBJECT_TYPE_TOKEN           = 3,        /**< Security Token (transferrable). */
    OBJECT_TYPE_TIMER           = 4,        /**< Timer (transferrable). */
    OBJECT_TYPE_WATCHER         = 5,        /**< Watcher (non-transferrable). */
    OBJECT_TYPE_AREA            = 6,        /**< Memory Area (transferrable). */
    OBJECT_TYPE_FILE            = 7,        /**< File (transferrable). */
    OBJECT_TYPE_PORT            = 8,        /**< Port (transferrable). */
    OBJECT_TYPE_CONNECTION      = 9,        /**< Connection (non-transferrable). */
    OBJECT_TYPE_SEMAPHORE       = 10,       /**< Semaphore (transferrable). */
    OBJECT_TYPE_PROCESS_GROUP   = 11,       /**< Process Group (non-transferrable). */
    OBJECT_TYPE_CONDITION       = 12,       /**< Condition (transferrable). */
};

/** Flags for handle table entries. */
enum {
    HANDLE_INHERITABLE          = (1<<0),   /**< Handle will be inherited by child processes. */
};

/** Details of an object event to wait for. */
typedef struct object_event {
    handle_t handle;                        /**< Handle to wait on. */
    uint32_t event;                         /**< Event to wait for. */
    uint32_t flags;                         /**< Flags for the event. */
    uint64_t data;                          /**< Integer data associated with the event. */
    void *udata;                            /**< User data, passed through unmodified. */
} object_event_t;

/** Object event flags. */
enum {
    OBJECT_EVENT_ERROR          = (1<<0),   /**< Set if an error occurred in this event. */
    OBJECT_EVENT_SIGNALLED      = (1<<1),   /**< Set if this event is signalled. */
    OBJECT_EVENT_ONESHOT        = (1<<2),   /**< Remove callback after firing the first time. */
    OBJECT_EVENT_EDGE           = (1<<3),   /**< Event should be edge triggered rather than level. */
};

/** Behaviour flags for kern_object_wait(). */
enum {
    OBJECT_WAIT_ALL             = (1<<0),   /**< Wait for all the specified events to occur. */
};

/** Modes for kern_handle_duplicate(). */
enum {
    /**
     * Allocate a new handle ID. If dest is INVALID_HANDLE, any available ID
     * will be used. Otherwise, the allocated handle ID will be the first
     * available ID greater or equal to the ID specified in dest.
     */
    HANDLE_DUPLICATE_ALLOCATE   = 0,

    /**
     * Use the exact ID specified in dest, closing any existing handle with
     * that ID. In this case, _new can be NULL.
     */
    HANDLE_DUPLICATE_EXACT      = 1,
};

/**
 * Type of an object event callback function. The function will be called via
 * a thread interrupt when the event that is registered for occurs. While the
 * function is executing, the thread's IPL will be raised to 1 above the
 * priority the callback was registered with, thus blocking further interrupts
 * while it is executing. When the function returns the IPL will be restored.
 *
 * @param event     Event structure.
 * @param ctx       Thread context before the function was called.
 */
typedef void (*object_callback_t)(object_event_t *event, struct thread_context *ctx);

extern status_t kern_object_type(handle_t handle, uint32_t *_type);
extern status_t kern_object_wait(object_event_t *events, size_t count, uint32_t flags, nstime_t timeout);
extern status_t kern_object_callback(object_event_t *event, object_callback_t callback, uint32_t priority);

extern status_t kern_handle_flags(handle_t handle, uint32_t *_flags);
extern status_t kern_handle_set_flags(handle_t handle, uint32_t flags);
extern status_t kern_handle_duplicate(uint32_t mode, handle_t handle, handle_t dest, handle_t *_new);
extern status_t kern_handle_close(handle_t handle);

__KERNEL_EXTERN_C_END
