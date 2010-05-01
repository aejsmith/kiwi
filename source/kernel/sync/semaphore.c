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
 * @brief		Semaphore implementation.
 *
 * @todo		Some events for semaphore objects, such as the count
 *			going above 0.
 */

#include <cpu/intr.h>

#include <lib/avl_tree.h>
#include <lib/refcount.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/process.h>

#include <public/semaphore.h>

#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <console.h>
#include <errors.h>
#include <init.h>
#include <kdbg.h>
#include <object.h>
#include <vmem.h>

/** Structure containing a userspace semaphore. */
typedef struct user_semaphore {
	object_t obj;			/**< Object header. */

	semaphore_t sem;		/**< Real semaphore. */
	refcount_t count;		/**< Number of handles to the semaphore. */
	semaphore_id_t id;		/**< ID of the semaphore. */
	char *name;			/**< Name of the semaphore. */
} user_semaphore_t;

/** Semaphore ID allocator. */
static vmem_t *semaphore_id_arena;

/** Tree of userspace semaphores. */
static AVL_TREE_DECLARE(semaphore_tree);
static RWLOCK_DECLARE(semaphore_tree_lock);

/** Down a semaphore.
 * @param sem		Semaphore to down.
 * @param timeout	Timeout in microseconds. A timeout of -1 will sleep
 *			forever until the count can be decreased, and a timeout
 *			of 0 will return an error immediately if unable to down
 *			the semaphore.
 * @param flags		Synchronization flags.
 * @return		0 on success, negative error code on failure. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set. */
int semaphore_down_etc(semaphore_t *sem, useconds_t timeout, int flags) {
	bool state;

	state = waitq_sleep_prepare(&sem->queue);
	if(sem->count) {
		--sem->count;
		spinlock_unlock_ni(&sem->queue.lock);
		intr_restore(state);
		return 0;
	}

	return waitq_sleep_unsafe(&sem->queue, timeout, flags, state);
}

/** Down a semaphore.
 * @param sem		Semaphore to down. */
void semaphore_down(semaphore_t *sem) {
	semaphore_down_etc(sem, -1, 0);
}

/** Up a semaphore.
 * @param sem		Semaphore to up.
 * @param count		Value to increment the count by. */
void semaphore_up(semaphore_t *sem, size_t count) {
	size_t i;

	spinlock_lock(&sem->queue.lock);
	for(i = 0; i < count; i++) {
		if(!waitq_wake_unsafe(&sem->queue)) {
			sem->count++;
		}
	}
	spinlock_unlock(&sem->queue.lock);
}

/** Initialise a semaphore structure.
 * @param sem		Semaphore to initialise.
 * @param name		Name of the semaphore, for debugging purposes.
 * @param initial	Initial value of the semaphore. */
void semaphore_init(semaphore_t *sem, const char *name, size_t initial) {
	waitq_init(&sem->queue, name);
	sem->count = initial;
}

/** Print a list of semaphores.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_semaphore(int argc, char **argv) {
	user_semaphore_t *sem;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_NONE, "Prints out a list of semaphore objects.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID    Name                 Refcount Count\n");
	kprintf(LOG_NONE, "==    ====                 ======== =====\n");

	AVL_TREE_FOREACH(&semaphore_tree, iter) {
		sem = avl_tree_entry(iter, user_semaphore_t);

		kprintf(LOG_NONE, "%-5" PRIu32 " %-20s %-8d %d\n", sem->id,
		        sem->sem.queue.name, refcount_get(&sem->count),
		        sem->sem.count);
	}

	return KDBG_OK;
}

/** Close a handle to a semaphore object.
 * @param handle	Handle being closed. */
static void semaphore_object_close(khandle_t *handle) {
	user_semaphore_t *sem = (user_semaphore_t *)handle->object;

	if(refcount_dec(&sem->count) == 0) {
		rwlock_write_lock(&semaphore_tree_lock);
		avl_tree_remove(&semaphore_tree, sem->id);
		rwlock_unlock(&semaphore_tree_lock);

		object_destroy(&sem->obj);
		vmem_free(semaphore_id_arena, sem->id, 1);
		kfree(sem);
	}
}

/** Semaphore object type. */
static object_type_t semaphore_object_type = {
	.id = OBJECT_TYPE_SEMAPHORE,
	.close = semaphore_object_close,
};

/** Create a new semaphore.
 * @param name		Optional name for the semaphore, for debugging purposes.
 * @param count		Initial count of the semaphore.
 * @return		Handle to the semaphore on success, negative error
 *			code on failure. */
