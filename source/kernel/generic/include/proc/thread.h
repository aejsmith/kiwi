/* Kiwi thread management code
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Thread management code.
 */

#ifndef __PROC_THREAD_H
#define __PROC_THREAD_H

#include <arch/thread.h>

#include <cpu/context.h>
#include <cpu/cpu.h>
#include <cpu/fpu.h>

#include <sync/spinlock.h>

#include <time/timer.h>

#include <types/list.h>
#include <types/refcount.h>

/** Maximum length of a thread name. */
#define THREAD_NAME_MAX		32

struct process;
struct waitq;

/** Entry function for a thread. */
typedef void (*thread_func_t)(void *, void *);

/** Definition of a thread. */
typedef struct thread {
	list_t header;			/**< Link to run queues. */

	/** Main thread information. */
	spinlock_t lock;		/**< Protects the thread's internals. */
	context_t context;		/**< CPU context. */
	fpu_context_t *fpu;		/**< FPU context. */
	thread_arch_t arch;		/**< Architecture thread data. */
	unative_t *kstack;		/**< Kernel stack pointer. */
	int flags;			/**< Flags for the thread. */
	cpu_t *cpu;			/**< CPU that the thread runs on. */
	int wire_count;			/**< How many calls to thread_wire() have been made. */
	refcount_t count;		/**< Number of handles to the thread. */
	bool killed;			/**< Whether thread_kill() has been called on the thread. */

	/** Scheduling information. */
	size_t priority;		/**< Current scheduling priority. */
	uint32_t timeslice;		/**< Current timeslice. */
	int preempt_off;		/**< Whether preemption is disabled. */
	bool preempt_missed;		/**< Whether preemption was missed due to being disabled. */

	/** Sleeping information. */
	list_t waitq_link;		/**< Link to wait queue. */
	struct waitq *waitq;		/**< Wait queue that the thread is sleeping on. */
	bool interruptible;		/**< Whether the sleep can be interrupted. */
	context_t sleep_context;	/**< Context to restore upon sleep interruption/timeout. */
	timer_t sleep_timer;		/**< Timer for sleep timeout. */
	bool timed_out;			/**< Whether the sleep timed out. */
	bool rwlock_writer;		/**< Whether the thread wants exclusive access to an rwlock. */

	/** State of the thread. */
	enum {
		THREAD_CREATED,		/**< Thread is newly created. */
		THREAD_READY,		/**< Thread is runnable. */
		THREAD_RUNNING,		/**< Thread is running on a CPU. */
		THREAD_SLEEPING,	/**< Thread is sleeping. */
		THREAD_DEAD,		/**< Thread is dead and awaiting cleanup. */
	} state;

	/** Information used by user memory functions. */
	atomic_t in_usermem;		/**< Whether the thread is in the user memory access functions. */
	context_t usermem_context;	/**< Context to restore upon user memory access fault. */

	/** Thread entry function. */
	thread_func_t entry;		/**< Entry function for the thread. */
	void *arg1;			/**< First argument to thread entry function. */
	void *arg2;			/**< Second argument to thread entry function. */

	/** Other thread information. */
	identifier_t id;		/**< ID of the thread. */
	char name[THREAD_NAME_MAX];	/**< Name of the thread. */
	struct process *owner;		/**< Pointer to parent process. */
	list_t owner_link;		/**< Link to parent process. */
} thread_t;

/** Thread flag definitions. */
#define THREAD_UNQUEUEABLE	(1<<1)	/**< Thread cannot be queued in the run queue. */
#define THREAD_UNPREEMPTABLE	(1<<2)	/**< Thread will not be preempted. */

/** Macro that expands to a pointer to the current thread. */
#define curr_thread		(curr_cpu->thread)

extern void thread_run(thread_t *thread);
extern void thread_wire(thread_t *thread);
extern void thread_unwire(thread_t *thread);
extern bool thread_interrupt(thread_t *thread);
extern void thread_kill(thread_t *thread);
extern void thread_exit(void) __noreturn;

extern void thread_rename(thread_t *thread, const char *name);

extern thread_t *thread_lookup(identifier_t id);
extern int thread_create(const char *name, struct process *owner, int flags,
                         thread_func_t entry, void *arg1, void *arg2, thread_t **threadp);
extern void thread_destroy(thread_t *thread);

extern int kdbg_cmd_thread(int argc, char **argv);

extern void thread_init(void);
extern void thread_reaper_init(void);

extern handle_t sys_thread_create(const char *name, void *stack, size_t stacksz, void (*func)(void *), void *arg1);
extern handle_t sys_thread_open(identifier_t id);
extern identifier_t sys_thread_id(handle_t handle);
extern void sys_thread_exit(int status);

#endif /* __PROC_THREAD_H */
