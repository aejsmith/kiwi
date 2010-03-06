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
 * @brief		Kernel object manager.
 *
 * The kernel object manager manages all userspace-accessible objects. It
 * allows processes (as well as the kernel) to create handles to objects, and
 * implements an access control list that limits which processes can access
 * objects.
 *
 * It does not, however, manage how objects are referred to (i.e. there isn't
 * a single namespace for all objects - for example FS entries are referred to
 * by path strings, but ports, memory areas, etc. are referred to by global
 * IDs), or the lifetime of objects - it is up to each object type to manage
 * these.
 *
 * @todo		Implement the ACL.
 */

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <sync/semaphore.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <object.h>

#if CONFIG_OBJECT_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Object waiting synchronization information structure. */
typedef struct object_wait_sync {
	semaphore_t sem;		/**< Semaphore counting events. */
	object_wait_t *event;		/**< Handle that received first event (atomically set). */
} object_wait_sync_t;

/** Cache for object handle structures. */
static slab_cache_t *object_handle_cache;

/** Initialise an object structure.
 * @param obj		Object to initialise.
 * @param type		Pointer to type structure for object type.
 * @param flags		Behaviour flags for the object. */
void object_init(object_t *obj, object_type_t *type, int flags) {
	if(flags & OBJECT_MAPPABLE) {
		assert(type->fault || type->page_get);
	}
	obj->type = type;
	obj->flags = flags;
}

/** Destroy an object structure.
 * @param obj		Object to destroy. */
void object_destroy(object_t *obj) {
	/* TODO: Clean ACL. */
}

/** Create a handle to an object.
 *
 * Creates a new handle to an object. The handle will have a reference count
 * of one. When it is no longer required, object_handle_release() should be
 * called on it. The handle will not be attached to any process - to attach
 * it to a process, use handle_table_add() on the process' handle table.
 *
 * @param obj		Object to create handle to.
 * @param data		Data pointer for the object.
 *
 * @return		Pointer to handle structure.
 */
object_handle_t *object_handle_create(object_t *obj, void *data) {
	object_handle_t *handle;

	assert(obj);

	handle = slab_cache_alloc(object_handle_cache, MM_SLEEP);
	refcount_set(&handle->count, 1);
	handle->object = obj;
	handle->data = data;
	return handle;
}

/** Increase the reference count of a handle.
 *
 * Increases the reference count of an object handle to signal that it is being
 * used. When the handle is no longer needed it should be released with
 * object_handle_release().
 * 
 * @param handle	Handle to increase reference count of.
 */
void object_handle_get(object_handle_t *handle) {
	assert(handle);
	refcount_inc(&handle->count);
}

/** Release a handle.
 *
 * Decreases the reference count of a handle. If no more references remain to
 * the handle, it will be destroyed.
 *
 * @param handle	Handle to release.
 */
void object_handle_release(object_handle_t *handle) {
	assert(handle);

	/* If there are no more references we can close it. */
	if(refcount_dec(&handle->count) == 0) {
		if(handle->object->type->close) {
			handle->object->type->close(handle);
		}
		slab_cache_free(object_handle_cache, handle);
	}
}

/** Insert a handle into a process' handle table.
 *
 * Allocates a handle ID for a process and adds a handle to it. On success,
 * the handle will have an extra reference on it.
 *
 * @param process	Process to attach into.
 * @param handle	Handl to attach.
 *
 * @return		Handle ID on success, negative error code on failure.
 */
handle_t object_handle_attach(process_t *process, object_handle_t *handle) {
	int ret;

	assert(process);
	assert(handle);

	rwlock_write_lock(&process->handles.lock);

	/* Find a handle ID in the table. */
	if((ret = bitmap_ffz(&process->handles.bitmap)) == -1) {
		rwlock_unlock(&process->handles.lock);
		return -ERR_RESOURCE_UNAVAIL;
	}

	/* Set the bit and add the handle to the tree. */
	object_handle_get(handle);
	bitmap_set(&process->handles.bitmap, ret);
	avl_tree_insert(&process->handles.tree, (key_t)ret, handle, NULL);

	dprintf("object: allocated handle %d in process %" PRId32 " (object: %p, data: %p)\n",
	        ret, process->id, handle->object, handle->data);
	rwlock_unlock(&process->handles.lock);
	return ret;
}

/** Detach a handle from a process.
 *
 * Removes the specified handle ID from a process' handle table and releases
 * the handle.
 *
 * @param process	Process to remove from.
 * @param id		ID of handle to detach.
 *
 * @return		0 on success, negative error code on failure.
 */
