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

#include <kernel/semaphore.h>

#include <lib/avl_tree.h>
#include <lib/refcount.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/process.h>

#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <console.h>
#include <init.h>
#include <kdbg.h>
#include <object.h>
#include <status.h>
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
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set. */
status_t semaphore_down_etc(semaphore_t *sem, useconds_t timeout, int flags) {
	bool state;

	state = waitq_sleep_prepare(&sem->queue);
	if(sem->count) {
		--sem->count;
		waitq_sleep_cancel(&sem->queue, state);
		return STATUS_SUCCESS;
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

/** Release a user semaphore.
 * @param sem		Semaphore to release. */
static void user_semaphore_release(user_semaphore_t *sem) {
	if(refcount_dec(&sem->count) == 0) {
		rwlock_write_lock(&semaphore_tree_lock);
		avl_tree_remove(&semaphore_tree, sem->id);
		rwlock_unlock(&semaphore_tree_lock);

		object_destroy(&sem->obj);
		vmem_free(semaphore_id_arena, sem->id, 1);
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
 * @param security	Security attributes for the ACL. If NULL, default
 *			attributes will be constructed which grant full access
 *			to the semaphore to the calling process' user.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to the semaphore.
 * @return		Status code describing result of the operation. */
status_t sys_semaphore_create(const char *name, size_t count, const object_security_t *security,
                              object_rights_t rights, handle_t *handlep) {
	object_security_t ksecurity = { -1, -1, NULL };
	user_semaphore_t *sem;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	if(security) {
		ret = object_security_from_user(&ksecurity, security, true);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	/* Construct a default ACL if required. */
	if(!ksecurity.acl) {
		ksecurity.acl = kmalloc(sizeof(*ksecurity.acl), MM_SLEEP);
		object_acl_init(ksecurity.acl);
		object_acl_add_entry(ksecurity.acl, ACL_ENTRY_USER, -1,
		                     OBJECT_SET_ACL | SEMAPHORE_USAGE);
	}

	sem = kmalloc(sizeof(user_semaphore_t), MM_SLEEP);
	sem->id = vmem_alloc(semaphore_id_arena, 1, 0);
	if(!sem->id) {
		kfree(sem);
		object_security_destroy(&ksecurity);
		return STATUS_NO_SEMAPHORES;
	}
	if(name) {
		ret = strndup_from_user(name, SEMAPHORE_NAME_MAX, &sem->name);
		if(ret != STATUS_SUCCESS) {
			vmem_free(semaphore_id_arena, sem->id, 1);
			kfree(sem);
			object_security_destroy(&ksecurity);
			return ret;
		}
	} else {
		sem->name = NULL;
	}

	object_init(&sem->obj, &semaphore_object_type, &ksecurity, NULL);
	object_security_destroy(&ksecurity);
	semaphore_init(&sem->sem, (sem->name) ? sem->name : "user_semaphore", count);
	refcount_set(&sem->count, 1);

	rwlock_write_lock(&semaphore_tree_lock);
	avl_tree_insert(&semaphore_tree, sem->id, sem, NULL);
	rwlock_unlock(&semaphore_tree_lock);

	ret = object_handle_create(&sem->obj, NULL, rights, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		user_semaphore_release(sem);
	}
	return ret;
}

/** Open a handle to a semaphore.
 * @param id		ID of the semaphore to open.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to semaphore.
 * @return		Status code describing result of the operation. */
status_t sys_semaphore_open(semaphore_id_t id, object_rights_t rights, handle_t *handlep) {
	user_semaphore_t *sem;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&semaphore_tree_lock);

	sem = avl_tree_lookup(&semaphore_tree, id);
	if(!sem) {
		rwlock_unlock(&semaphore_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&sem->count);
	rwlock_unlock(&semaphore_tree_lock);

	ret = object_handle_create(&sem->obj, NULL, rights, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		user_semaphore_release(sem);
	}
	return ret;
}

/** Get the ID of a semaphore.
 * @param handle	Handle to semaphore to get ID of.
 * @return		ID of semaphore on success, -1 if handle is invalid. */
semaphore_id_t sys_semaphore_id(handle_t handle) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	semaphore_id_t ret;

	if(object_handle_lookup(NULL, handle, OBJECT_TYPE_SEMAPHORE, 0, &khandle) != STATUS_SUCCESS) {
		return -1;
	}

	sem = (user_semaphore_t *)khandle->object;
	ret = sem->id;
	object_handle_release(khandle);
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
 * @return		Status code describing result of the operation.
 */
status_t sys_semaphore_down(handle_t handle, useconds_t timeout) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_SEMAPHORE, SEMAPHORE_USAGE, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	sem = (user_semaphore_t *)khandle->object;
	ret = semaphore_down_etc(&sem->sem, timeout, SYNC_INTERRUPTIBLE);
	object_handle_release(khandle);
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
 * @return		Status code describing result of the operation.
 */
status_t sys_semaphore_up(handle_t handle, size_t count) {
	object_handle_t *khandle;
	user_semaphore_t *sem;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_SEMAPHORE, SEMAPHORE_USAGE, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	sem = (user_semaphore_t *)khandle->object;
	semaphore_up(&sem->sem, count);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Initialise the semaphore ID allocator. */
static void __init_text semaphore_id_init(void) {
	semaphore_id_arena = vmem_create("semaphore_id_arena", 1, 65535, 1, NULL,
	                                 NULL, NULL, 0, 0, 0, MM_FATAL);
}
INITCALL(semaphore_id_init);
