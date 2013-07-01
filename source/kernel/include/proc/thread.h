/*
 * Copyright (C) 2008-2012 Alex Smith
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
 * @brief		Thread management code.
 */

#ifndef __PROC_THREAD_H
#define __PROC_THREAD_H

#include <arch/setjmp.h>
#include <arch/thread.h>

#include <kernel/signal.h>
#include <kernel/thread.h>

#include <lib/avl_tree.h>
#include <lib/list.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <sync/spinlock.h>
#include <sync/sync.h>

#include <cpu.h>
#include <object.h>
#include <time.h>

struct process;

/** Entry function for a thread. */
typedef void (*thread_func_t)(void *, void *);

/** Thread creation arguments structure, for thread_uspace_trampoline(). */
typedef struct thread_uspace_args {
	ptr_t sp;			/**< Stack pointer. */
	ptr_t entry;			/**< Entry point address. */
	ptr_t arg;			/**< Argument. */
} thread_uspace_args_t;

/** Definition of a thread. */
typedef struct thread {
	object_t obj;			/**< Object header. */

	/** Architecture thread implementation. */
	arch_thread_t arch;

	/** State of the thread. */
	enum {
		THREAD_CREATED,		/**< Newly created, not yet made runnable. */
		THREAD_READY,		/**< Ready and waiting to be run. */
		THREAD_RUNNING,		/**< Running on some CPU. */
		THREAD_SLEEPING,	/**< Sleeping, waiting for some event to occur. */
		THREAD_DEAD,		/**< Dead, waiting to be cleaned up. */
	} state;

	/**
	 * Lock for the thread.
	 *
	 * This lock protects data in the thread that may be modified by
	 * other threads. Some data members are only ever accessed by the
	 * thread itself, and therefore it is not necessary to take the lock
	 * when accessing these.
	 */
	spinlock_t lock;

	/** Main thread information. */
	void *kstack;			/**< Kernel stack pointer. */
	unsigned flags;			/**< Flags for the thread. */
	int priority;			/**< Priority of the thread. */
	size_t wired;			/**< How many calls to thread_wire() have been made. */
	size_t preempt_disabled;	/**< Whether preemption is disabled. */
	bool missed_preempt;		/**< Whether preemption was missed due to being disabled. */

	/** Scheduling information. */
	list_t runq_link;		/**< Link to run queues. */
	int max_prio;			/**< Maximum scheduling priority. */
	int curr_prio;			/**< Current scheduling priority. */
	cpu_t *cpu;			/**< CPU that the thread runs on. */
	useconds_t timeslice;		/**< Current timeslice. */

	/** Sleeping information. */
	list_t wait_link;		/**< Link to a waiting list. */
	timer_t sleep_timer;		/**< Sleep timeout timer. */
	status_t sleep_status;		/**< Sleep status (timed out/interrupted). */
	spinlock_t *wait_lock;		/**< Lock for the waiting list. */
	const char *waiting_on;		/**< What is being waited on (for informational purposes). */

	/** Accounting information. */
	useconds_t last_time;		/**< Time that the thread entered/left the kernel. */
	useconds_t kernel_time;		/**< Total time the thread has spent in the kernel. */
	useconds_t user_time;		/**< Total time the thread has spent in user mode. */

	/** Information used by user memory functions. */
	bool in_usermem;		/**< Whether the thread is in the user memory access functions. */
	jmp_buf usermem_context;	/**< Context to restore upon user memory access fault. */

	/**
	 * Reference count for the thread.
	 *
	 * A running thread always has at least 1 reference on it. Handles and
	 * pointers to a thread create an extra reference to it. When the
	 * count reaches 0, the thread is destroyed.
	 */
	refcount_t count;

	/** Signal information. */
	sigset_t signal_mask;		/**< Signal mask for the thread. */
	sigset_t pending_signals;	/**< Bitmap of pending signals. */
	siginfo_t signal_info[NSIG];	/**< Information associated with pending signals. */
	stack_t signal_stack;		/**< Alternate signal stack. */

	/** Thread entry function. */
	thread_func_t func;		/**< Entry function for the thread. */
	void *arg1;			/**< First argument to thread entry function. */
	void *arg2;			/**< Second argument to thread entry function. */

	/** Other thread information. */
	ptr_t ustack;			/**< User-mode stack base. */
	size_t ustack_size;		/**< Size of the user-mode stack. */
	thread_id_t id;			/**< ID of the thread. */
	avl_tree_node_t tree_link;	/**< Link to thread tree. */
	char name[THREAD_NAME_MAX];	/**< Name of the thread. */
	notifier_t death_notifier;	/**< Notifier for thread death. */
	int status;			/**< Exit status of the thread. */
	struct process *owner;		/**< Pointer to parent process. */
	list_t owner_link;		/**< Link to parent process. */
} thread_t;

/** Internal flags for a thread (do not set these). */
#define THREAD_INTERRUPTIBLE	(1<<0)	/**< Thread is in an interruptible sleep. */
#define THREAD_INTERRUPTED	(1<<1)	/**< Thread has been interrupted. */
#define THREAD_KILLED		(1<<2)	/**< Thread has been killed. */
#define THREAD_RWLOCK_WRITER	(1<<3)	/**< Thread is blocked on an rwlock for writing. */

/** Macro that expands to a pointer to the current thread. */
#define curr_thread		(curr_cpu->thread)

extern void arch_thread_init(thread_t *thread, void *stack, void (*entry)(void));
extern void arch_thread_destroy(thread_t *thread);
extern void arch_thread_switch(thread_t *thread, thread_t *prev);
extern ptr_t arch_thread_tls_addr(thread_t *thread);
extern status_t arch_thread_set_tls_addr(thread_t *thread, ptr_t addr);
extern void arch_thread_enter_userspace(ptr_t entry, ptr_t stack, ptr_t arg) __noreturn;

extern void thread_uspace_trampoline(void *_args, void *arg2);

extern void thread_retain(thread_t *thread);
extern void thread_release(thread_t *thread);
extern void thread_wire(thread_t *thread);
extern void thread_unwire(thread_t *thread);
extern void thread_wake(thread_t *thread);
extern bool thread_interrupt(thread_t *thread);
extern void thread_kill(thread_t *thread);
extern void thread_rename(thread_t *thread, const char *name);

extern void thread_preempt(void);
extern void thread_disable_preempt(void);
extern void thread_enable_preempt(void);
extern status_t thread_sleep(spinlock_t *lock, useconds_t timeout, const char *name, int flags);
extern void thread_yield(void);
extern void thread_at_kernel_entry(void);
extern void thread_at_kernel_exit(void);
extern void thread_exit(void) __noreturn;

extern thread_t *thread_lookup_unsafe(thread_id_t id);
extern thread_t *thread_lookup(thread_id_t id);
extern status_t thread_create(const char *name, struct process *owner, unsigned flags,
	thread_func_t func, void *arg1, void *arg2, object_security_t *security,
	thread_t **threadp);
extern void thread_run(thread_t *thread);

extern void thread_init(void);

#endif /* __PROC_THREAD_H */
