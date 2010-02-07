/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Per-process object manager.
 */

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/handle.h>
#include <proc/process.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <init.h>
#include <kdbg.h>

#if CONFIG_HANDLE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Handle waiting synchronization information structure. */
typedef struct handle_wait_sync {
	semaphore_t sem;		/**< Semaphore counting events. */
	handle_wait_t *event;		/**< Handle that received first event (atomically set). */
} handle_wait_sync_t;

/** Slab cache for allocating handle information structures. */
static slab_cache_t *handle_info_cache;

/** Constructor for handle information structures.
 * @param obj		Object to construct.
 * @param data		Data parameter (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int handle_info_cache_ctor(void *obj, void *data, int kmflag) {
	handle_info_t *info = obj;

	refcount_set(&info->count, 0);
	rwlock_init(&info->lock, "handle_lock");
	return 0;
}

/** Notifier function to use for handle waiting.
 * @param arg1		Unused.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
void handle_wait_notifier(void *arg1, void *arg2, void *arg3) {
	handle_wait_t *wait = arg3;
	wait->cb(wait);
}

/** Create a new handle.
 *
 * Allocates a new handle in a handle table.
 *
 * @param table		Table to allocate in.
 * @param type		Type structure for the handle.
 * @param data		Data to associate with the handle.
 *
 * @return		Handle ID on success, negative error code on failure.
 */
handle_t handle_create(handle_table_t *table, handle_type_t *type, void *data) {
	handle_info_t *info;
	int ret;

	/* Require that data be set because there's no point having a handle
	 * with no associated data. */
	if(!table || !type || !data) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate the handle information structure. */
	info = slab_cache_alloc(handle_info_cache, MM_SLEEP);
	info->type = type;
	info->data = data;

	mutex_lock(&table->lock, 0);

	/* Find a handle ID in the table. */
	if((ret = bitmap_ffz(&table->bitmap)) == -1) {
		mutex_unlock(&table->lock);
		slab_cache_free(handle_info_cache, info);
		return -ERR_RESOURCE_UNAVAIL;
	}

	/* Set the bit and add the handle to the tree. */
	bitmap_set(&table->bitmap, ret);
	avl_tree_insert(&table->tree, (key_t)ret, info, NULL);

	/* Add a reference. */
	refcount_set(&info->count, 1);

	dprintf("handle: allocated handle %d in table %p (data: %p)\n",
	        ret, table, data);
	mutex_unlock(&table->lock);
	return (handle_t)ret;
}

/** Look up a handle in a table.
 *
 * Looks up the handle with the given ID in a handle table, ensuring that the
 * handle is the correct type. The returned handle structure is locked for
 * reading (with a readers/writer lock) - this ensures that nothing will
 * attempt to close the handle while it is in use, but allows multiple threads
 * to access the handle at a time. Note that it is up to the code that manages
 * the handle (e.g. the VFS) to implement proper synchronization for the data
 * it associates with the handle. When the handle is no longer needed, it
 * should be released with handle_release().
 *
 * @param table		Table to look up in.
 * @param handle	Handle ID to look up.
 * @param type		ID of wanted type (if negative, no type checking will
 *			be performed).
 * @param infop		Where to store handle information structure pointer.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_get(handle_table_t *table, handle_t handle, int type, handle_info_t **infop) {
	handle_info_t *info;

	if(!table || !infop) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&table->lock, 0);

	/* Look up the handle in the tree. */
	if(!(info = avl_tree_lookup(&table->tree, (key_t)handle))) {
		mutex_unlock(&table->lock);
		return -ERR_NOT_FOUND;
	}

	rwlock_read_lock(&info->lock, 0);

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && info->type->id != type) {
		rwlock_unlock(&info->lock);
		mutex_unlock(&table->lock);
		return -ERR_TYPE_INVAL;
	}

	mutex_unlock(&table->lock);
	*infop = info;
	return 0;
}

/** Release a handle.
 *
 * Releases a handle previously obtained with handle_get().
 *
 * @param info		Handle to release.
 */
void handle_release(handle_info_t *info) {
	assert(info->lock.readers);
	rwlock_unlock(&info->lock);
}

/** Close a handle.
 *
 * Closes a handle in a handle table.
 *
 * @param table		Table to close handle in.
 * @param handle	Handle ID to close.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_close(handle_table_t *table, handle_t handle) {
	handle_info_t *info;
	bool free = false;
	int ret = 0;

	if(!table) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&table->lock, 0);

	/* Look up the handle in the tree. */
	if(!(info = avl_tree_lookup(&table->tree, (key_t)handle))) {
		mutex_unlock(&table->lock);
		return -ERR_NOT_FOUND;
	}

	/* Closing the handle requires exclusive access (ensures that the
	 * handle is not currently in use). */
	rwlock_write_lock(&info->lock, 0);

	/* If there are no more references we can close it. */
	if(refcount_dec(&info->count) == 0) {
		if(info->type->close && (ret = info->type->close(info)) != 0) {
			/* Failed, return the reference and return an error. */
			refcount_inc(&info->count);
			goto out;
		}

		free = true;
	}

	/* Remove from the tree and mark the ID as free. */
	avl_tree_remove(&table->tree, (key_t)handle);
	bitmap_clear(&table->bitmap, handle);

	dprintf("handle: closed handle %" PRId32 " in table %p\n", handle, table);
