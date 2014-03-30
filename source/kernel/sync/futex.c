/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @todo		Use a hash table instead of an AVL tree for the global
 *			table?
 */

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/futex.h>
#include <sync/mutex.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

/** Structure containing details of a futex. */
typedef struct futex {
	phys_ptr_t phys;		/**< Physical address of futex. */
	refcount_t count;		/**< Number of processes referring to the futex. */
	spinlock_t lock;		/**< Lock for the futex. */
	list_t threads;			/**< List of threads waiting on the futex. */
	avl_tree_node_t tree_link;	/**< Link to global futex tree. */
} futex_t;

/** Structure linking a futex to a process. */
typedef struct futex_link {
	futex_t *futex;			/**< Futex. */
	avl_tree_node_t node;		/**< AVL tree node. */
} futex_link_t;

/** Futex allocator. */
static slab_cache_t *futex_cache = NULL;

/** Tree of all futexes, keyed by physical address. */
static AVL_TREE_DEFINE(futex_tree);
static MUTEX_DEFINE(futex_tree_lock, 0);

/** Constructor for futex structures.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void futex_ctor(void *obj, void *data) {
	futex_t *futex = obj;

	spinlock_init(&futex->lock, "futex_lock");
	list_init(&futex->threads);
}

/** Clean up a process' futexes.
 * @param proc		Process to clean up. */
void futex_process_cleanup(process_t *proc) {
	futex_link_t *link;
	futex_t *futex;

	mutex_lock(&futex_tree_lock);

	AVL_TREE_FOREACH_SAFE(&proc->futexes, iter) {
		link = avl_tree_entry(iter, futex_link_t, node);

		futex = link->futex;
		avl_tree_remove(&proc->futexes, &link->node);
		kfree(link);

		/* If no more processes refer to the futex we can free it. */
		if(refcount_dec(&futex->count) == 0) {
			avl_tree_remove(&futex_tree, &futex->tree_link);
			slab_cache_free(futex_cache, futex);
		}
	}

	mutex_unlock(&futex_tree_lock);
}

/** Look up a futex.
 * @param addr		Virtual address in current process.
 * @param futexp	Where to store pointer to futex structure.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR or
 *			STATUS_ACCESS_DENIED if the address is invalid or is
 *			not writeable. */
static status_t futex_lookup(int32_t *addr, futex_t **futexp) {
	ptr_t base, offset;
	phys_ptr_t phys;
	futex_link_t *link;
	futex_t *futex;
	status_t ret;

	/* Check if the address is 4 byte aligned. This will ensure that the
	 * address does not cross a page boundary because page sizes are
	 * powers of 2. */
	if(!addr || (ptr_t)addr % sizeof(int32_t))
		return STATUS_INVALID_ARG;

	/* Get the page containing the address and the offset within it. */
	base = ROUND_DOWN((ptr_t)addr, PAGE_SIZE);
	offset = (ptr_t)addr - base;

	/* Lock the page for read and write access and look up the physical
	 * address of it. */
	ret = vm_lock_page(curr_proc->aspace, base, VM_PROT_READ | VM_PROT_WRITE,
		&phys);
	if(ret != STATUS_SUCCESS)
		return ret;

	phys += offset;

	mutex_lock(&curr_proc->lock);

	/* Firstly try to find the futex in the process' tree. This is quicker
	 * than searching the global tree (after the first access to the futex
	 * by the process), as a process will only use a small subset of all of
	 * the futexes in the system. */
	link = avl_tree_lookup(&curr_proc->futexes, phys, futex_link_t, node);
	if(!link) {
		mutex_lock(&futex_tree_lock);

		/* Use the global tree. */
		futex = avl_tree_lookup(&futex_tree, phys, futex_t, tree_link);
		if(!futex) {
			/* Couldn't find it, this means that this is the first
			 * access to this futex. Create a structure for it. */
			futex = slab_cache_alloc(futex_cache, MM_KERNEL);
			refcount_set(&futex->count, 1);
			futex->phys = phys;

			/* Attach it to the global tree. */
			avl_tree_insert(&futex_tree, phys, &futex->tree_link);
		} else {
			refcount_inc(&futex->count);
		}

		mutex_unlock(&futex_tree_lock);

		/* Attach to the process' tree. */
		link = kmalloc(sizeof(*link), MM_KERNEL);
		link->futex = futex;
		avl_tree_insert(&curr_proc->futexes, phys, &link->node);
	}

	mutex_unlock(&curr_proc->lock);
	*futexp = link->futex;
	return STATUS_SUCCESS;
}

/** Unlock the page containing a futex.
 * @param addr		Address of futex. */
static void futex_finish(int32_t *addr) {
	ptr_t base;

	base = ROUND_DOWN((ptr_t)addr, PAGE_SIZE);
	vm_unlock_page(curr_proc->aspace, base);
}

/** Wait for a futex.
 * @param addr		Pointer to futex.
 * @param val		Value of the futex prior to the call. This is needed to
 *			prevent a race condition if another thread modifies the
 *			futex before this thread has gone to sleep. If the
 *			value has changed just before sleeping, the function
 *			will return STATUS_TRY_AGAIN.
 * @param timeout	Timeout in nanoseconds. If -1, the function will block
 *			until woken by a call to futex_wake(). If 0, an error
 *			will be returned immediately.
 * @return		Status code describing result of the operation. */
