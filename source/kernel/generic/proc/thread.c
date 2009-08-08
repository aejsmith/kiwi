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

#include <console/kprintf.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <mm/slab.h>
#include <mm/kheap.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/waitq.h>

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

extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);

static AVL_TREE_DECLARE(thread_tree);		/**< Tree of all threads. */
static SPINLOCK_DECLARE(thread_tree_lock);	/**< Lock for thread AVL tree. */
static vmem_t *thread_id_arena;			/**< Thread ID Vmem arena. */
static slab_cache_t *thread_cache;		/**< Cache for thread structures. */

/** Constructor for thread objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int thread_cache_ctor(void *obj, void *data, int kmflag) {
	thread_t *thread = (thread_t *)obj;

	spinlock_init(&thread->lock, "thread_lock");
	list_init(&thread->header);
	list_init(&thread->waitq_link);
	list_init(&thread->owner_link);

	return 0;
}

/** Thread entry point.
 *
 * Entry point for all threads. Wraps the real main function for a thread to
 * peform post-switch tasks before calling the function. This is necessary
 * because when real_schedule() switches to a newly-created thread, it will
 * return to this function and sched_post() does not get called by the
 * scheduler, so this function must do that.
 */
static void thread_trampoline(void) {
	sched_post_switch(true);

	dprintf("thread: entered thread %" PRId32 "(%s) on CPU %" PRIu32 "\n",
		curr_thread->id, curr_thread->name, curr_cpu->id);

	curr_thread->entry(curr_thread->arg1, curr_thread->arg2);
	thread_exit();
	while(1) {
		sched_yield();
	}
}

/** Lookup a thread.
 *
 * Looks for a thread with the specified id in the thread tree.
 *
 * @param id		ID of the thread to find.
 *
 * @return		Pointer to thread found, or NULL if not found.
 */
thread_t *thread_lookup(identifier_t id) {
	thread_t *thread;

	spinlock_lock(&thread_tree_lock, 0);
	thread = avl_tree_lookup(&thread_tree, (key_t)id);
	spinlock_unlock(&thread_tree_lock);

	return thread;
}

/** Run a newly-created thread.
 *
 * Moves a newly created thread into the Ready state and places it on the
 * run queues to be scheduled.
 *
 * @param thread	Thread to run.
 */
void thread_run(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	assert(thread->state == THREAD_CREATED);

	thread->state = THREAD_READY;
	thread->cpu = curr_cpu;
	sched_thread_insert(thread);

	spinlock_unlock(&thread->lock);
}

/** Rename a thread.
 *
 * Changes the name of a thread.
 *
 * @param thread	Thread to rename.
 * @param name		New name for the thread.
 */
void thread_rename(thread_t *thread, const char *name) {
	spinlock_lock(&thread->lock, 0);
	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;
	spinlock_unlock(&thread->lock);
}

/** Create a new thread.
 *
 * Creates a new thread that will begin execution at the given function and
 * places it in the Created state.
 *
 * @todo		If the thread is not tied to the current CPU, pick the
 *			best CPU for it to run on.
 *
 * @param name		Name to give the thread.
 * @param owner		Process that the thread should belong to.
 * @param flags		Flags for the thread.
 * @param entry		Entry function for the thread.
 * @param arg1		First argument to pass to entry function.
 * @param arg2		Second argument to pass to entry function.
 * @param threadp	Where to store pointer to thread structure.
 *
 * @return		0 on success, a negative error code on failure.
 */
int thread_create(const char *name, process_t *owner, int flags, thread_func_t entry,
                  void *arg1, void *arg2, thread_t **threadp) {
	thread_t *thread;
	int ret;

	if(name == NULL || owner == NULL || threadp == NULL) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a thread structure from the cache. The thread constructor
	 * caches a kernel stack with the thread for us. */
	thread = slab_cache_alloc(thread_cache, MM_SLEEP);

	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;

	/* Allocate a kernel stack and initialize the thread context. */
	thread->kstack = kheap_alloc(KSTACK_SIZE, MM_SLEEP);
	context_init(&thread->context, (ptr_t)thread_trampoline, thread->kstack);

	/* Initialize architecture-specific data. */
	ret = thread_arch_init(thread);
	if(ret != 0) {
		kheap_free(thread->kstack, KSTACK_SIZE);
		slab_cache_free(thread_cache, thread);
		return ret;
	}

	/* Allocate an ID for the thread. */
	thread->id = (identifier_t)vmem_alloc(thread_id_arena, 1, MM_SLEEP);

	atomic_set(&thread->in_usermem, 0);

	/* Initially set the CPU to NULL - the thread will be assigned to a
	 * CPU when thread_run() is called on it. */
	thread->cpu = NULL;

	thread->flags = flags;
	thread->priority = 0;
	thread->timeslice = 0;
	thread->preempt_off = 0;
	thread->preempt_missed = false;
	thread->waitq = NULL;
	thread->interruptible = false;
	thread->state = THREAD_CREATED;
	thread->entry = entry;
	thread->arg1 = arg1;
	thread->arg2 = arg2;
	thread->owner = owner;

	/* Add the thread to the owner. */
	spinlock_lock(&owner->lock, 0);
	list_append(&owner->threads, &thread->owner_link);
	owner->num_threads++;
	spinlock_unlock(&owner->lock);

	/* Add to the thread tree. */
	spinlock_lock(&thread_tree_lock, 0);
	avl_tree_insert(&thread_tree, (key_t)thread->id, thread, NULL);
	spinlock_unlock(&thread_tree_lock);

	*threadp = thread;

	dprintf("thread: created thread %" PRId32 "(%s) (thread: %p, owner: %p)\n",
		thread->id, thread->name, thread, owner);
	return 0;
}