handle_t sys_semaphore_create(const char *name, size_t count) {
	user_semaphore_t *sem;
	khandle_t *handle;
	handle_t ret;

	sem = kmalloc(sizeof(user_semaphore_t), MM_SLEEP);
	if(!(sem->id = vmem_alloc(semaphore_id_arena, 1, 0))) {
		kfree(sem);
		return -ERR_RESOURCE_UNAVAIL;
	}
	if(name) {
		if((ret = strdup_from_user(name, 0, &sem->name)) != 0) {
			vmem_free(semaphore_id_arena, sem->id, 1);
			kfree(sem);
			return ret;
		}
	} else {
		sem->name = NULL;
	}

	object_init(&sem->obj, &semaphore_object_type);
	semaphore_init(&sem->sem, (sem->name) ? sem->name : "user_semaphore", count);
	refcount_set(&sem->count, 1);

	handle_create(&sem->obj, NULL, NULL, 0, &handle);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	if(ret < 0) {
		/* The handle_release call frees the semaphore. */
		return ret;
	}

	rwlock_write_lock(&semaphore_tree_lock);
	avl_tree_insert(&semaphore_tree, sem->id, sem, NULL);
	rwlock_unlock(&semaphore_tree_lock);
	return ret;
}

/** Open a handle to a semaphore.
 * @param id		ID of the semaphore to open.
 * @return		Handle to the semaphore on success, negative error
 *			code on failure. */
handle_t sys_semaphore_open(semaphore_id_t id) {
	user_semaphore_t *sem;
	khandle_t *handle;
	handle_t ret;

	rwlock_read_lock(&semaphore_tree_lock);

	if(!(sem = avl_tree_lookup(&semaphore_tree, id))) {
		rwlock_unlock(&semaphore_tree_lock);
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&sem->count);
	rwlock_unlock(&semaphore_tree_lock);

	handle_create(&sem->obj, NULL, NULL, 0, &handle);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	return ret;
}

/** Get the ID of a semaphore.
 * @param handle	Handle to semaphore to get ID of.
 * @return		ID of semaphore on success, negative error code on
 *			failure. */
semaphore_id_t sys_semaphore_id(handle_t handle) {
	user_semaphore_t *sem;
	semaphore_id_t ret;
	khandle_t *khandle;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_SEMAPHORE, &khandle)) != 0) {
		return ret;
	}

	sem = (user_semaphore_t *)khandle->object;
	ret = sem->id;
	handle_release(khandle);
	return ret;
}

/** Down (decrease the count of) a semaphore.
 *
 * Attempts to decrease the count of a semaphore by 1. If the count of the
 * semaphore is currently 0, the function will block until another thread
 * ups the semaphore, or until the timeout expires.
 *
 * @param handle	Handle to semaphore to down.
 * @param timeout	Timeout in microseconds. A timeout of -1 will sleep
 *			forever until the count can be decreased, and a timeout
 *			of 0 will return an error immediately if unable to down
 *			the semaphore.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_semaphore_down(handle_t handle, useconds_t timeout) {
	user_semaphore_t *sem;
	khandle_t *khandle;
	int ret;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_SEMAPHORE, &khandle)) != 0) {
		return ret;
	}

	sem = (user_semaphore_t *)khandle->object;
	ret = semaphore_down_etc(&sem->sem, timeout, SYNC_INTERRUPTIBLE);
	handle_release(khandle);
	return ret;
}

/** Up (increase the count of) a semaphore.
 *
 * Increases the count of a semaphore by the specified number. This can cause
 * waiting threads to be woken.
 *
 * @param handle	Handle to semaphore to up.
 * @param count		Value to up the semaphore by.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_semaphore_up(handle_t handle, size_t count) {
	user_semaphore_t *sem;
	khandle_t *khandle;
	int ret;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_SEMAPHORE, &khandle)) != 0) {
		return ret;
	}

	sem = (user_semaphore_t *)khandle->object;
	semaphore_up(&sem->sem, count);
	handle_release(khandle);
	return 0;
}

/** Initialise the semaphore ID allocator. */
static void __init_text semaphore_id_init(void) {
	semaphore_id_arena = vmem_create("semaphore_id_arena", 1, 65535, 1, NULL,
	                                 NULL, NULL, 0, 0, MM_FATAL);
}
INITCALL(semaphore_id_init);
