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

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <object.h>

struct vm_aspace;

/** Structure containing process creation information. */
typedef struct process_create_info {
	/** Arguments provided by the caller. */
	const char *path;		/**< Path to program. */
	const char *const *args;	/**< Argument array. */
	const char *const *env;		/**< Environment array. */
	handle_t (*handles)[2];		/**< Handle mapping array. */
	int count;			/**< Number of handles in the array. */

	/** Information used internally by the loader. */
	struct vm_aspace *aspace;	/**< Address space for the process. */
	void *data;			/**< Data pointer for the ELF loader. */
	int argc;			/**< Argument count. */
	int envc;			/**< Environment variable count. */
	ptr_t arg_block;		/**< Address of argument block mapping. */
	ptr_t stack;			/**< Address of stack mapping. */

	/** Information to return to the caller. */
	semaphore_t sem;		/**< Semaphore to wait for completion on. */
	int status;			/**< Status code to return from the call. */
} process_create_info_t;

/** Structure containing details about a process. */
typedef struct process {
	object_t obj;			/**< Kernel object header. */

	int flags;			/**< Behaviour flags for the process. */
	size_t priority;		/**< Priority of the process. */
	struct vm_aspace *aspace;	/**< Process' address space. */
	mutex_t lock;			/**< Lock to protect data in structure. */
	refcount_t count;		/**< Number of handles to/threads in the process. */
	handle_table_t *handles;	/**< Table of open handles. */
	io_context_t ioctx;		/**< I/O context structure. */
	list_t threads;			/**< List of threads. */

	/** State of the process. */
	enum {
		PROCESS_RUNNING,	/**< Running. */
		PROCESS_DEAD,		/**< Dead. */
	} state;

	process_id_t id;		/**< ID of the process. */
	char *name;			/**< Name of the process. */
	notifier_t death_notifier;	/**< Notifier for process death. */
	int status;			/**< Exit status of the process. */
	process_create_info_t *create;	/**< Creation information structure. */
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
extern int process_create(const char *const args[], const char *const env[], int flags,
                          int priority, process_t *parent, process_t **procp);
extern void process_exit(int status) __noreturn;

extern int kdbg_cmd_process(int argc, char **argv);

extern void process_init(void);

#endif /* __PROC_PROCESS_H */
