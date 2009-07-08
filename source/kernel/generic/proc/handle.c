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
 *
 * @todo		Use a readers-writer lock on handles and have all gets
 *			use read access, and operations like closing use write
 *			access - this would allow multiple things to use a
 *			handle at a time (and have proper synchronization
 *			implemented by the user of the handle), while ensuring
 *			that a handle will not be closed while it is being
 *			used.
 */

#include <console/kprintf.h>

#include <mm/slab.h>

#include <proc/handle.h>
#include <proc/process.h>

#include <errors.h>
#include <init.h>

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
	mutex_init(&info->lock, "handle_lock", 0);
	return 0;
}

/** Create a new handle in a process.
 *
 * Allocates a new handle in a process' handle table and sets its data to the
 * given pointer.
 *
 * @param process	Process to allocate in.
 * @param ops		Operations for the handle.
 * @param data		Data to associate with the handle.
 *
 * @return		Handle ID on success, negative error code on failure.
 */
handle_t handle_create(process_t *process, handle_ops_t *ops, void *data) {
	handle_info_t *info;
	int ret;

	/* Require that data be set because there's no point having a handle
	 * with no associated data. */
	if(!process || !ops || !data) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate the handle information structure. */
	info = slab_cache_alloc(handle_info_cache, MM_SLEEP);
	info->ops = ops;
	info->data = data;

	mutex_lock(&process->handles.lock, 0);

	/* Find a handle ID in the table. */
	ret = bitmap_ffz(&process->handles.bitmap);
	if(ret == -1) {
		mutex_unlock(&process->handles.lock);
		slab_cache_free(handle_info_cache, info);
		return -ERR_NO_HANDLES;
	}

	/* Set the bit and add the handle to the tree. */
	bitmap_set(&process->handles.bitmap, ret);
	avltree_insert(&process->handles.tree, (key_t)ret, info, NULL);

	/* Add a reference. */
	refcount_set(&info->count, 1);

	dprintf("handle: allocated handle %d in process %p(%s) (data: %p)\n",
	        ret, process, process->name, data);
	mutex_unlock(&process->handles.lock);
	return (handle_t)ret;
}

/** Look up a handle in a process.
 *
 * Looks up the handle with the given ID in a process' handle table, ensuring
 * that the handle is the correct type. The operations structure address is
 * used to identify a handle type - if the handle's operations structure
 * pointer is the same as the one provided to this function, then it is assumed
 * to be the correct type. The returned handle structure is locked; when it is
 * no longer needed, it should be released with handle_release().
 *
 * @param process	Process to look up in.
 * @param handle	Handle ID to look up.
 * @param ops		Operations structure for checking handle type.
 * @param infop		Where to store handle structure pointer.
 *
 * @return		0 on success, with handle data pointer stored in datap,
 *			negative error code on failure.
 */
int handle_get(process_t *process, handle_t handle, handle_ops_t *ops, handle_info_t **infop) {
	handle_info_t *info;

	mutex_lock(&process->handles.lock, 0);

	/* Look up the handle in the tree. */
	info = avltree_lookup(&process->handles.tree, (key_t)handle);
	if(!info) {
		mutex_unlock(&process->handles.lock);
		return -ERR_NOT_FOUND;
	}

	mutex_lock(&info->lock, 0);

	if(info->ops != ops) {
		mutex_unlock(&info->lock);
		mutex_unlock(&process->handles.lock);
		return -ERR_TYPE_INVAL;
	}

	mutex_unlock(&process->handles.lock);
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
	mutex_unlock(&info->lock);
}

/** Close a handle.
 *
 * Closes a handle in a process.
 *
 * @param process	Process to close handle in.
 * @param handle	Handle ID to close.
 *
 * @return		0 on success, negative error code on failure.
 */
int handle_close(process_t *process, handle_t handle) {
	handle_info_t *info;
	bool free = false;
	int ret = 0;

	mutex_lock(&process->handles.lock, 0);

	/* Look up the handle in the tree. */
	info = avltree_lookup(&process->handles.tree, (key_t)handle);
	if(!info) {
		mutex_unlock(&process->handles.lock);
		return -ERR_NOT_FOUND;
	}

	mutex_lock(&info->lock, 0);

	/* If there are no more references we can close it. */
	if(refcount_dec(&info->count) == 0) {
		if(info->ops->close && (ret = info->ops->close(info->data)) != 0) {
			/* Failed, return the reference and return an error. */
			refcount_inc(&info->count);
			goto out;
		}

		free = true;
	}

	/* Remove from the tree and mark the ID as free. */
	avltree_remove(&process->handles.tree, (key_t)handle);
	bitmap_clear(&process->handles.bitmap, handle);

	dprintf("handle: closed handle %" PRIu32 " in process %p(%s)\n", handle,
	        process, process->name);
out:
	mutex_unlock(&info->lock);
	mutex_unlock(&process->handles.lock);

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

	/* Close all handles in the table. */
	while((node = avltree_node_first(&table->tree))) {
		info = avltree_entry(node, handle_info_t);

		avltree_remove(&table->tree, node->key);

		if(refcount_dec(&info->count) == 0) {
			if(info->ops->close && (ret = info->ops->close(info->data)) != 0) {
				kprintf(LOG_DEBUG, "handle: failed to destroy handle %" PRIu64 "(%p) (%d)\n",
				        node->key, info, ret);
			}

			slab_cache_free(handle_info_cache, info);
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
	return handle_close(curr_thread->owner, handle);
}
