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

#include <console/kprintf.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/slab.h>
#include <mm/vmem.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <types/avl.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Process containing all kernel-mode threads. */
process_t *kernel_proc;

static AVL_TREE_DECLARE(process_tree);		/**< Tree of all processes. */
static SPINLOCK_DECLARE(process_tree_lock);	/**< Lock for process AVL tree. */
static vmem_t *process_id_arena;		/**< Process ID Vmem arena. */
static slab_cache_t *process_cache;		/**< Cache for process structures. */

/** Constructor for process objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int process_cache_ctor(void *obj, void *data, int kmflag) {
	process_t *process = (process_t *)obj;

	spinlock_init(&process->lock, "process_lock");
	list_init(&process->threads);
	list_init(&process->children);
	list_init(&process->parent_link);

	return 0;
}

/** Lookup a process.
 *
 * Looks for a process with the specified ID in the process tree.
 *
 * @param id		ID of the process to find.
 *
 * @return		Pointer to process found, or NULL if not found.
 */
process_t *process_lookup(identifier_t id) {
	process_t *process;

	spinlock_lock(&process_tree_lock, 0);
	process = avl_tree_lookup(&process_tree, (key_t)id);
	spinlock_unlock(&process_tree_lock);

	return process;
}

/** Create a new process.
 *
 * Allocates and initializes a new process.
 *
 * @param name		Name to give the process.
 * @param parent	Parent of the process.
 * @param priority	Priority for the process.
 * @param flags		Behaviour/creation flags for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char *name, process_t *parent, int priority, int flags, process_t **procp) {
	process_t *process;
	int ret;

	if(name == NULL || (parent == NULL && kernel_proc != NULL) || procp == NULL) {
		return -ERR_PARAM_INVAL;
	} else if(priority < 0 || priority >= PRIORITY_MAX) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a process structure from the cache. */
	process = slab_cache_alloc(process_cache, MM_SLEEP);

	strncpy(process->name, name, PROC_NAME_MAX);
	process->name[PROC_NAME_MAX - 1] = 0;

	/* Create the address space. */
	if(!(flags & PROCESS_NOASPACE)) {
		if(!(process->aspace = aspace_create())) {
			slab_cache_free(process_cache, process);
			return -ERR_NO_MEMORY;
		}
	} else {
		flags &= ~PROCESS_NOASPACE;
		process->aspace = NULL;
	}

	/* Allocate an ID for the process. */
	process->id = (kernel_proc) ? (identifier_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : 0;

	process->flags = flags;
	process->priority = priority;
	process->num_threads = 0;
	process->state = PROC_RUNNING;
	process->parent = parent;

	/* Initialize the process' handle table. */
	ret = handle_table_init(&process->handles, (parent) ? &parent->handles : NULL);
	if(ret != 0) {
		vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
		slab_cache_free(process_cache, process);
		return ret;
	}

	/** Initialize the I/O context. */
	ret = io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	if(ret != 0) {
		handle_table_destroy(&process->handles);
		vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
		slab_cache_free(process_cache, process);
		return ret;
	}

	/* Add to the parent's process list. */
	if(parent != NULL) {
		spinlock_lock(&parent->lock, 0);
		list_append(&parent->children, &process->parent_link);
		spinlock_unlock(&parent->lock);
	}

	/* Add to the process tree. */
	spinlock_lock(&process_tree_lock, 0);
	avl_tree_insert(&process_tree, (key_t)process->id, process, NULL);
	spinlock_unlock(&process_tree_lock);

	*procp = process;

	dprintf("proc: created process %" PRId32 "(%s) (proc: %p, parent: %p)\n",
		process->id, process->name, process, parent);
	return 0;
}

/** Reset the specified process.
 *
 * Resets the state of the specified process. If the process is the current
 * process, then all threads in the process will be terminated other than the
 * current. Otherwise, all threads will be terminated. The name of the process
 * will then be changed to the given name and its address space will be set to
 * the given address space (the old one will be destroyed, if any).
 *
 * The only reason for this function to fail is if invalid parameters are
 * passed. It is up to the caller to handle failures - the best thing to do in
 * this case would be to terminate the process.
 *
 * If the process is the current process, and the old address space set in the
 * process is the current address space, then this function will switch to the
 * new address space.
 *
 * @todo		The functionality to kill threads is not yet
 *			implemented. This means that it cannot be used on
 *			the current process if it has threads other than the
 *			current, or on other processes that have threads.
 *
 * @param process	Process to reset. Must not be the kernel process.
 * @param name		New name to give the process (must not be zero-length).
 * @param aspace	Address space for the process (can be NULL).
 *
 * @return		0 on success, negative error code on failure.
 */
int process_reset(process_t *process, const char *name, aspace_t *aspace) {
	aspace_t *prev;

	if(!process || process == kernel_proc || !name || !name[0]) {
		return -ERR_PARAM_INVAL;
	}

	spinlock_lock(&process->lock, 0);

	/* TODO: Implement these. */
	if((process == curr_proc && process->num_threads > 1) || (process != curr_proc && process->num_threads)) {
		fatal("TODO: Implement process_reset for extra threads");
	}

	/* Reset the process' name. */
	strncpy(process->name, name, PROC_NAME_MAX);
	process->name[PROC_NAME_MAX - 1] = 0;

	/* Replace the address space, and switch to the new one. */
	prev = process->aspace;
	process->aspace = aspace;
	if(prev == curr_aspace) {
		aspace_switch(aspace);
	}

	spinlock_unlock(&process->lock);

	/* Now that we're unlocked we can destroy the old address space. */
	if(prev) {
		aspace_destroy(prev);
	}

	return 0;
}

/** Initialize the process table and slab cache. */
void process_init(void) {
	int ret;

	/* Create the process slab cache and ID Vmem arena. */
	process_id_arena = vmem_create("process_id_arena", 1, 65534, 1, NULL, NULL, NULL, 0, MM_FATAL);
	process_cache = slab_cache_create("process_cache", sizeof(process_t), 0,
	                                  process_cache_ctor, NULL, NULL, NULL, NULL,
	                                  0, MM_FATAL);

	/* Create the kernel process. */
	ret = process_create("[kernel]", NULL, PRIORITY_KERNEL,
	                     PROCESS_CRITICAL | PROCESS_FIXEDPRIO | PROCESS_NOASPACE,
	                     &kernel_proc);
	if(ret != 0) {
		fatal("Could not initialize kernel process: %d", ret);
	}
}

/** Dump the contents of the process table.
 *
 * Dumps out the contents of the process hash table.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_process(int argc, char **argv) {
	process_t *process;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all running processes.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID    Prio Flags Threads Aspace             Name\n");
	kprintf(LOG_NONE, "==    ==== ===== ======= ======             ====\n");

	AVL_TREE_FOREACH(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRId32 " %-4d %-5d %-7zu %-18p %s\n",
			process->id, process->priority, process->flags,
			process->num_threads, process->aspace, process->name);
	}

	return KDBG_OK;
}
