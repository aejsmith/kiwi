/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Futex implementation.
 *
 * Reference:
 *  - Fuss, Futexes and Furwocks: Fast Userlevel Locking in Linux
 *    http://www.kernel.org/doc/ols/2002/ols2002-pages-479-495.pdf
 *  - Futexes are Tricky
 *    http://dept-info.labri.fr/~denis/Enseignement/2008-IR/Articles/01-futex.pdf
 *
 * TODO:
 *  - We should restrict what type of memory futexes can be placed in. For
 *    example, it makes little sense to allow one to be placed in a device's
 *    memory.
 *  - Use a hash table instead of an AVL tree for the global table?
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
    phys_ptr_t phys;                /**< Physical address of futex. */
    refcount_t count;               /**< Number of processes referring to the futex. */
    spinlock_t lock;                /**< Lock for the futex. */
    list_t waiters;                 /**< List of threads waiting on the futex. */
    avl_tree_node_t tree_link;      /**< Link to global futex tree. */
} futex_t;

/** Structure linking a futex to a process. */
typedef struct futex_link {
    futex_t *futex;                 /**< Futex. */
    avl_tree_node_t node;           /**< AVL tree node. */
} futex_link_t;

/** Thread wait state for a futex. */
typedef struct futex_waiter {
    list_t link;
    thread_t *thread;

    /**
     * Futex that is being waited on. This can change while sleeping via
     * kern_futex_requeue().
     */
    futex_t *futex;
} futex_waiter_t;

/** Futex allocator. */
static slab_cache_t *futex_cache;

/** Tree of all futexes, keyed by physical address. */
static AVL_TREE_DEFINE(futex_tree);
static MUTEX_DEFINE(futex_tree_lock, 0);

static void futex_ctor(void *obj, void *data) {
    futex_t *futex = obj;

    spinlock_init(&futex->lock, "futex_lock");
    list_init(&futex->waiters);
}

/** Cleans up a process' futexes.
 * @param proc          Process to clean up. */
void futex_process_cleanup(process_t *proc) {
    mutex_lock(&futex_tree_lock);

    avl_tree_foreach_safe(&proc->futexes, iter) {
        futex_link_t *link = avl_tree_entry(iter, futex_link_t, node);
        futex_t *futex     = link->futex;

        avl_tree_remove(&proc->futexes, &link->node);
        kfree(link);

        /* If no more processes refer to the futex we can free it. */
        if (refcount_dec(&futex->count) == 0) {
            avl_tree_remove(&futex_tree, &futex->tree_link);
            slab_cache_free(futex_cache, futex);
        }
    }

    mutex_unlock(&futex_tree_lock);
}

/** Looks up a futex.
 * @param addr          Virtual address in current process.
 * @param _futex        Where to store pointer to futex structure.
 * @return              STATUS_SUCCESS on success, STATUS_INVALID_ADDR or
 *                      STATUS_ACCESS_DENIED if the address is invalid or is
 *                      not writeable. */
