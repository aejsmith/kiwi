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

#include <cpu/cpu.h>

#include <sync/spinlock.h>

#include <types/list.h>

#include <context.h>

/** Maximum length of a thread name. */
#define THREAD_NAME_MAX		32

struct process;
struct wait_queue;

/** Entry function for a thread. */
typedef void (*thread_func_t)(void *);

/** Definition of a thread. */
typedef struct thread {
	list_t header;			/**< Link to run queues. */

	/** Main thread information. */
	spinlock_t lock;		/**< Protects the thread's internals. */
	context_t context;		/**< CPU context. */
	unative_t *kstack;		/**< Kernel stack pointer. */
	int flags;			/**< Flags for the thread. */
	cpu_t *cpu;			/**< CPU that the thread runs on. */

	/** Scheduling information. */
	size_t priority;		/**< Current scheduling priority. */
	uint32_t timeslice;		/**< Current timeslice. */
	int preempt_off;		/**< Whether preemption is disabled. */
	bool preempt_missed;		/**< Whether preemption was missed due to being disabled. */

	/** Sleeping information. */
	list_t waitq_link;		/**< Link to wait queue. */
	struct wait_queue *waitq;	/**< Wait queue that the thread is sleeping on. */
	bool interruptible;		/**< Whether the sleep can be interrupted. */
	context_t sleep_context;	/**< Context to restore upon sleep interruption. */

	/** State of the thread. */
	enum {
		THREAD_CREATED,		/**< Thread is newly created. */
		THREAD_READY,		/**< Thread is runnable. */
		THREAD_RUNNING,		/**< Thread is running on a CPU. */
		THREAD_SLEEPING,	/**< Thread is sleeping. */
		THREAD_DEAD,		/**< Thread is dead and awaiting cleanup. */
	} state;

	/** Thread entry function. */
	thread_func_t entry;		/**< Entry function for the thread. */
	void *arg;			/**< Argument to thread entry function. */

	/** Other thread information. */
	thread_id_t id;			/**< ID of the thread. */
	char name[THREAD_NAME_MAX];	/**< Name of the thread. */
	struct process *owner;		/**< Pointer to parent process. */
	list_t owner_link;		/**< Link to parent process. */
} thread_t;

/** Thread flag definitions. */
#define THREAD_UNMOVABLE	(1<<0)	/**< Thread is tied to the CPU it is running on. */
#define THREAD_UNQUEUEABLE	(1<<1)	/**< Thread cannot be queued in the run queue. */
#define THREAD_UNPREEMPTABLE	(1<<2)	/**< Thread will not be preempted. */

/** Macro that expands to a pointer to the current thread. */
#define curr_thread		(curr_cpu->thread)

extern thread_t *thread_lookup(thread_id_t id);
extern void thread_run(thread_t *thread);
extern int thread_create(const char *name, struct process *owner, int flags, thread_func_t entry, void *arg, thread_t **threadp);
extern void thread_destroy(thread_t *thread);
extern void thread_exit(void);

extern void thread_init(void);

extern int kdbg_cmd_thread(int argc, char **argv);

#endif /* __PROC_THREAD_H */
