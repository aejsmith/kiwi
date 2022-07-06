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
 * @brief               Thread management code.
 */

#pragma once

#include <arch/setjmp.h>
#include <arch/thread.h>

#include <kernel/exception.h>
#include <kernel/thread.h>

#include <lib/avl_tree.h>
#include <lib/list.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <security/token.h>

#include <sync/spinlock.h>

#include <time.h>

struct cpu;
struct frame;
struct process;
struct thread_interrupt;

/** Entry function for a thread. */
typedef void (*thread_func_t)(void *, void *);

/** Definition of a thread. */
typedef struct thread {
    /** Architecture thread implementation. */
    arch_thread_t arch;

    /** State of the thread. */
    enum {
        THREAD_CREATED,                 /**< Newly created, not yet made runnable. */
        THREAD_READY,                   /**< Ready and waiting to be run. */
        THREAD_RUNNING,                 /**< Running on some CPU. */
        THREAD_SLEEPING,                /**< Sleeping, waiting for some event to occur. */
        THREAD_DEAD,                    /**< Dead, waiting to be cleaned up. */
    } state;

    /**
     * This lock protects data in the thread that may be modified by other
     * threads. Some data members are only ever accessed by the thread itself,
     * and therefore it is not necessary to take the lock when accessing these.
     */
    spinlock_t lock;

    /** Main thread information. */
    void *kstack;                       /**< Kernel stack pointer. */
    atomic_uint32_t __flags;            /**< Flags for the thread (use thread_*flags functions). */
    int priority;                       /**< Priority of the thread. */
    size_t wired;                       /**< How many calls to thread_wire() have been made. */
    size_t preempt_count;               /**< Whether preemption is disabled. */

    /** Scheduling information. */
    list_t runq_link;                   /**< Link to run queues. */
    int max_prio;                       /**< Maximum scheduling priority. */
    int curr_prio;                      /**< Current scheduling priority. */
    struct cpu *cpu;                    /**< CPU that the thread runs on. */
    nstime_t timeslice;                 /**< Current timeslice. */

    /** Sleeping information. */
    list_t wait_link;                   /**< Link to a waiting list. */
    timer_t sleep_timer;                /**< Sleep timeout timer. */
    status_t sleep_status;              /**< Sleep status (timed out/interrupted). */
    const char *waiting_on;             /**< What is being waited on (for informational purposes). */

    /** Accounting information. */
    nstime_t last_time;                 /**< Time that the thread entered/left the kernel. */
    nstime_t kernel_time;               /**< Total time the thread has spent in the kernel. */
    nstime_t user_time;                 /**< Total time the thread has spent in user mode. */

    /**
     * Reference count for the thread. A running thread always has at least 1
     * reference on it. Handles and pointers to a thread create an extra
     * reference to it. When the count reaches 0, the thread is destroyed.
     */
    refcount_t count;

    /** User mode interrupt information. */
    uint32_t ipl;                       /**< User mode interrupt priority level. */
    list_t interrupts;                  /**< Pending user mode interrupts. */
    list_t callbacks;                   /**< Event callbacks registered by this thread. */

    /** Exception handler table. */
    exception_handler_t exceptions[EXCEPTION_MAX];
    thread_stack_t exception_stack;     /**< Exception stack. */

    /** Overridden security token for the thread (protected by process lock). */
    token_t *token;

    /**
     * Active token for the thread. When a thread calls token_current(), we
     * save the current token here. Subsequent calls to token_current() return
     * the saved token. The saved token is cleared when the thread returns to
     * userspace. This behaviour means that a thread's identity effectively
     * remains constant for the entire time that it is in the kernel, and won't
     * change if another thread changes the process-wide security token.
     */
    token_t *active_token;

    /** Context to restore upon user memory access fault. */
    jmp_buf usermem_context;

    /** Thread entry function. */
    thread_func_t func;                 /**< Entry function for the thread. */
    void *arg1;                         /**< First argument to thread entry function. */
    void *arg2;                         /**< Second argument to thread entry function. */

    /** Other thread information. */
    ptr_t ustack;                       /**< User stack base. */
    size_t ustack_size;                 /**< Size of the user stack. */
    thread_id_t id;                     /**< ID of the thread. */
    avl_tree_node_t tree_link;          /**< Link to thread tree. */
    char name[THREAD_NAME_MAX];         /**< Name of the thread. */
    notifier_t death_notifier;          /**< Notifier for thread death. */
    int status;                         /**< Exit status of the thread. */
    int reason;                         /**< Exit reason of the thread. */
    struct process *owner;              /**< Pointer to parent process. */
    list_t owner_link;                  /**< Link to parent process. */
} thread_t;

/** Internal flags for a thread. */
enum {
    THREAD_INTERRUPTIBLE    = (1<<0),   /**< Thread is in an interruptible sleep. */
    THREAD_INTERRUPTED      = (1<<1),   /**< Thread has been interrupted. */
    THREAD_KILLED           = (1<<2),   /**< Thread has been killed. */
    THREAD_PREEMPTED        = (1<<3),   /**< Thread was preempted while preemption disabled. */
    THREAD_IN_USERMEM       = (1<<4),   /**< Thread is in a safe user memory access function. */
};

