/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Process management functions.
 */

#ifndef __PROC_PROCESS_H
#define __PROC_PROCESS_H

#include <io/context.h>

#include <kernel/process.h>
#include <kernel/signal.h>

#include <lib/avl_tree.h>
#include <lib/notifier.h>

#include <proc/thread.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <object.h>

struct vm_aspace;
struct process_create;

/** Structure containing details about a process. */
typedef struct process {
	object_t obj;			/**< Kernel object header. */

	/** Main thread information. */
	mutex_t lock;			/**< Lock to protect data in structure. */
	refcount_t count;		/**< Number of handles to/threads in the process. */

	/** Scheduling information. */
	int flags;			/**< Behaviour flags for the process. */
	int priority;			/**< Priority class of the process. */
	struct vm_aspace *aspace;	/**< Process' address space. */

	/** Resource information. */
	handle_table_t *handles;	/**< Table of open handles. */
	io_context_t ioctx;		/**< I/O context structure. */
	list_t threads;			/**< List of threads. */
	avl_tree_t futexes;		/**< Tree of futexes that the process has accessed. */

	/** Security information. */
	mutex_t security_lock;		/**< Lock for security context. */
	security_context_t security;	/**< Security context (canonicalised). */

	/** Signal information. */
	sigset_t signal_mask;		/**< Bitmap of masked signals. */
	sigaction_t signal_act[NSIG];	/**< Signal action structures. */

	/** State of the process. */
	enum {
		PROCESS_RUNNING,	/**< Running. */
		PROCESS_DEAD,		/**< Dead. */
	} state;

	/** Other process information. */
	avl_tree_node_t tree_link;	/**< Link to process tree. */
	process_id_t id;		/**< ID of the process. */
	char *name;			/**< Name of the process. */
	notifier_t death_notifier;	/**< Notifier for process death. */
	int status;			/**< Exit status. */
	int reason;			/**< Exit reason. */
	struct process_create *create;	/**< Internal creation information structure. */
} process_t;

/** Process flag definitions. */
#define PROCESS_CRITICAL	(1<<0)	/**< Process is critical to system operation, cannot die. */

/** Internal priority classes. */
#define PRIORITY_CLASS_SYSTEM	3	/**< Used for the kernel process. */
#define PRIORITY_CLASS_MAX	3

/** Macro that expands to a pointer to the current process. */
#define curr_proc		(curr_thread->owner)

extern process_t *kernel_proc;

extern void process_attach(process_t *process, thread_t *thread);
extern void process_detach(thread_t *thread);

extern process_t *process_lookup_unsafe(process_id_t id);
extern process_t *process_lookup(process_id_t id);
extern status_t process_create(const char *const args[], const char *const env[],
	int flags, int priority, process_t *parent, process_t **procp);
extern void process_exit(int status, int reason) __noreturn;

extern void process_init(void);
extern void process_shutdown(void);

#endif /* __PROC_PROCESS_H */
