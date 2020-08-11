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
 * @brief               Process management functions.
 */

#ifndef __PROC_PROCESS_H
#define __PROC_PROCESS_H

#include <io/context.h>

#include <kernel/process.h>

#include <lib/avl_tree.h>
#include <lib/notifier.h>

#include <proc/thread.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <object.h>

struct ipc_port;
struct process_load;
struct vm_aspace;

/** Structure containing details about a process. */
typedef struct process {
    /** Lock to protect the process data. */
    mutex_t lock;

    /**
     * Reference count.
     *
     * This reference count counts the number of handles and pointers to the
     * process, as well as the number of threads attached (in any state).
     */
    refcount_t count;

    /**
     * Running thread count.
     *
     * This counts the number of threads running in the process. It is used to
     * determine when the process has died and when we can free up its
     * resources: a process can potentially stay alive a long time after it has
     * died if a handle is open to it, but we should destroy its address space,
     * etc., as soon as the last thread dies.
     */
    refcount_t running;

    /** Scheduling information. */
    unsigned flags;                     /**< Behaviour flags for the process. */
    int priority;                       /**< Priority class of the process. */

    /** Resource information. */
    token_t *token;                     /**< Security token for the process. */
    struct vm_aspace *aspace;           /**< Process' address space. */
    handle_table_t handles;             /**< Table of open handles. */
    io_context_t io;                    /**< I/O context. */
    list_t threads;                     /**< List of threads. */
    avl_tree_t futexes;                 /**< Tree of futexes that the process has accessed. */
    list_t images;                      /**< List of loaded images. */
    ptr_t thread_restore;               /**< Address of kern_thread_restore() in libkernel. */

    /** Exception handler table. */
    exception_handler_t exceptions[EXCEPTION_MAX];

    /** Special ports. */
    struct ipc_port *root_port;         /**< Root port. */

    /** State of the process. */
    enum {
        PROCESS_CREATED,                /**< Created. */
        PROCESS_RUNNING,                /**< Running. */
        PROCESS_DEAD,                   /**< Dead. */
    } state;

    /** Other process information. */
    avl_tree_node_t tree_link;          /**< Link to process tree. */
    process_id_t id;                    /**< ID of the process. */
    char *name;                         /**< Name of the process. */
    notifier_t death_notifier;          /**< Notifier for process death. */
    int status;                         /**< Exit status. */
    int reason;                         /**< Exit reason. */
    struct process_load *load;          /**< Internal program loading information. */
} process_t;

/** Process flag definitions. */
#define PROCESS_CRITICAL        (1<<0)  /**< Process is critical to system operation, cannot die. */

/** Internal priority classes. */
#define PRIORITY_CLASS_SYSTEM   3       /**< Used for the kernel process. */
#define PRIORITY_CLASS_MAX      3

/** Macro that expands to a pointer to the current process. */
#define curr_proc               (curr_thread->owner)

extern process_t *kernel_proc;

extern void process_retain(process_t *process);
extern void process_release(process_t *process);
extern void process_attach_thread(process_t *process, thread_t *thread);
extern void process_thread_started(thread_t *thread);
extern void process_thread_exited(thread_t *thread);
extern void process_detach_thread(thread_t *thread);

extern bool process_access_unsafe(process_t *process);
extern bool process_access(process_t *process);

extern void process_exit(void) __noreturn;

extern process_t *process_lookup_unsafe(process_id_t id);
extern process_t *process_lookup(process_id_t id);

extern status_t process_create(
    const char *const args[], const char *const env[], uint32_t flags,
    int priority, process_t **_process);

extern void process_init(void);
extern void process_shutdown(void);

#endif /* __PROC_PROCESS_H */
