/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		I/O context functions.
 */

#include <io/fs.h>

#include <proc/process.h>

#include <assert.h>
#include <status.h>

extern fs_mount_t *root_mount;

/**
 * Initialize an I/O context.
 *
 * Initializes an I/O context structure. If a parent context is provided, then
 * the new context will inherit parts of the parent context such as current
 * working directory. If no parent is specified, the working directory will be
 * set to the root of the filesystem.
 *
 * @param context	Context to initialize.
 * @param parent	Parent context (can be NULL).
 */
void io_context_init(io_context_t *context, io_context_t *parent) {
	rwlock_init(&context->lock, "io_context_lock");
	context->work_dir = NULL;
	context->root_dir = NULL;

	/* Inherit parent's current/root directories if possible. */
	if(parent) {
		rwlock_read_lock(&parent->lock);

		assert(parent->root_dir);
		assert(parent->work_dir);

		fs_node_retain(parent->root_dir);
		context->root_dir = parent->root_dir;
		fs_node_retain(parent->work_dir);
		context->work_dir = parent->work_dir;

		rwlock_unlock(&parent->lock);
	} else if(root_mount) {
		fs_node_retain(root_mount->root);
		context->root_dir = root_mount->root;
		fs_node_retain(root_mount->root);
		context->work_dir = root_mount->root;
	} else {
		/* This should only be the case when the kernel process is
		 * being created. */
		assert(!kernel_proc);
	}
}

/** Destroy an I/O context.
 * @param context	Context to destroy. */
void io_context_destroy(io_context_t *context) {
	fs_node_release(context->work_dir);
	fs_node_release(context->root_dir);
}

/**
 * Set the working directory of an I/O context.
 *
 * Sets the working directory of an I/O context to the specified filesystem
 * node. The previous working directory node will be released, and the supplied
 * node will be referenced.
 *
 * @param context	Context to set directory of.
 * @param node		Node to set to.
 *
 * @return		Status code describing result of the operation.
 */
status_t io_context_set_work_dir(io_context_t *context, fs_node_t *node) {
	fs_node_t *old;

	if(node->file.type != FILE_TYPE_DIR)
		return STATUS_NOT_DIR;

	fs_node_retain(node);

	rwlock_write_lock(&context->lock);
	old = context->work_dir;
	context->work_dir = node;
	rwlock_unlock(&context->lock);

	fs_node_release(old);
	return STATUS_SUCCESS;
}

/**
 * Set the root directory of an I/O context.
 *
 * Sets both the root directory and working directory of an I/O context to
 * the specified directory.
 *
 * @param context	Context to set in.
 * @param node		Node to set to.
 *
 * @return		Status code describing result of the operation.
 */
status_t io_context_set_root_dir(io_context_t *context, fs_node_t *node) {
	fs_node_t *old_root, *old_work;

	if(node->file.type != FILE_TYPE_DIR)
		return STATUS_NOT_DIR;

	/* Get twice: one for root, one for working. */
	fs_node_retain(node);
	fs_node_retain(node);

	rwlock_write_lock(&context->lock);
	old_work = context->work_dir;
	context->work_dir = node;
	old_root = context->root_dir;
	context->root_dir = node;
	rwlock_unlock(&context->lock);

	fs_node_release(old_work);
	fs_node_release(old_root);
	return STATUS_SUCCESS;
}