/**
 * Function called after a thread interrupt has been set up. This can be used
 * for some deferred cleanup work (see e.g. object_event_signal()). If not
 * NULL, this function is responsible for making sure the structure is freed,
 * otherwise structure will be freed with kfree().
 *
 * This is executed during return to user mode and therefore is not considered
 * to be in interrupt context.
 */
typedef void (*thread_post_interrupt_cb_t)(struct thread_interrupt *interrupt);

/** User mode thread interrupt structure. */
typedef struct thread_interrupt {
    list_t header;                      /**< Link to interrupt list. */
    uint32_t priority;                  /**< Interrupt priority. */
    thread_post_interrupt_cb_t post_cb; /**< Post-setup callback. */
    void *cb_data;                      /**< Argument for callback. */

    /**
     * Address of the user-mode interrupt handler function. The function will
     * be called with a pointer to the interrupt data as its first argument,
     * and a pointer to the saved thread state as its second argument.
     */
    ptr_t handler;

    /** Alternate stack to use (if base is NULL will not switch stack). */
    thread_stack_t stack;

    /**
     * Size of the interrupt data to pass to the handler, which should
     * immediately follow this structure. The data will be copied onto the
     * thread's user stack and the handler will receive a pointer to it. For
     * this reason, users of this must exercise caution to ensure that kernel
     * memory is not accidentally leaked to user mode e.g. through uninitialized
     * padding in structures.
     */
    size_t size;
} thread_interrupt_t;

/** Sleeping behaviour flags. */
enum {
    SLEEP_INTERRUPTIBLE     = (1<<0),   /**< Sleep should be interruptible. */
    SLEEP_ABSOLUTE          = (1<<1),   /**< Specified timeout is absolute, not relative to current time. */

    /**
     * Don't relock the specified lock upon return(). Do not use this unless
     * calling thread_sleep() directly.
     */
    __SLEEP_NO_RELOCK       = (1<<2),
};

/** Macro that expands to a pointer to the current thread. */
#define curr_thread             (arch_curr_thread())

/** Atomically adds the given flag(s) to the thread's flags.
 * @param thread        Thread to set for.
 * @param flags         Flag(s) to set.
 * @return              Previous thread flags. */
static inline uint32_t thread_set_flag(thread_t *thread, uint32_t flags) {
    return atomic_fetch_or(&thread->__flags, flags);
}

/** Atomically clears the given flag(s) from the thread's flags.
 * @param thread        Thread to set for.
 * @param flags         Flag(s) to clear.
 * @return              Previous thread flags.  */
static inline uint32_t thread_clear_flag(thread_t *thread, uint32_t flags) {
    return atomic_fetch_and(&thread->__flags, ~flags);
}

/** Gets a thread's flags.
 * @param thread        Thread to get from.
 * @return              Thread flags. */
static inline uint32_t thread_flags(thread_t *thread) {
    return atomic_load(&thread->__flags);
}

extern void arch_thread_init(thread_t *thread);
extern void arch_thread_destroy(thread_t *thread);
extern void arch_thread_clone(thread_t *thread, struct frame *frame);
extern void arch_thread_switch(thread_t *thread, thread_t *prev);
extern void arch_thread_set_tls_addr(ptr_t addr);
extern void arch_thread_user_setup(struct frame *frame, ptr_t entry, ptr_t sp, ptr_t arg);
extern void arch_thread_user_enter(struct frame *frame) __noreturn;
extern status_t arch_thread_interrupt_setup(thread_interrupt_t *interrupt, uint32_t ipl);
extern status_t arch_thread_interrupt_restore(uint32_t *_ipl);
extern void arch_thread_backtrace(void (*cb)(ptr_t));

extern void thread_trampoline(void);

extern void thread_retain(thread_t *thread);
extern void thread_release(thread_t *thread);

extern void thread_rename(thread_t *thread, const char *name);
extern void thread_wire(thread_t *thread);
extern void thread_unwire(thread_t *thread);
extern bool thread_wake(thread_t *thread);
extern void thread_kill(thread_t *thread);
extern void thread_interrupt(thread_t *thread, thread_interrupt_t *interrupt);

extern status_t thread_sleep(spinlock_t *lock, nstime_t timeout, const char *name, uint32_t flags);
extern void thread_yield(void);
extern void thread_at_kernel_entry(bool interrupt);
extern void thread_at_kernel_exit(void);
extern void thread_exception(exception_info_t *info);
extern void thread_exit(void) __noreturn;

extern thread_t *thread_lookup_unsafe(thread_id_t id);
extern thread_t *thread_lookup(thread_id_t id);

extern status_t thread_create(
    const char *name, struct process *owner, uint32_t flags, thread_func_t func,
    void *arg1, void *arg2, thread_t **_thread);
extern void thread_run(thread_t *thread);

extern void thread_init(void);
