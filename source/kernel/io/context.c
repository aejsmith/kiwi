/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <io/fs.h>

#include <proc/process.h>

#include <assert.h>
#include <errors.h>

extern void fs_node_get(fs_node_t *node);
extern fs_mount_t *root_mount;

/** Initialise an I/O context.
 *
 * Initialises an I/O context structure. If a parent context is provided, then
 * the new context will inherit parts of the parent context such as current
 * working directory. If no parent is specified, the working directory will be
 * set to the root of the filesystem.
 *
 * @param context	Context to initialise.
 * @param parent	Parent context (can be NULL).
 */
void io_context_init(io_context_t *context, io_context_t *parent) {
	rwlock_init(&context->lock, "io_context_lock");
	context->curr_dir = NULL;
	context->root_dir = NULL;

	/* Inherit parent's current/root directories if possible. */
	if(parent) {
		rwlock_read_lock(&parent->lock);

		assert(parent->root_dir);
		assert(parent->curr_dir);

		fs_node_get(parent->root_dir);
		context->root_dir = parent->root_dir;
		fs_node_get(parent->curr_dir);
		context->curr_dir = parent->curr_dir;

		rwlock_unlock(&parent->lock);
	} else if(root_mount) {
		fs_node_get(root_mount->root);
		context->root_dir = root_mount->root;
		fs_node_get(root_mount->root);
		context->curr_dir = root_mount->root;
	} else {
		/* This should only be the case when the kernel process is
		 * being created. */
		assert(!kernel_proc);
	}
}

/** Destroy an I/O context.
 * @param context	Context to destroy. */
void io_context_destroy(io_context_t *context) {
	fs_node_release(context->curr_dir);
}

/** Set the current directory of an I/O context.
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
int io_context_setcwd(io_context_t *context, fs_node_t *node) {
	fs_node_t *old;

	if(node->type != FS_NODE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	fs_node_get(node);

	rwlock_write_lock(&context->lock);
	old = context->curr_dir;
	context->curr_dir = node;
	rwlock_unlock(&context->lock);

	fs_node_release(node);
	return 0;
}

/** Set the root directory of an I/O context.
 *
 * Sets both the root directory and current directory of an I/O context to
 * the specified directory.
 *
 * @param context	Context to set in.
 * @param node		Node to set to.
 *
 * @return		0 on success, negative error code on failure.
 */
int io_context_setroot(io_context_t *context, fs_node_t *node) {
	fs_node_t *oldr, *oldc;

	if(node->type != FS_NODE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	/* Get twice: one for root, one for current. */
	fs_node_get(node);
	fs_node_get(node);

	rwlock_write_lock(&context->lock);
	oldc = context->curr_dir;
	context->curr_dir = node;
	oldr = context->root_dir;
	context->root_dir = node;
	rwlock_unlock(&context->lock);

	fs_node_release(oldc);
	fs_node_release(oldr);
	return 0;
}
