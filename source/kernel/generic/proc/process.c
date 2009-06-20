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
#include <proc/subsystem.h>
#include <proc/thread.h>

#include <types/avltree.h>

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

static AVLTREE_DECLARE(process_tree);		/**< Tree of all processes. */
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
process_t *process_lookup(process_id_t id) {
	process_t *process;

	spinlock_lock(&process_tree_lock, 0);
	process = avltree_lookup(&process_tree, (key_t)id);
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
 * @param subsystem	Subsystem that the process will run under.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char *name, process_t *parent, int priority, int flags,
                   subsystem_t *subsystem, process_t **procp) {
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
	process->id = (kernel_proc) ? (process_id_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : 0;

	process->flags = flags;
	process->priority = priority;
	process->num_threads = 0;
	process->state = PROC_RUNNING;
	process->parent = parent;

	/* Perform subsystem initialization, if any. */
	process->subsystem = subsystem;
	if(process->subsystem && process->subsystem->process_init) {
		ret = process->subsystem->process_init(process);
		if(ret != 0) {
			vmem_free(process_id_arena, (unative_t)process->id, 1);
			slab_cache_free(process_cache, process);
			return ret;
		}
	}

	/* Add to the parent's process list. */
	if(parent != NULL) {
		spinlock_lock(&parent->lock, 0);
		list_append(&parent->children, &process->parent_link);
		spinlock_unlock(&parent->lock);
	}

	/* Add to the process tree. */
	spinlock_lock(&process_tree_lock, 0);
	avltree_insert(&process_tree, (key_t)process->id, process, NULL);
	spinlock_unlock(&process_tree_lock);

	*procp = process;

	dprintf("proc: created process %" PRIu32 "(%s) (proc: %p, parent: %p)\n",
		process->id, process->name, process, parent);
	return 0;
}

/** Reset the specified process.
 *
 * Resets the state of the specified process. If the process is the current
 * process, then all threads in the process will be terminated other than the
 * current. Otherwise, all threads will be terminated. The name of the process
 * will then be changed to the given name and its address space will be set to
 * the given address space (the old one will be destroyed, if any). If the
 * process' subsystem is the same as the new subsystem, then the subsystem's
 * reset callback will be called. Otherwise, the current subsystem's
 * destruction callback will be called (if there is a current subsystem),
 * and the new subsystem's initialization callback will be called (if there
 * is a new subsystem).
 *
 * The only two reasons for this function to fail are if the new subsystem
 * differs from the old system and the new subsystem's initialization callback
 * fails, or if invalid parameters are passed. It is up to the caller to handle
 * failures - the best thing to do in this case would be to terminate the
 * process.
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
 * @param subsystem	New subsystem for the process (can be NULL).
 *
 * @return		0 on success, negative error code on failure.
 */
int process_reset(process_t *process, const char *name, aspace_t *aspace, subsystem_t *subsystem) {
	subsystem_t *prev_subsys;
	aspace_t *prev_as;
	int ret;

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
	prev_as = process->aspace;
	process->aspace = aspace;
	if(prev_as == curr_aspace) {
		aspace_switch(aspace);
	}

	/* Swap the subsystem pointers. */
	prev_subsys = process->subsystem;
	process->subsystem = subsystem;

	spinlock_unlock(&process->lock);

	/* Now that we're unlocked we can destroy the old address space. */
	if(prev_as) {
		aspace_destroy(prev_as);
	}

	/* If the previous subsystem is the same as the old subsystem, we use
	 * the process_reset callback. Otherwise, destroy the old subsystem,
	 * and initialize the new one. */
	if(subsystem && subsystem == prev_subsys) {
		if(subsystem->process_reset) {
			subsystem->process_reset(process);
		}
	} else {
		if(prev_subsys && prev_subsys->process_destroy) {
			prev_subsys->process_destroy(process);
		}

		/* Initialize the new subsystem. If it fails, set subsystem
		 * to NULL. */
		if(subsystem && subsystem->process_init) {
			ret = process->subsystem->process_init(process);
			if(ret != 0) {
				process->subsystem = NULL;
				return ret;
			}
		}
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
	                     NULL, &kernel_proc);
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

	kprintf(LOG_NONE, "ID    Prio Flags Threads Aspace             Subsystem Name\n");
	kprintf(LOG_NONE, "==    ==== ===== ======= ======             ========= ====\n");

	AVLTREE_FOREACH(&process_tree, iter) {
		process = avltree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRIu32 " %-4d %-5d %-7" PRIs " %-16p %-9s %s\n",
			process->id, process->priority, process->flags,
			process->num_threads, process->aspace,
		        (process->subsystem) ? process->subsystem->name : "None",
		        process->name);
	}

	return KDBG_OK;
}