int object_handle_detach(process_t *process, handle_t id) {
	object_handle_t *handle;

	assert(process);

	rwlock_write_lock(&process->handles.lock);

	/* Look up the handle in the tree. */
	if(!(handle = avl_tree_lookup(&process->handles.tree, (key_t)id))) {
		rwlock_unlock(&process->handles.lock);
		return -ERR_NOT_FOUND;
	}

	/* Remove from the tree and mark the ID as free. */
	avl_tree_remove(&process->handles.tree, (key_t)id);
	bitmap_clear(&process->handles.bitmap, id);
	object_handle_release(handle);

	dprintf("object: detached handle %" PRId32 " from process %" PRId32 "\n",
	        id, process->id);
	rwlock_unlock(&process->handles.lock);
	return 0;
}

/** Look up a handle in a process' handle table.
 *
 * Looks up the handle with the given ID in a process' handle table, ensuring
 * that the object is a certain type. The returned handle will have an extra
 * reference on it - when it is no longer needed, it should be released with
 * object_handle_release().
 *
 * @param process	Process to look up in.
 * @param id		Handle ID to look up.
 * @param type		Required object type ID (if negative, no type checking
 *			will be performed).
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int object_handle_lookup(process_t *process, handle_t id, int type, object_handle_t **handlep) {
	object_handle_t *handle;

	assert(process);
	assert(handlep);

	rwlock_read_lock(&process->handles.lock);

	/* Look up the handle in the tree. */
	if(!(handle = avl_tree_lookup(&process->handles.tree, (key_t)id))) {
		rwlock_unlock(&process->handles.lock);
		return -ERR_NOT_FOUND;
	}

	object_handle_get(handle);

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && handle->object->type->id != type) {
		object_handle_release(handle);
		rwlock_unlock(&process->handles.lock);
		return -ERR_TYPE_INVAL;
	}

	*handlep = handle;
	rwlock_unlock(&process->handles.lock);
	return 0;
}

/** Notifier function to use for object waiting.
 * @param arg1		Unused.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
void object_wait_notifier(void *arg1, void *arg2, void *arg3) {
	object_wait_callback(arg3);
}

/** Object waiting callback function.
 * @param wait		Wait information structure. */
void object_wait_callback(object_wait_t *wait) {
	object_wait_sync_t *sync = wait->priv;

	__sync_bool_compare_and_swap(&sync->event, NULL, wait);
	semaphore_up(&sync->sem, 1);
}

/** Wait for an event to happen on an object.
 * @param handle	Handle to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			return immediately if the event has not happened, and
 *			a value of -1 will block indefinitely until the event
 *			happens.
 * @return		0 on success, negative error code on failure. */
int object_wait(object_handle_t *handle, int event, useconds_t timeout) {
	object_wait_sync_t sync;
	object_wait_t wait;
	int ret;

	if(!handle) {
		return -ERR_PARAM_INVAL;
	}

	semaphore_init(&sync.sem, "object_wait_sem", 0);
	sync.event = NULL;
	wait.handle = handle;
	wait.event = event;
	wait.priv = &sync;

	if(!handle->object->type->wait || !handle->object->type->unwait) {
		return -ERR_NOT_SUPPORTED;
	} else if((ret = handle->object->type->wait(&wait)) != 0) {
		return ret;
	}

	ret = semaphore_down_etc(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
	handle->object->type->unwait(&wait);
	return ret;
}

/** Wait for events to happen on multiple objects.
 * @param handles	Array of handles to wait for.
 * @param event		Array of event IDs to wait for (specific to object
 *			type). The index into the array selects the handle the
 *			event is for - for example, specifying 1 as the event
 *			at index 1 will wait for event 1 on the handle at index
 *			1 in the handle array. If you wish to wait for multiple
 *			events on one handle, specify the handle multiple times
 *			in the arrays.
 * @param count		Number of handles.
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			return immediately if none of the events have happened,
 *			and a value of -1 will block indefinitely until one of
 *			the events happen.
 * @return		Index of event that occurred on success, negative error
 *			code on failure. */
int object_wait_multiple(object_handle_t **handles, int *events, size_t count, useconds_t timeout) {
	object_wait_sync_t sync;
	object_wait_t *waits;
	size_t i;
	int ret;

	if(!count || count > 1024 || !handles || !events) {
		return -ERR_PARAM_INVAL;
	}

	semaphore_init(&sync.sem, "object_wait_sem", 0);
	sync.event = NULL;

	/* Allocate wait structures for each handle and fill them in. */
	waits = kmalloc(sizeof(object_wait_t) * count, MM_SLEEP);
	for(i = 0; i < count; i++) {
		if(!handles[i]) {
			ret = -ERR_PARAM_INVAL;
			goto out;
		} else if(!handles[i]->object->type->wait || !handles[i]->object->type->unwait) {
			ret = -ERR_NOT_SUPPORTED;
			goto out;
		}

		waits[i].handle = handles[i];
		waits[i].event = events[i];
		waits[i].priv = &sync;
		waits[i].idx = i;

		if((ret = handles[i]->object->type->wait(&waits[i])) != 0) {
			goto out;
		}
	}

	ret = semaphore_down_etc(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
out:
	for(i = 0; i < count; i++) {
		handles[i]->object->type->unwait(&waits[i]);
	}
	if(ret == 0) {
		assert(sync.event);
		ret = sync.event->idx;
	}
	kfree(waits);
	return ret;
}

/** Initialises a handle table.
 *
 * Initialises a handle table structure and duplicates inheritable handles
 * from another handle table into it if required.
 *
 * @todo		Duplicate handles.
 *
 * @param table		Table to initialise.
 * @param parent	Parent process' handle table.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_table_init(handle_table_t *table, handle_table_t *parent) {
	avl_tree_init(&table->tree);
	bitmap_init(&table->bitmap, CONFIG_HANDLE_MAX, NULL, MM_SLEEP);
	rwlock_init(&table->lock, "handle_table_lock");
	return 0;
}

/** Destroy a handle table.
 * @param table		Table being destroyed. */
void handle_table_destroy(handle_table_t *table) {
	object_handle_t *handle;

	/* Close all handles. */
	AVL_TREE_FOREACH_SAFE(&table->tree, iter) {
		handle = avl_tree_entry(iter, object_handle_t);
		object_handle_release(handle);
		avl_tree_remove(&table->tree, iter->key);
	}

	bitmap_destroy(&table->bitmap);
}

/** Initialise the object handle cache. */
void __init_text handle_cache_init(void) {
	object_handle_cache = slab_cache_create("object_handle_cache", sizeof(object_handle_t),
	                                        0, NULL, NULL, NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                                        NULL, 0, MM_FATAL);
}

/** Get the type of an object referred to by a handle.
 * @param handle	Handle to object.
 * @return		Type ID of object. */
int sys_object_type(handle_t handle) {
	object_handle_t *obj;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) == 0) {
		ret = obj->object->type->id;
		object_handle_release(obj);
	}
	return ret;
}