out:
	rwlock_unlock(&info->lock);
	mutex_unlock(&table->lock);

	/* Free the structure if necessary. */
	if(free) {
		slab_cache_free(handle_info_cache, info);
	}
	return ret;
}

/** Handle waiting callback function.
 * @param wait		Wait information structure. */
static void handle_wait_cb(handle_wait_t *wait) {
	handle_wait_sync_t *sync = wait->data;

	__sync_bool_compare_and_swap(&sync->event, NULL, wait);
	semaphore_up(&sync->sem, 1);
}

/** Wait for an event to happen on an object.
 *
 * Waits for the specified event to happen on an object referred to by a
 * handle.
 *
 * @param table		Table containing the handle.
 * @param handle	Handle ID to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			return immediately if the event has not happened, and
 *			a value of -1 will block indefinitely until the event
 *			happens.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_wait(handle_table_t *table, handle_t handle, int event, timeout_t timeout) {
	handle_wait_sync_t sync;
	handle_wait_t wait;
	int ret;

	semaphore_init(&sync.sem, "handle_wait_sem", 0);
	sync.event = NULL;
	wait.event = event;
	wait.data = &sync;
	wait.cb = handle_wait_cb;

	if((ret = handle_get(table, handle, -1, &wait.info)) != 0) {
		return ret;
	} else if(!wait.info->type->wait || !wait.info->type->unwait) {
		handle_release(wait.info);
		return -ERR_NOT_SUPPORTED;
	} else if((ret = wait.info->type->wait(&wait)) != 0) {
		handle_release(wait.info);
		return ret;
	}

	ret = semaphore_down_timeout(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
	wait.info->type->unwait(&wait);
	handle_release(wait.info);
	return ret;
}

/** Wait for events to happen on multiple objects.
 *
 * Waits for the one of the specified events to happen on an object.
 *
 * @param table		Table containing the handles.
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
 *
 * @return		Index of handle that had the first event on success,
 *			negative error code on failure.
 */
int handle_wait_multiple(handle_table_t *table, handle_t *handles, int *events, size_t count, timeout_t timeout) {
	handle_wait_sync_t sync;
	handle_wait_t *waits;
	size_t i;
	int ret;

	if(!count || count > 1024 || !table || !handles || !events) {
		return -ERR_PARAM_INVAL;
	}

	semaphore_init(&sync.sem, "handle_wait_sem", 0);
	sync.event = NULL;

	/* Allocate wait structures for each handle and fill them in. */
	waits = kcalloc(count, sizeof(handle_wait_t), MM_SLEEP);
	for(i = 0; i < count; i++) {
		waits[i].event = events[i];
		waits[i].data = &sync;
		waits[i].cb = handle_wait_cb;
		waits[i].idx = i;

		if((ret = handle_get(table, handles[i], -1, &waits[i].info)) != 0) {
			goto out;
		} else if(!waits[i].info->type->wait || !waits[i].info->type->unwait) {
			handle_release(waits[i].info);
			waits[i].info = NULL;
			ret = -ERR_NOT_SUPPORTED;
			goto out;
		} else if((ret = waits[i].info->type->wait(&waits[i])) != 0) {
			handle_release(waits[i].info);
			waits[i].info = NULL;
			goto out;
		}
	}

	ret = semaphore_down_timeout(&sync.sem, timeout, SYNC_INTERRUPTIBLE);
out:
	for(i = 0; i < count; i++) {
		if(waits[i].info) {
			waits[i].info->type->unwait(&waits[i]);
			handle_release(waits[i].info);
		}
	}
	if(ret == 0) {
		assert(sync.event);
		ret = sync.event->idx;
	}
	kfree(waits);
	return ret;
}

/** Initialises a process' handle table.
 *
 * Initialises a process' handle table structure and duplicates handles
 * from its parent if required.
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
	mutex_init(&table->lock, "handle_table_lock", 0);
	return 0;
}

/** Destroy a process' handle table.
 *
 * Closes all handles in a process' handle table and frees data allocated for
 * it.
 *
 * @param table		Table to destroy.
 */