/** Destroy a thread.
 *
 * Detaches the given thread from its owner and destroys it.
 *
 * @param thread	Thread to destroy.
 */
void thread_destroy(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	dprintf("thread: destroying thread %" PRId32 "(%s) (thread: %p, owner: %" PRId32 ")\n",
		thread->id, thread->name, thread, thread->owner->id);

	assert(list_empty(&thread->header));

	/* Detach from its owner. */
	spinlock_lock(&thread->owner->lock, 0);
	list_remove(&thread->owner_link);
	thread->owner->num_threads--;
	spinlock_unlock(&thread->owner->lock);

	/* Remove from thread tree. */
	spinlock_lock(&thread_tree_lock, 0);
	avl_tree_remove(&thread_tree, (key_t)thread->id);
	spinlock_unlock(&thread_tree_lock);

	/* Now clean up the thread. */
	kheap_free(thread->kstack, KSTACK_SIZE);
	context_destroy(&thread->context);
	thread_arch_destroy(thread);

	/* Deallocate the thread ID. */
	vmem_free(thread_id_arena, (vmem_resource_t)thread->id, 1);

	spinlock_unlock(&thread->lock);
	slab_cache_free(thread_cache, thread);
}

/** Terminate the current thread. */
void thread_exit(void) {
	curr_thread->state = THREAD_DEAD;
	sched_yield();
}

/** Initialize the thread cache. */
void __init_text thread_init(void) {
	thread_id_arena = vmem_create("thread_id_arena", 1, 65534, 1, NULL, NULL, NULL, 0, MM_FATAL);
	thread_cache = slab_cache_create("thread_cache", sizeof(thread_t), 0,
	                                 thread_cache_ctor, NULL, NULL, NULL,
	                                 NULL, 0, MM_FATAL);
}

/** Print information about a thread.
 * @param thread	Thread to print.
 * @param level		Log level. */
static inline void thread_dump(thread_t *thread, int level) {
	kprintf(level, "%-5" PRId32 " %-5" PRId32 " ", thread->id, thread->owner->id);

	switch(thread->state) {
	case THREAD_CREATED:	kprintf(level, "Created  "); break;
	case THREAD_READY:	kprintf(level, "Ready    "); break;
	case THREAD_RUNNING:	kprintf(level, "Running  "); break;
	case THREAD_SLEEPING:	kprintf(level, "Sleeping "); break;
	case THREAD_DEAD:	kprintf(level, "Dead (!) "); break;
	default:		kprintf(level, "Bad      "); break;
	}

	kprintf(level, "%-4" PRIu32 " %-4zu %-5d %-20s %s\n", (thread->cpu) ? thread->cpu->id : 0,
		thread->priority, thread->flags, (thread->waitq) ? thread->waitq->name : "None",
		thread->name);
}

/** Dump a list of threads.
 *
 * Dumps out a list of threads.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_thread(int argc, char **argv) {
	process_t *process;
	thread_t *thread;
	unative_t pid;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<process ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all threads, or a list of threads within a process\n");
		kprintf(LOG_NONE, "if given a process ID. The ID is given as an expression.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID    Owner State    CPU  Prio Flags WaitQ                Name\n");
	kprintf(LOG_NONE, "==    ===== =====    ===  ==== ===== =====                ====\n");

	if(argc == 2) {
		/* Find the process ID. */
		if(kdbg_parse_expression(argv[1], &pid, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(process = process_lookup(pid))) {
			kprintf(LOG_NONE, "Invalid process ID.\n");
			return KDBG_FAIL;
		}

		LIST_FOREACH(&process->threads, iter) {
			thread = list_entry(iter, thread_t, owner_link);
			thread_dump(thread, LOG_NONE);
		}
	} else {
		AVL_TREE_FOREACH(&thread_tree, iter) {
			thread = avl_tree_entry(iter, thread_t);
			thread_dump(thread, LOG_NONE);
		}
	}

	return KDBG_OK;
}