/** Wait for an event to happen on an object.
 * @param handle	Handle ID to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			return immediately if the event has not happened, and
 *			a value of -1 will block indefinitely until the event
 *			happens.
 * @return		0 on success, negative error code on failure. */
int sys_object_wait(handle_t handle, int event, useconds_t timeout) {
	object_handle_t *obj;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) != 0) {
		return ret;
	}

	ret = object_wait(obj, event, timeout);
	object_handle_release(obj);
	return ret;
}

/** Wait for events to happen on multiple objects.
 * @param handles	Array of handle IDs to wait for.
 * @param event		Array of event IDs to wait for (specific to object
 *			type). The index into the array selects the handle the
 *			event is for - for example, specifying 1 as the event
 *			at index 1 will wait for event 1 on the handle at index
 *			1 in the handle array. If you wish to wait for multiple
 *			events on one handle, specify the handle multiple times
 *			in the arrays.
 * @param count		Number of handles.
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			return immediately if none of the events have happened,
 *			and a value of -1 will block indefinitely until one of
 *			the events happen.
 * @return		Index of event that occurred on success, negative error
 *			code on failure. */
int sys_object_wait_multiple(handle_t *handles, int *events, size_t count, useconds_t timeout) {
	object_handle_t **kobjs = NULL;
	handle_t *khandles = NULL;
	int *kevents = NULL;
	size_t i;
	int ret;

	if(!count || count > 1024 || !handles || !events) {
		return -ERR_PARAM_INVAL;
	}

	khandles = kmalloc(sizeof(handle_t) * count, MM_SLEEP);
	if((ret = memcpy_from_user(khandles, handles, sizeof(handle_t) * count)) != 0) {
		goto out;
	}

	kevents = kmalloc(sizeof(int) * count, MM_SLEEP);
	if((ret = memcpy_from_user(kevents, events, sizeof(int) * count)) != 0) {
		goto out;
	}

	kobjs = kcalloc(count, sizeof(object_handle_t *), MM_SLEEP);
	for(i = 0; i < count; i++) {
		if((ret = object_handle_lookup(curr_proc, khandles[i], -1, &kobjs[i])) != 0) {
			goto out;
		}
	}

	ret = object_wait_multiple(kobjs, kevents, count, timeout);
out:
	if(kobjs) {
		for(i = 0; i < count; i++) {
			if(!kobjs[i]) {
				break;
			}
			object_handle_release(kobjs[i]);
		}
	}
	if(kevents) {
		kfree(kevents);
	}
	if(khandles) {
		kfree(khandles);
	}
	return ret;
}

/** Close a handle.
 * @param handle	Handle ID to close.
 * @return		0 on success, negative error code on failure. */
int sys_handle_close(handle_t handle) {
	return object_handle_detach(curr_proc, handle);
}