static status_t futex_lookup(int32_t *addr, futex_t **_futex) {
    /* Check if the address is 4 byte aligned. This will ensure that the address
     * does not cross a page boundary because page sizes are powers of 2. */
    if (!addr || (ptr_t)addr % sizeof(int32_t))
        return STATUS_INVALID_ARG;

    /* Get the page containing the address and the offset within it. */
    ptr_t base   = round_down((ptr_t)addr, PAGE_SIZE);
    ptr_t offset = (ptr_t)addr - base;

    /* Lock the page for read and write access and look up the physical address
     * of it. */
    phys_ptr_t phys;
    status_t ret = vm_lock_page(curr_proc->aspace, base, VM_ACCESS_READ | VM_ACCESS_WRITE, &phys);
    if (ret != STATUS_SUCCESS)
        return ret;

    phys += offset;

    mutex_lock(&curr_proc->lock);

    /* Firstly try to find the futex in the process' tree. This is quicker than
     * searching the global tree (after the first access to the futex by the
     * process), as a process will only use a small subset of all of the futexes
     * in the system. */
    futex_link_t *link = avl_tree_lookup(&curr_proc->futexes, phys, futex_link_t, node);
    if (!link) {
        mutex_lock(&futex_tree_lock);

        /* Use the global tree. */
        futex_t *futex = avl_tree_lookup(&futex_tree, phys, futex_t, tree_link);
        if (!futex) {
            /* Couldn't find it, this means that this is the first access to
             * this futex. Create a structure for it. */
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

    *_futex = link->futex;
    return STATUS_SUCCESS;
}

/** Unlocks the page containing a futex. */
static void futex_finish(int32_t *addr) {
    ptr_t base;

    base = round_down((ptr_t)addr, PAGE_SIZE);
    vm_unlock_page(curr_proc->aspace, base);
}

/** Waits for a futex.
 * @param addr          Pointer to futex.
 * @param val           Value of the futex prior to the call. This is needed to
 *                      prevent a race condition if another thread modifies the
 *                      futex before this thread has gone to sleep. If the
 *                      value has changed just before sleeping, the function
 *                      will return STATUS_TRY_AGAIN.
 * @param timeout       Timeout in nanoseconds. If -1, the function will block
 *                      until woken by a call to futex_wake(). If 0, an error
 *                      will be returned immediately.
 * @return              Status code describing result of the operation. */
status_t kern_futex_wait(int32_t *addr, int32_t val, nstime_t timeout) {
    status_t ret;

    futex_t *futex;
    ret = futex_lookup(addr, &futex);
    if (ret != STATUS_SUCCESS)
        return ret;

    spinlock_lock(&futex->lock);

    /* Now check the value to see if it has changed (see parameter description
     * above). The page is locked meaning it is safe to access it directly. */
    if (*addr == val) {
        futex_waiter_t waiter;
        waiter.thread = curr_thread;
        waiter.futex  = futex;

        list_init(&waiter.link);
        list_append(&futex->waiters, &waiter.link);

        /* The futex can change while waiting due to kern_futex_requeue(). Don't
         * relock the same lock upon return and instead do it manually with the
         * current futex. */
        ret = thread_sleep(&futex->lock, timeout, "futex", SLEEP_INTERRUPTIBLE | __SLEEP_NO_RELOCK);

        spinlock_lock(&waiter.futex->lock);
        list_remove(&waiter.link);
        spinlock_unlock(&waiter.futex->lock);
    } else {
        spinlock_unlock(&futex->lock);
        ret = STATUS_TRY_AGAIN;
    }

    futex_finish(addr);
    return ret;
}

static size_t wake_threads(futex_t *futex, size_t count) {
    size_t woken = 0;
    while (woken < count && !list_empty(&futex->waiters)) {
        futex_waiter_t *waiter = list_first(&futex->waiters, futex_waiter_t, link);
        list_remove(&waiter->link);

        /* Don't count threads that failed sleep but hadn't removed themselves
         * yet. */
        if (thread_wake(waiter->thread))
            woken++;
    }

    return woken;
}

/** Wakes up threads waiting on a futex.
 * @param addr          Pointer to futex.
 * @param count         Number of threads to attempt to wake.
 * @param _woken        Where to store number of threads actually woken.
 * @return              Status code describing result of the operation. */
status_t kern_futex_wake(int32_t *addr, size_t count, size_t *_woken) {
    if (!count)
        return STATUS_INVALID_ARG;

    futex_t *futex;
    status_t ret = futex_lookup(addr, &futex);
    if (ret != STATUS_SUCCESS)
        return ret;

    spinlock_lock(&futex->lock);

    size_t woken = wake_threads(futex, count);

    spinlock_unlock(&futex->lock);
    futex_finish(addr);

    /* Store the number of woken threads if requested. */
    return (_woken) ? write_user(_woken, woken) : STATUS_SUCCESS;
}

/**
 * Wakes up the specified number of threads from the source futex, and moves
 * all the remaining waiting threads to the wait queue of the destination
 * futex.
 *
 * @param addr1         Pointer to source futex.
 * @param val           Value of the source futex prior to the call. This is
 *                      needed to prevent a race condition if another thread
 *                      modifies the futex prior to waking. If the value has
 *                      changed, the function will return STATUS_TRY_AGAIN.
 * @param count         Number of threads to wake.
 * @param addr2         Pointer to destination futex.
 * @param _woken        Where to store number of woken threads.
 *
 * @return              Status code describing the result of the operation.
 */
status_t kern_futex_requeue(int32_t *addr1, int32_t val, size_t count, int32_t *addr2, size_t *_woken) {
    status_t ret;

    if (!count)
        return STATUS_INVALID_ARG;

    futex_t *source;
    ret = futex_lookup(addr1, &source);
    if (ret != STATUS_SUCCESS)
        return ret;

    futex_t *dest;
    ret = futex_lookup(addr2, &dest);
    if (ret != STATUS_SUCCESS) {
        futex_finish(addr1);
        return ret;
    }

    /* Another thread could potentially be performing a requeue with source and
     * dest swapped. Avoid deadlock by locking the futex with the lowest address
     * first. */
    if (source <= dest) {
        spinlock_lock(&source->lock);
        if (source != dest)
            spinlock_lock(&dest->lock);
    } else {
        spinlock_lock(&dest->lock);
        spinlock_lock(&source->lock);
    }

    /* Now check the value to see if it has changed (see parameter description
     * above). The page is locked meaning it is safe to access it directly. */
    if (*addr1 != val) {
        ret = STATUS_TRY_AGAIN;
        goto out;
    }

    /* Wake the specified number of threads. */
    size_t woken = wake_threads(source, count);

    if (source != dest) {
        /* Now move the remaining threads onto the destination list. */
        list_foreach_safe(&source->waiters, iter) {
            futex_waiter_t *waiter = list_entry(iter, futex_waiter_t, link);
            list_append(&dest->waiters, &waiter->link);
            waiter->futex = dest;
        }
    }

out:
    spinlock_unlock(&source->lock);
    if (source != dest)
        spinlock_unlock(&dest->lock);

    futex_finish(addr2);
    futex_finish(addr1);

    /* Store the number of woken threads if requested. */
    if (ret == STATUS_SUCCESS && _woken)
        ret = write_user(_woken, woken);

    return ret;
}

/** Initializes the futex cache. */
static __init_text void futex_init(void) {
    futex_cache = object_cache_create(
        "futex_cache",
        futex_t, futex_ctor, NULL, NULL, 0, MM_BOOT);
}

INITCALL(futex_init);
