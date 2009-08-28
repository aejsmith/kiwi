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

#include <io/context.h>

#include <lib/notifier.h>

#include <proc/handle.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/spinlock.h>

#include <types/list.h>
#include <types/refcount.h>

struct vfs_node;
struct vm_aspace;

/** Process arguments structure. */
typedef struct process_args {
	char *path;			/**< Path to program. */
	char **args;			/**< Argument array. */
	char **env;			/**< Environment variable array. */
	int args_count;			/**< Number of entries in argument array (excluding NULL-terminator). */
	int env_count;			/**< Number of entries in environment array (excluding NULL-terminator). */
} process_args_t;

/** Structure containing details about a process. */
typedef struct process {
	spinlock_t lock;		/**< Lock to protect data in structure. */
	identifier_t id;		/**< ID of the process. */
	char *name;			/**< Name of the process. */
	int flags;			/**< Behaviour flags for the process. */
	size_t priority;		/**< Priority of the process. */
	refcount_t count;		/**< Number of handles/threads open to the process. */
	int status;			/**< Exit status of the process. */

	struct vm_aspace *aspace;	/**< Process' address space. */
	list_t threads;			/**< List of threads. */
	handle_table_t handles;		/**< Table of open handles. */
	io_context_t ioctx;		/**< I/O context structure. */

	notifier_t death_notifier;	/**< Notifier called when process dies. */
} process_t;

/** Process flag definitions */
#define PROCESS_CRITICAL	(1<<0)	/**< Process is critical to system operation, cannot die. */
#define PROCESS_FIXEDPRIO	(1<<1)	/**< Process' priority is fixed and should not be changed. */

/** Macro that expands to a pointer to the current process. */
#define curr_proc		(curr_thread->owner)

extern process_t *kernel_proc;

extern int process_create(const char **args, const char **environ, int flags, int priority,
                          process_t *parent, process_t **procp);
extern process_t *process_lookup(identifier_t id);

extern void process_init(void);

extern int kdbg_cmd_process(int argc, char **argv);

extern handle_t sys_process_create(const char *path, char *const args[], char *const environ[], bool inherit);
extern int sys_process_replace(const char *path, char *const args[], char *const environ[], bool inherit);
extern int sys_process_duplicate(handle_t *handlep);
extern handle_t sys_process_open(identifier_t id);
extern identifier_t sys_process_id(handle_t handle);
extern void sys_process_exit(int status);

#endif /* __PROC_PROCESS_H */
