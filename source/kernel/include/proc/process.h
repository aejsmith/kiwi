/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Process management functions.
 */

#ifndef __PROC_PROCESS_H
#define __PROC_PROCESS_H

#include <io/context.h>

#include <lib/notifier.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <public/process.h>

#include <sync/spinlock.h>

#include <object.h>

struct vm_aspace;

/** Structure containing details about a process. */
typedef struct process {
	object_t obj;			/**< Kernel object header. */

	mutex_t lock;			/**< Lock to protect data in structure. */
	process_id_t id;		/**< ID of the process. */
	char *name;			/**< Name of the process. */
	int flags;			/**< Behaviour flags for the process. */
	size_t priority;		/**< Priority of the process. */
	refcount_t count;		/**< Number of handles open to and threads in the process. */
	int status;			/**< Exit status of the process. */

	/** State of the process. */
	enum {
		PROCESS_RUNNING,	/**< Running. */
		PROCESS_DEAD,		/**< Dead. */
	} state;

	struct vm_aspace *aspace;	/**< Process' address space. */
	list_t threads;			/**< List of threads. */
	handle_table_t handles;		/**< Table of open handles. */
	io_context_t ioctx;		/**< I/O context structure. */

	notifier_t death_notifier;	/**< Notifier for process death (do NOT add to when already dead). */
} process_t;

/** Process flag definitions. */
#define PROCESS_CRITICAL	(1<<0)	/**< Process is critical to system operation, cannot die. */
#define PROCESS_FIXEDPRIO	(1<<1)	/**< Process' priority is fixed and should not be changed. */

/** Macro that expands to a pointer to the current process. */
#define curr_proc		(curr_thread->owner)

extern process_t *kernel_proc;

extern void process_attach(process_t *process, thread_t *thread);
extern void process_detach(thread_t *thread);

extern process_t *process_lookup_unsafe(process_id_t id);
extern process_t *process_lookup(process_id_t id);
extern int process_create(const char **args, const char **environ, int flags, int cflags,
                          int priority, process_t *parent, process_t **procp);
extern void process_exit(int status) __noreturn;

extern int kdbg_cmd_process(int argc, char **argv);

extern void process_init(void);

#endif /* __PROC_PROCESS_H */
