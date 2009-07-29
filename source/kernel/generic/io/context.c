/* Kiwi I/O context functions
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
 * @brief		I/O context functions.
 */

#include <io/context.h>

#include <errors.h>

/** Initialize an I/O context.
 *
 * Initializes an I/O context structure. If a parent context is provided, then
 * the new context will inherit parts of the parent context such as current
 * working directory. In-progress asynchronous I/O requests are not inherited.
 * If no parent is specified, the working directory will be set to the root of
 * the filesystem.
 *
 * @param context	Context to initialize.
 * @param parent	Parent context (can be NULL).
 *
 * @return		0 on success, negative error code on failure.
 */
int io_context_init(io_context_t *context, io_context_t *parent) {
	mutex_init(&context->lock, "io_context_lock", 0);
	list_init(&context->async_requests);
	context->curr_dir = NULL;

	/* Inherit parent's current directory if possible. If we have a parent
	 * context, it may not have a working directory - this is because when
	 * the kernel process is initialized, the VFS is not initialized and
	 * the root filesystem has not been mounted. In this case, we attempt
	 * to get the root of the filesystem again. */
	if(parent) {
		mutex_lock(&parent->lock, 0);
		if(parent->curr_dir) {
			vfs_node_get(parent->curr_dir);
			context->curr_dir = parent->curr_dir;
		}
		mutex_unlock(&parent->lock);
	}

	/* Fall back on getting the root of the FS if we don't have a current
	 * directory now (this may still fail). */
	if(!context->curr_dir) {
		vfs_node_lookup("/", true, &context->curr_dir);
	}

	return 0;
}

/** Destroy an I/O context.
 *
 * Destroys an I/O context by cancelling all in-progress asynchronous I/O
 * requests and freeing its current directory.
 *
 * @param context	Context to destroy.
 */
void io_context_destroy(io_context_t *context) {
	vfs_node_release(context->curr_dir);
}

/** Get current directory of an I/O context.
 *
 * Gets a pointer to the node for the current directory of an I/O context.
 * The node will be locked and have an extra reference set on it - when it is
 * no longer required by the caller, it should be unlocked and released.
 *
 * @param context	Context to get directory from.
 *
 * @return		Pointer to locked and referenced node, or NULL if the
 *			context does not have a current directory.
 */
vfs_node_t *io_context_getcwd(io_context_t *context) {
	vfs_node_t *node;

	mutex_lock(&context->lock, 0);

	if((node = context->curr_dir)) {
		mutex_lock(&node->lock, 0);
		vfs_node_get(node);
	}

	mutex_unlock(&context->lock);
	return node;
}

/** Set current directory in an I/O context.
 *
 * Sets the current directory of an I/O context to the specified filesystem
 * node. The previous working directory node will be released, and the supplied
 * node will be referenced.
 *
 * @param context	Context to set directory of.
 * @param node		Node to set to.
 *
 * @return		0 on success, negative error code on failure.
 */
int io_context_setcwd(io_context_t *context, vfs_node_t *node) {
	vfs_node_t *old;

	if(node->type != VFS_NODE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	vfs_node_get(node);

	mutex_lock(&context->lock, 0);
	old = context->curr_dir;
	context->curr_dir = node;
	mutex_unlock(&context->lock);

	if(old) {
		vfs_node_release(old);
	}
	return 0;
}
