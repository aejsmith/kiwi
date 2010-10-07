/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Futex implementation.
 *
 * Reference:
 *  - Fuss, Futexes and Furwocks: Fast Userlevel Locking in Linux
 *    http://www.kernel.org/doc/ols/2002/ols2002-pages-479-495.pdf
 *  - Futexes are Tricky
 *    http://dept-info.labri.fr/~denis/Enseignement/2008-IR/Articles/01-futex.pdf
 *  - The magical Futex
 *    http://www.owenshepherd.net/2010/08/11/the-magical-futex/
 *
 * @todo		We should restrict what type of memory futexes can be
 *			placed in. For example, it makes little sense to allow
 *			one to be placed in a device's memory.
 */

#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/futex.h>
#include <sync/mutex.h>
#include <sync/waitq.h>

#include <assert.h>
#include <status.h>
#include <init.h>

/** Structure containing details of a futex. */
typedef struct futex {
	phys_ptr_t phys;		/**< Physical address of futex. */
	refcount_t count;		/**< Number of processes referring to the futex. */
	waitq_t queue;			/**< Queue for waiting on the futex. */
} futex_t;

/** Futex allocator. */
static slab_cache_t *futex_cache;

/** Tree of all futexes. */
static AVL_TREE_DECLARE(futex_tree);
static MUTEX_DECLARE(futex_tree_lock, 0);

/** Constructor for futex structures.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void futex_ctor(void *obj, void *data) {
	futex_t *futex = obj;
	waitq_init(&futex->queue, "futex_queue");
}

/** Clean up a process' futexes.
 * @param proc		Process to clean up. */
void futex_cleanup(process_t *proc) {
	futex_t *futex;

	mutex_lock(&futex_tree_lock);

	AVL_TREE_FOREACH_SAFE(&proc->futexes, iter) {
		futex = avl_tree_entry(iter, futex_t);

		avl_tree_remove(&proc->futexes, futex->phys);

		/* If no more processes refer to the futex we can free it. */
		if(refcount_dec(&futex->count) == 0) {
			avl_tree_remove(&futex_tree, futex->phys);
			slab_cache_free(futex_cache, futex);
		}
	}
}

/** Look up a futex.
 * @param addr		Virtual address in current process.
 * @return		Pointer to futex structure. Can only fail if the
 *			virtual address is invalid, in which case NULL will be
 *			returned. */
static futex_t *futex_lookup(ptr_t addr) {
	phys_ptr_t phys;
	futex_t *futex;
	int32_t tmp;

	/* Check if the address is 4 byte aligned. This will ensure that the
	 * address does not cross a page boundary because page sizes are
	 * powers of 2. */
	if(addr % sizeof(int32_t)) {
		return NULL;
	}

	/* Look up the physical address. */
	if(!page_map_find(&curr_aspace->pmap, addr, &phys)) {
		/* The page may not be mapped in. Try to trigger a fault, then
		 * check again. */
		if(memcpy_from_user(&tmp, (void *)addr, sizeof(tmp)) != STATUS_SUCCESS) {
			return NULL;
		} else if(!page_map_find(&curr_aspace->pmap, addr, &phys)) {
			return NULL;
		}
	}

	mutex_lock(&curr_proc->lock);

	/* Firstly try to find the futex in the process' tree. This is quicker
	 * than searching the global tree (after the first access to the futex
	 * by the process), as a process will only use a small subset of all of
	 * the futexes in the system. */
	futex = avl_tree_lookup(&curr_proc->futexes, phys);
	if(!futex) {
		mutex_lock(&futex_tree_lock);

		/* Use the global tree. */
		futex = avl_tree_lookup(&futex_tree, phys);
		if(!futex) {
			/* Couldn't find it, this means that this is the first
			 * access to this futex. Create a structure for it. */
			futex = slab_cache_alloc(futex_cache, MM_SLEEP);
			refcount_set(&futex->count, 1);
			futex->phys = phys;

			/* Attach it to the global tree and the process. */
			avl_tree_insert(&futex_tree, phys, futex, NULL);
			avl_tree_insert(&curr_proc->futexes, phys, futex, NULL);
		}

		mutex_unlock(&futex_tree_lock);
	}

	mutex_unlock(&curr_proc->lock);
	return futex;
}

/** Wait for a futex.
 * @param addr		Pointer to futex.
 * @param val		Value of the futex prior to the call. This is needed to
 *			prevent a race condition if another thread ups the
 *			futex before this thread has gone to sleep. If the
 *			value has changed just before sleeping, the function
 *			will return STATUS_TRY_AGAIN.
 * @param timeout	Timeout in microseconds. If -1, the function will block
 *			until woken by a call to futex_wake(). If 0, an error
 *			will be returned immediately.
 * @return		Status code describing result of the operation. */
status_t sys_futex_wait(int32_t *addr, int32_t val, useconds_t timeout) {
	int32_t *mapping;
	futex_t *futex;
	status_t ret;
	bool state;

	/* Find the futex. */
	futex = futex_lookup((ptr_t)addr);
	if(!futex) {
		return STATUS_INVALID_ADDR;
	}

	/* Map the futex into memory so that we can check its value.
	 * Wire ourselves to the current CPU to make a remote TLB invalidation
	 * unnecessary when unmapping. */
	thread_wire(curr_thread);
	mapping = page_phys_map(futex->phys, sizeof(*mapping), MM_SLEEP);

	/* Prepare to sleep on the queue. */
	state = waitq_sleep_prepare(&futex->queue);

	/* Now check the value to see if it has changed (see parameter
	 * description above). */
	if(*mapping == val) {
		ret = waitq_sleep_unsafe(&futex->queue, timeout, SYNC_INTERRUPTIBLE, state);
	} else {
		waitq_sleep_cancel(&futex->queue, state);
		ret = STATUS_TRY_AGAIN;
	}

	page_phys_unmap(mapping, sizeof(*mapping), false);
	thread_unwire(curr_thread);
	return ret;
}

/** Wake up threads waiting on a futex.
 * @param addr		Pointer to futex.
 * @param count		Number of threads to attempt to wake.
 * @param wokenp	Where to store number of threads actually woken.
 * @return		Status code describing result of the operation. */
status_t sys_futex_wake(int32_t *addr, size_t count, size_t *wokenp) {
	size_t woken = 0;
	futex_t *futex;

	if(!count) {
		return STATUS_INVALID_ARG;
	}

	/* Find the futex. */
	futex = futex_lookup((ptr_t)addr);
	if(!futex) {
		return STATUS_INVALID_ADDR;
	}

	/* Wake the threads. */
	while(count--) {
		if(!waitq_wake(&futex->queue)) {
			break;
		}
		woken++;
	}

	/* Store the number of woken threads if requested. */
	if(wokenp) {
		return memcpy_to_user(wokenp, &woken, sizeof(*wokenp));
	}
	return STATUS_SUCCESS;
}

/** Initialise the futex cache. */
static void __init_text futex_init(void) {
	futex_cache = slab_cache_create("futex_cache", sizeof(futex_t), 0, futex_ctor,
	                                NULL, NULL, NULL, 0, MM_FATAL);
}
INITCALL(futex_init);
