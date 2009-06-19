/* Kiwi process management
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
 * @brief		Process management functions.
 */

#ifndef __PROC_PROCESS_H
#define __PROC_PROCESS_H

#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/spinlock.h>

#include <types/list.h>

#include <types.h>

struct aspace;
struct subsystem;

/** Maximum length of a process name. */
#define PROC_NAME_MAX		32

/** Structure containing details about a process. */
typedef struct process {
	spinlock_t lock;		/**< Lock to protect data in structure. */
	struct aspace *aspace;		/**< Process' address space. */
	int flags;			/**< Behaviour flags for the process. */
	size_t priority;		/**< Priority of the process. */

	/** Subsystem information. */
	struct subsystem *subsystem;	/**< Subsystem that the process is running under. */
	void *data;			/**< Subsystem data pointer. */

	/** Thread information. */
	list_t threads;			/**< List of threads. */
	size_t num_threads;		/**< Number of threads. */

	/** State of the process. */
	enum {
		PROC_RUNNING,		/**< Process is running. */
	} state;

	/** Other information about the process. */
	process_id_t id;		/**< ID of the process. */
	char name[PROC_NAME_MAX];	/**< Name of the process. */
	struct process *parent;		/**< Pointer to parent process. */
	list_t children;		/**< List of child processes. */
	list_t parent_link;		/**< Link to parent process child list. */
} process_t;

/** Process flag definitions */
#define PROCESS_CRITICAL	(1<<0)	/**< Process is critical to system operation, cannot die. */
#define PROCESS_FIXEDPRIO	(1<<1)	/**< Process' priority is fixed and should not be changed. */
#define PROCESS_NOASPACE	(1<<2)	/**< Process does not require a seperate address space. */

/** Macro that expands to a pointer to the current process. */
#define curr_proc		(curr_thread->owner)

extern process_t *kernel_proc;

extern process_t *process_lookup(process_id_t id);
extern int process_create(const char *name, process_t *parent, int priority, int flags,
                          struct subsystem *subsystem, process_t **procp);
extern int process_reset(process_t *process, const char *name, struct aspace *aspace,
                         struct subsystem *subsystem);

extern void process_init(void);

extern int kdbg_cmd_process(int argc, char **argv);

#endif /* __PROC_PROCESS_H */