status_t kern_futex_wait(int32_t *addr, int32_t val, nstime_t timeout) {
	futex_t *futex;
	status_t ret;

	/* Find the futex. */
	ret = futex_lookup(addr, &futex);
	if(ret != STATUS_SUCCESS)
		return ret;

	spinlock_lock(&futex->lock);

	/* Now check the value to see if it has changed (see parameter
	 * description above). The page is locked meaning it is safe to access
	 * it directly. */
	if(*addr == val) {
		list_append(&futex->threads, &curr_thread->wait_link);
		ret = thread_sleep(&futex->lock, timeout, "futex", SLEEP_INTERRUPTIBLE);
	} else {
		spinlock_unlock(&futex->lock);
		ret = STATUS_TRY_AGAIN;
	}

	futex_finish(addr);
	return ret;
}

/** Wake up threads waiting on a futex.
 * @param addr		Pointer to futex.
 * @param count		Number of threads to attempt to wake.
 * @param wokenp	Where to store number of threads actually woken.
 * @return		Status code describing result of the operation. */
status_t kern_futex_wake(int32_t *addr, size_t count, size_t *wokenp) {
	futex_t *futex;
	size_t woken = 0;
	thread_t *thread;
	status_t ret;

	if(!count)
		return STATUS_INVALID_ARG;

	/* Find the futex. */
	ret = futex_lookup(addr, &futex);
	if(ret != STATUS_SUCCESS)
		return ret;

	spinlock_lock(&futex->lock);

	/* Wake the threads. */
	while(count-- && !list_empty(&futex->threads)) {
		thread = list_first(&futex->threads, thread_t, wait_link);
		thread_wake(thread);
		woken++;
	}

	spinlock_unlock(&futex->lock);
	futex_finish(addr);

	/* Store the number of woken threads if requested. */
	return (wokenp) ? write_user(wokenp, woken) : STATUS_SUCCESS;
}

/**
 * Wake up or requeue threads waiting on a futex.
 *
 * Wakes up the specified number of threads from the source futex, and moves
 * all the remaining waiting threads to the wait queue of the destination
 * futex.
 *
 * @param addr1		Pointer to source futex.
 * @param val		Value of the source futex prior to the call. This is
 *			needed to prevent a race condition if another thread
 *			modifies the futex prior to waking. If the value has
 *			changed, the function will return STATUS_TRY_AGAIN.
 * @param count		Number of threads to wake.
 * @param addr2		Pointer to destination futex.
 * @param wokenp	Where to store number of woken threads.
 *
 * @return		Status code describing the result of the operation.
 */
status_t kern_futex_requeue(int32_t *addr1, int32_t val, size_t count,
	int32_t *addr2, size_t *wokenp)
{
	futex_t *source, *dest;
	size_t woken = 0;
	thread_t *thread;
	status_t ret;

	if(!count)
		return STATUS_INVALID_ARG;

	/* Find the futexes. */
	ret = futex_lookup(addr1, &source);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = futex_lookup(addr2, &dest);
	if(ret != STATUS_SUCCESS) {
		futex_finish(addr1);
		return ret;
	}

	/* Another thread could potentially be performing a requeue with source
	 * and dest swapped. Avoid deadlock by locking the futex with the
	 * lowest address first. */
	if(source <= dest) {
		spinlock_lock(&source->lock);
		if(source != dest)
			spinlock_lock(&dest->lock);
	} else {
		spinlock_lock(&dest->lock);
		spinlock_lock(&source->lock);
	}

	/* Now check the value to see if it has changed (see parameter
	 * description above). The page is locked meaning it is safe to access
	 * it directly. */
	if(*addr1 != val) {
		ret = STATUS_TRY_AGAIN;
		goto out;
	}

	/* Wake the specified number of threads. */
	while(count-- && !list_empty(&source->threads)) {
		thread = list_first(&source->threads, thread_t, wait_link);
		thread_wake(thread);
		woken++;
	}

	if(source != dest) {
		/* Now move the remaining threads onto the destination list. */
		LIST_FOREACH_SAFE(&source->threads, iter) {
			thread = list_entry(iter, thread_t, wait_link);

			/* We don't need to lock the thread here. The members
			 * we are changing are only touched when interrupting
			 * threads under protection of wait_lock (which is the
			 * source lock). If the thread is currently being
			 * interrupted by another CPU, it may be waiting to get
			 * the wait lock. There is a special handling in
			 * thread.c to handle the wait lock having changed once
			 * it manages to acquire it. */
			assert(thread->wait_lock == &source->lock);
			thread->wait_lock = &dest->lock;
			list_append(&dest->threads, &thread->wait_link);
		}
	}

out:
	spinlock_unlock(&source->lock);
	if(source != dest)
		spinlock_unlock(&dest->lock);

	futex_finish(addr2);
	futex_finish(addr1);

	/* Store the number of woken threads if requested. */
	if(ret == STATUS_SUCCESS && wokenp)
		ret = write_user(wokenp, woken);

	return ret;
}

/** Initialize the futex cache. */
static __init_text void futex_init(void) {
	futex_cache = object_cache_create("futex_cache", futex_t, futex_ctor,
		NULL, NULL, 0, MM_BOOT);
}

INITCALL(futex_init);