void handle_table_destroy(handle_table_t *table) {
	handle_info_t *info;
	int ret;

	mutex_lock(&table->lock, 0);

	/* Close all handles in the table. */
	AVL_TREE_FOREACH_SAFE(&table->tree, iter) {
		info = avl_tree_entry(iter, handle_info_t);

		rwlock_write_lock(&info->lock, 0);

		if(refcount_dec(&info->count) == 0) {
			if(info->type->close && (ret = info->type->close(info)) != 0) {
				kprintf(LOG_WARN, "handle: failed to destroy handle %" PRIu64 "(%p) (%d)\n",
				        iter->key, info, ret);
			}

			rwlock_unlock(&info->lock);
			slab_cache_free(handle_info_cache, info);
		} else {
			rwlock_unlock(&info->lock);
		}

		avl_tree_remove(&table->tree, iter->key);
	}

	bitmap_destroy(&table->bitmap);
	mutex_unlock(&table->lock);
}

/** Initialise the handle slab cache. */
static void __init_text handle_init(void) {
	handle_info_cache = slab_cache_create("handle_info_cache", sizeof(handle_info_t), 0,
	                                      handle_info_cache_ctor, NULL, NULL, NULL,
	                                      SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(handle_init);

#if 0
# pragma mark Debugger functions.
#endif

/** Print a list of handles for a process.
 *
 * Prints out a list of all currently open handles in a process.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_handles(int argc, char **argv) {
	handle_info_t *handle;
	process_t *process;
	unative_t id;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <process ID>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out a list of all currently open handles in a process.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &id, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	} else if(!(process = process_lookup(id))) {
		kprintf(LOG_NONE, "Invalid process ID.\n");
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID    Type                   Count  Data\n");
	kprintf(LOG_NONE, "==    ====                   =====  ====\n");

	AVL_TREE_FOREACH(&process->handles.tree, iter) {
		handle = avl_tree_entry(iter, handle_info_t);

		kprintf(LOG_NONE, "%-5" PRIu64 " %d - %-18p %-6d %p\n",
		        iter->key, handle->type->id, handle->type,
		        refcount_get(&handle->count), handle->data);
	}

	return KDBG_OK;
}

#if 0
# pragma mark System calls.
#endif

/** Close a handle.
 *
 * Closes a handle in the current process.
 *
 * @param handle	Handle ID to close.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_handle_close(handle_t handle) {
	return handle_close(&curr_proc->handles, handle);
}

/** Get the type of a handle.
 *
 * Gets the type ID of the specified handle in the current process.
 *
 * @param handle	Handle to get type of.
 *
 * @return		Type ID (positive value) on success, negative error
 *			code on failure.
 */
int sys_handle_type(handle_t handle) {
	handle_info_t *info;
	int ret;

	if((ret = handle_get(&curr_proc->handles, handle, -1, &info)) != 0) {
		return ret;
	}

	ret = info->type->id;
	handle_release(info);
	return ret;
}

/** Wait for an event to happen on an object.
 *
 * Waits for the specified event to happen on an object referred to by a
 * handle.
 *
 * @param handle	Handle ID to wait on.
 * @param event		Event ID to wait for (specific to object type).
 * @param timeout	Maximum time to wait in microseconds. A value of 0
 *			will wait indefinitely until an event occurs, and a
 *			value of -1 will return immediately if the event has
 *			not happened.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_handle_wait(handle_t handle, int event, timeout_t timeout) {
	return handle_wait(&curr_proc->handles, handle, event, timeout);
}

/** Wait for events to happen on multiple objects.
 *
 * Waits for the one of the specified events to happen on an object.
 *
 * @param handles	Array of handle IDs to wait for.
 * @param event		Array of event IDs to wait for (specific to object
 *			type). The index into the array selects the handle the
 *			event is for - for example, specifying 1 as the event
 *			at index 1 will wait for event 1 on the handle at index
 *			1 in the handle array. If you wish to wait for multiple
 *			events on one handle, specify the handle multiple times
 *			in the arrays.
 * @param count		Number of handles.
 * @param timeout	Maximum time to wait in microseconds. A value of 0
 *			will wait indefinitely until an event occurs, and a
 *			value of -1 will return immediately if the event has
 *			not happened.
 *
 * @return		Index of handle that had the first event on success,
 *			negative error code on failure.
 */
int sys_handle_wait_multiple(handle_t *handles, int *events, size_t count, timeout_t timeout) {
	handle_t *khandles;
	int *kevents;
	int ret;

	if(!count || count > 1024 || !handles || !events) {
		return -ERR_PARAM_INVAL;
	}

	khandles = kmalloc(sizeof(handle_t) * count, MM_SLEEP);
	if((ret = memcpy_from_user(khandles, handles, sizeof(handle_t) * count)) != 0) {
		kfree(khandles);
		return ret;
	}

	kevents = kmalloc(sizeof(int) * count, MM_SLEEP);
	if((ret = memcpy_from_user(kevents, events, sizeof(int) * count)) != 0) {
		kfree(kevents);
		kfree(khandles);
		return ret;
	}

	ret = handle_wait_multiple(&curr_proc->handles, khandles, kevents, count, timeout);
	kfree(kevents);
	kfree(khandles);
	return ret;
}
