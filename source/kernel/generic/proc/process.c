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

/** Allocate a new ID for a process.
 * @return		Allocated ID. */
static process_id_t process_id_alloc(void) {
	if(kernel_proc == NULL) {
		return 0;
	}

	return (process_id_t)vmem_alloc(process_id_arena, 1, MM_SLEEP);
}

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
 * @param flags		Behaviour flags for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char *name, process_t *parent, int priority, int flags, process_t **procp) {
	process_t *process;

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
		process->aspace = NULL;
	}

	/* Allocate an ID for the process. */
	process->id = process_id_alloc();

	process->flags = flags;
	process->priority = priority;
	process->num_threads = 0;
	process->state = PROC_RUNNING;
	process->parent = parent;

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

	dprintf("proc: created process %" PRIu32 "(%s) (proc: 0x%p, parent: 0x%p)\n",
		process->id, process->name, process, parent);
	return 0;
}

/** Initialize the process table and slab cache. */
void process_init(void) {
	int ret;

	/* Create the process slab cache and ID Vmem arena. */
	process_id_arena = vmem_create("process_id_arena", 1, 65534, 1, NULL, NULL, NULL, 0, MM_FATAL);
	process_cache = slab_cache_create("process_cache", sizeof(process_t), 0,
	                                  process_cache_ctor, NULL, NULL, NULL, 0, MM_FATAL);

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

	kprintf(LOG_NONE, "ID    Priority Flags Threads  AS                 Name\n");
	kprintf(LOG_NONE, "==    ======== ===== =======  ==                 ====\n");

	AVLTREE_FOREACH(&process_tree, iter) {
		process = avltree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRIu32 " %-8d %-5d %-8" PRIs " 0x%-16p %s\n",
			process->id, process->priority, process->flags,
			process->num_threads, process->aspace, process->name);
	}

	return KDBG_OK;
}
