/*
 * Copyright (C) 2008-2012 Alex Smith
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
 * @brief		Semaphore implementation.
 *
 * @todo		Some events for semaphore objects, such as the count
 *			going above 0.
 */

#include <kernel/semaphore.h>

#include <lib/avl_tree.h>
#include <lib/id_allocator.h>
#include <lib/refcount.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/process.h>

#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <kernel.h>
#include <object.h>
#include <status.h>

#if 0

/** Structure containing a userspace semaphore. */
typedef struct user_semaphore {
	object_t obj;			/**< Object header. */

	semaphore_t sem;		/**< Real semaphore. */
	refcount_t count;		/**< Number of handles to the semaphore. */
	semaphore_id_t id;		/**< ID of the semaphore. */
	char *name;			/**< Name of the semaphore. */
	avl_tree_node_t tree_link;	/**< Link to semaphore tree. */
} user_semaphore_t;

/** User semaphore ID allocator. */
static id_allocator_t semaphore_id_allocator;

/** Tree of user semaphores. */
static AVL_TREE_DECLARE(semaphore_tree);
static RWLOCK_DECLARE(semaphore_tree_lock);

#endif

/** Down a semaphore.
 * @param sem		Semaphore to down.
 * @param timeout	Timeout in microseconds. If SYNC_ABSOLUTE is specified,
 *			will always be taken to be a system time at which the
 *			sleep will time out. Otherwise, taken as the number of
 *			microseconds in which the sleep will time out. If 0 is
 *			specified, the function will return an error immediately
 *			if the lock cannot be acquired immediately. If -1
 *			is specified, the thread will sleep indefinitely until
 *			the semaphore can be downed or it is interrupted.
 * @param flags		Synchronization flags.
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set. */
status_t semaphore_down_etc(semaphore_t *sem, useconds_t timeout, int flags) {
	spinlock_lock(&sem->lock);

	if(sem->count) {
		--sem->count;
		spinlock_unlock(&sem->lock);
		return STATUS_SUCCESS;
	}

	list_append(&sem->threads, &curr_thread->wait_link);
	return thread_sleep(&sem->lock, timeout, sem->name, flags);
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
	thread_t *thread;
	size_t i;

	spinlock_lock(&sem->lock);

	for(i = 0; i < count; i++) {
		if(list_empty(&sem->threads)) {
			sem->count++;
		} else {
			thread = list_first(&sem->threads, thread_t, wait_link);
			thread_wake(thread);
		}
	}

	spinlock_unlock(&sem->lock);
}

/** Initialize a semaphore structure.
 * @param sem		Semaphore to initialize.
 * @param name		Name of the semaphore, for debugging purposes.
 * @param initial	Initial value of the semaphore. */
void semaphore_init(semaphore_t *sem, const char *name, size_t initial) {
	spinlock_init(&sem->lock, "semaphore_lock");
	list_init(&sem->threads);
	sem->count = initial;
	sem->name = name;
}

#if 0

/** Release a user semaphore.
 * @param sem		Semaphore to release. */
static void user_semaphore_release(user_semaphore_t *sem) {
	if(refcount_dec(&sem->count) == 0) {
		rwlock_write_lock(&semaphore_tree_lock);
		avl_tree_remove(&semaphore_tree, &sem->tree_link);
		rwlock_unlock(&semaphore_tree_lock);

		object_destroy(&sem->obj);
		id_allocator_free(&semaphore_id_allocator, sem->id);
		kfree(sem);
	}
}

/** Close a handle to a semaphore object.
 * @param handle	Handle being closed. */
static void semaphore_object_close(object_handle_t *handle) {
	user_semaphore_release((user_semaphore_t *)handle->object);
}

/** Semaphore object type. */
static object_type_t semaphore_object_type = {
	.id = OBJECT_TYPE_SEMAPHORE,
	.close = semaphore_object_close,
};

/** Create a new semaphore.
 * @param name		Optional name for the semaphore, for debugging purposes.
 * @param count		Initial count of the semaphore.
 * @param handlep	Where to store handle to the semaphore.
 * @return		Status code describing result of the operation. */
status_t kern_semaphore_create(const char *name, size_t count, handle_t *handlep) {
	user_semaphore_t *sem;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	sem = kmalloc(sizeof(user_semaphore_t), MM_WAIT);
	sem->id = id_allocator_alloc(&semaphore_id_allocator);
	if(sem->id < 0) {
		kfree(sem);
		return STATUS_NO_SEMAPHORES;
	}
	if(name) {
		ret = strndup_from_user(name, SEMAPHORE_NAME_MAX, &sem->name);
		if(ret != STATUS_SUCCESS) {
			id_allocator_free(&semaphore_id_allocator, sem->id);
			kfree(sem);
			return ret;
		}
	} else {
		sem->name = NULL;
	}

	object_init(&sem->obj, &semaphore_object_type);
	semaphore_init(&sem->sem, (sem->name) ? sem->name : "user_semaphore", count);
	refcount_set(&sem->count, 1);

	rwlock_write_lock(&semaphore_tree_lock);
	avl_tree_insert(&semaphore_tree, &sem->tree_link, sem->id, sem);
	rwlock_unlock(&semaphore_tree_lock);

	ret = object_handle_create(&sem->obj, NULL, 0, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS)
		user_semaphore_release(sem);

	return ret;
}

/** Open a handle to a semaphore.
 * @param id		ID of the semaphore to open.
 * @param handlep	Where to store handle to semaphore.
 * @return		Status code describing result of the operation. */
status_t kern_semaphore_open(semaphore_id_t id, handle_t *handlep) {
	user_semaphore_t *sem;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	rwlock_read_lock(&semaphore_tree_lock);

	sem = avl_tree_lookup(&semaphore_tree, id);
	if(!sem) {
		rwlock_unlock(&semaphore_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&sem->count);
	rwlock_unlock(&semaphore_tree_lock);

	ret = object_handle_open(&sem->obj, NULL, 0, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS)
		user_semaphore_release(sem);

	return ret;
}

/** Get the ID of a semaphore.
 * @param handle	Handle to semaphore to get ID of.
 * @return		ID of semaphore on success, -1 if handle is invalid. */
semaphore_id_t kern_semaphore_id(handle_t handle) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	semaphore_id_t ret;

	if(object_handle_lookup(handle, OBJECT_TYPE_SEMAPHORE, 0, &khandle) != STATUS_SUCCESS)
		return -1;

	sem = (user_semaphore_t *)khandle->object;
	ret = sem->id;
	object_handle_release(khandle);
	return ret;
}

/**
 * Down (decrease the count of) a semaphore.
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
 * @return		Status code describing result of the operation.
 */
status_t kern_semaphore_down(handle_t handle, useconds_t timeout) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_SEMAPHORE, 0, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	sem = (user_semaphore_t *)khandle->object;
	ret = semaphore_down_etc(&sem->sem, timeout, SYNC_INTERRUPTIBLE);
	object_handle_release(khandle);
	return ret;
}

/**
 * Up (increase the count of) a semaphore.
 *
 * Increases the count of a semaphore by the specified number. This can cause
 * waiting threads to be woken.
 *
 * @param handle	Handle to semaphore to up.
 * @param count		Value to up the semaphore by.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_semaphore_up(handle_t handle, size_t count) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_SEMAPHORE, 0, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	sem = (user_semaphore_t *)khandle->object;
	semaphore_up(&sem->sem, count);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Initialize the semaphore ID allocator. */
static __init_text void semaphore_id_init(void) {
	id_allocator_init(&semaphore_id_allocator, 65535, MM_BOOT);
}

INITCALL(semaphore_id_init);

#endif
