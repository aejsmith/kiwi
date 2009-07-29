/* Kiwi per-process object manager
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

#include <console/kprintf.h>

#include <mm/slab.h>

#include <proc/handle.h>
#include <proc/process.h>

#include <assert.h>
#include <errors.h>
#include <init.h>
#include <kdbg.h>

#if CONFIG_HANDLE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

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
		return -ERR_NO_HANDLES;
	}

	/* Set the bit and add the handle to the tree. */
	bitmap_set(&table->bitmap, ret);
	avltree_insert(&table->tree, (key_t)ret, info, NULL);

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
	if(!(info = avltree_lookup(&table->tree, (key_t)handle))) {
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
	if(!(info = avltree_lookup(&table->tree, (key_t)handle))) {
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
	avltree_remove(&table->tree, (key_t)handle);
	bitmap_clear(&table->bitmap, handle);

	dprintf("handle: closed handle %" PRId32 " in table %p\n", handle, table);
out:
	rwlock_unlock(&info->lock);
	mutex_unlock(&table->lock);

	/* Free the structure if necessary. */
	if(free) {
		slab_cache_free(handle_info_cache, info);
	}
	return 0;
}

/** Initializes a process' handle table.
 *
 * Initializes a process' handle table structure and duplicates handles
 * from its parent if required.
 *
 * @param table		Table to initialize.
 * @param parent	Parent process' handle table.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_table_init(handle_table_t *table, handle_table_t *parent) {
	avltree_init(&table->tree);
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
	avltree_node_t *node;
	handle_info_t *info;
	int ret;

	mutex_lock(&table->lock, 0);

	/* Close all handles in the table - cannot use tree iterator here
	 * because removing makes the current node invalid. */
	while((node = avltree_node_first(&table->tree))) {
		info = avltree_entry(node, handle_info_t);

		avltree_remove(&table->tree, node->key);

		rwlock_write_lock(&info->lock, 0);

		if(refcount_dec(&info->count) == 0) {
			if(info->type->close && (ret = info->type->close(info)) != 0) {
				kprintf(LOG_NORMAL, "handle: failed to destroy handle %" PRIu64 "(%p) (%d)\n",
				        node->key, info, ret);
			}

			rwlock_unlock(&info->lock);
			slab_cache_free(handle_info_cache, info);
		} else {
			rwlock_unlock(&info->lock);
		}
	}

	bitmap_destroy(&table->bitmap);
	mutex_unlock(&table->lock);
}

/** Initialize the handle slab cache. */
static void __init_text handle_init(void) {
	handle_info_cache = slab_cache_create("handle_info_cache", sizeof(handle_info_t), 0,
	                                      handle_info_cache_ctor, NULL, NULL, NULL, NULL,
	                                      0, MM_FATAL);
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

	AVLTREE_FOREACH(&process->handles.tree, iter) {
		handle = avltree_entry(iter, handle_info_t);

		kprintf(LOG_NONE, "%-5" PRIu64 " %-2d(%-18p) %-6d %p\n",
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
