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
 * @brief		RAM-based temporary filesystem.
 */

#include <io/entry_cache.h>
#include <io/fs.h>
#include <io/request.h>

#include <lib/atomic.h>
#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/vm_cache.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** RAMFS mount information structure. */
typedef struct ramfs_mount {
	atomic_t next_id;		/**< Next node ID. */
} ramfs_mount_t;

/** RAMFS node information structure. */
typedef struct ramfs_node {
	union {
		vm_cache_t *cache;	/**< Data cache. */
		entry_cache_t *entries;	/**< Directory entry store. */
		char *target;		/**< Symbolic link destination. */
	};

	nstime_t created;		/**< Time of creation. */
	nstime_t accessed;		/**< Time of last access. */
	nstime_t modified;		/**< Time last modified. */
} ramfs_node_t;

/** Free a RAMFS node.
 * @param node		Node to free. */
static void ramfs_node_free(fs_node_t *node) {
	ramfs_node_t *data = node->data;

	/* Destroy the data caches. */
	switch(node->file.type) {
	case FILE_TYPE_REGULAR:
		vm_cache_destroy(data->cache, true);
		break;
	case FILE_TYPE_DIR:
		entry_cache_destroy(data->entries);
		break;
	case FILE_TYPE_SYMLINK:
		kfree(data->target);
		break;
	default:
		break;
	}

	kfree(data);
}

/** Create a RAMFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @param nodep		Where to store pointer to created node.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_create(fs_node_t *parent, const char *name,
	file_type_t type, const char *target, fs_node_t **nodep)
{
	ramfs_mount_t *mount = parent->mount->data;
	ramfs_node_t *pdata = parent->data, *data;
	node_id_t id;

	assert(parent->file.type == FILE_TYPE_DIR);

	/* Create the information structure. */
	data = kmalloc(sizeof(*data), MM_KERNEL);
	data->created = unix_time();
	data->accessed = unix_time();
	data->modified = unix_time();

	/* Allocate a unique ID for the node. */
	id = atomic_inc(&mount->next_id);

	/* Create data stores. */
	switch(type) {
	case FILE_TYPE_REGULAR:
		data->cache = vm_cache_create(0, NULL, NULL);
		break;
	case FILE_TYPE_DIR:
		data->entries = entry_cache_create(NULL, NULL);

		/* Add '.' and '..' entries to the cache. */
		entry_cache_insert(data->entries, ".", id);
		entry_cache_insert(data->entries, "..", parent->id);
		break;
	case FILE_TYPE_SYMLINK:
		data->target = kstrdup(target, MM_KERNEL);
		break;
	default:
		kfree(data);
		return STATUS_NOT_SUPPORTED;
	}

	entry_cache_insert(pdata->entries, name, id);
	*nodep = fs_node_alloc(parent->mount, id, type, parent->ops, data);
	return STATUS_SUCCESS;
}

/** Unlink a RAMFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name of the node in the directory.
 * @param node		Node structure describing the node being created.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_unlink(fs_node_t *parent, const char *name, fs_node_t *node) {
	ramfs_node_t *data = node->data;
	ramfs_node_t *pdata = parent->data;
	dir_entry_t *entry;

	assert(parent->file.type == FILE_TYPE_DIR);

	if(node->file.type == FILE_TYPE_DIR) {
		mutex_lock(&data->entries->lock);

		/* Ensure the directory is empty. */
		RADIX_TREE_FOREACH(&data->entries->entries, iter) {
			entry = radix_tree_entry(iter, dir_entry_t);

			if(strcmp(entry->name, ".") && strcmp(entry->name, "..")) {
				mutex_unlock(&data->entries->lock);
				return STATUS_DIR_NOT_EMPTY;
			}
		}

		mutex_unlock(&data->entries->lock);
	}

	entry_cache_remove(pdata->entries, name);
	fs_node_remove(node);
	return STATUS_SUCCESS;
}

/** Get information about a RAMFS node.
 * @param node		Node to get information on.
 * @param info		Information structure to fill in. */
static void ramfs_node_info(fs_node_t *node, file_info_t *info) {
	ramfs_node_t *data = node->data;

	info->links = 1;
	info->block_size = PAGE_SIZE;
	info->created = data->created;
	info->accessed = data->accessed;
	info->modified = data->modified;

	switch(node->file.type) {
	case FILE_TYPE_REGULAR:
		info->size = data->cache->size;
		break;
	case FILE_TYPE_SYMLINK:
		info->size = strlen(data->target);
		break;
	default:
		info->size = 0;
		break;
	}
}

/** Resize a RAMFS file.
 * @param node		Node to resize.
 * @param size		New size of the node.
 * @return		Always returns STATUS_SUCCESS. */
static status_t ramfs_node_resize(fs_node_t *node, offset_t size) {
	ramfs_node_t *data = node->data;

	assert(node->file.type == FILE_TYPE_REGULAR);

	vm_cache_resize(data->cache, size);
	data->modified = unix_time();
	return STATUS_SUCCESS;
}

/** Look up a RAMFS directory entry.
 * @param node		Node to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry points to.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_lookup(fs_node_t *node, const char *name, node_id_t *idp) {
	ramfs_node_t *data = node->data;

	assert(node->file.type == FILE_TYPE_DIR);
	return entry_cache_lookup(data->entries, name, idp);
}

/** Read the destination of a RAMFS symbolic link.
 * @param node		Node to read from.
 * @param destp		Where to store pointer to string containing link
 *			destination.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_read_symlink(fs_node_t *node, char **destp) {
	ramfs_node_t *data = node->data;

	assert(node->file.type == FILE_TYPE_SYMLINK);

	*destp = kstrdup(data->target, MM_KERNEL);
	return STATUS_SUCCESS;
}

/** Perform I/O on a RAMFS file.
 * @param node		Node to perform I/O on.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_io(fs_node_t *node, file_handle_t *handle, io_request_t *request) {
	ramfs_node_t *data = node->data;
	status_t ret;

	assert(node->file.type == FILE_TYPE_REGULAR);

	if(request->op == IO_OP_WRITE) {
		if((offset_t)(request->offset + request->total) > data->cache->size)
			vm_cache_resize(data->cache, request->offset + request->total);
	}

	ret = vm_cache_io(data->cache, request);
	if(ret != STATUS_SUCCESS)
		return ret;

	if(request->op == IO_OP_WRITE && request->transferred)
		data->modified = unix_time();

	return STATUS_SUCCESS;
}

/** Get the data cache for a RAMFS file.
 * @param node		Node to get cache for.
 * @param handle	File handle structure.
 * @return		Pointer to node's VM cache. */
static vm_cache_t *ramfs_node_get_cache(fs_node_t *node, file_handle_t *handle) {
	ramfs_node_t *data = node->data;

	assert(node->file.type == FILE_TYPE_REGULAR);
	return data->cache;
}

/** Read a RAMFS directory entry.
 * @param node		Node to read from.
 * @param handle	File handle structure.
 * @param entryp	Where to store pointer to directory entry structure.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_read_dir(fs_node_t *node, file_handle_t *handle,
	dir_entry_t **entryp)
{
	ramfs_node_t *data = node->data;
	dir_entry_t *entry;
	offset_t i = 0;

	assert(node->file.type == FILE_TYPE_DIR);

	mutex_lock(&data->entries->lock);

	RADIX_TREE_FOREACH(&data->entries->entries, iter) {
		entry = radix_tree_entry(iter, dir_entry_t);

		if(i++ == handle->offset) {
			*entryp = kmemdup(entry, entry->length, MM_KERNEL);
			mutex_unlock(&data->entries->lock);
			handle->offset++;
			return STATUS_SUCCESS;
		}
	}

	mutex_unlock(&data->entries->lock);
	return STATUS_NOT_FOUND;
}

/** RAMFS node operations structure. */
static fs_node_ops_t ramfs_node_ops = {
	.free = ramfs_node_free,
	.create = ramfs_node_create,
	.unlink = ramfs_node_unlink,
	.info = ramfs_node_info,
	.resize = ramfs_node_resize,
	.lookup = ramfs_node_lookup,
	.read_symlink = ramfs_node_read_symlink,
	.io = ramfs_node_io,
	.get_cache = ramfs_node_get_cache,
	.read_dir = ramfs_node_read_dir,
};

/** Unmount a RAMFS.
 * @param mount		Mount that's being unmounted. */
static void ramfs_unmount(fs_mount_t *mount) {
	kfree(mount->data);
}

/** RAMFS mount operations structure. */
static fs_mount_ops_t ramfs_mount_ops = {
	.unmount = ramfs_unmount,
};

/** Mount a RAMFS filesystem.
 * @param mount		Mount structure for the FS.
 * @param opts		Array of mount options.
 * @param count		Number of options.
 * @return		Status code describing result of the operation. */
static status_t ramfs_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
	ramfs_mount_t *data;
	ramfs_node_t *ndata;

	mount->ops = &ramfs_mount_ops,
	mount->data = data = kmalloc(sizeof(ramfs_mount_t), MM_KERNEL);
	data->next_id = 1;

	/* Create the root directory, and add '.' and '..' entries. */
	ndata = kmalloc(sizeof(ramfs_node_t), MM_KERNEL);
	ndata->created = unix_time();
	ndata->accessed = unix_time();
	ndata->modified = unix_time();
	ndata->entries = entry_cache_create(NULL, NULL);
	entry_cache_insert(ndata->entries, ".", 0);
	entry_cache_insert(ndata->entries, "..", 0);
	mount->root = fs_node_alloc(mount, 0, FILE_TYPE_DIR, &ramfs_node_ops, ndata);
	return STATUS_SUCCESS;
}

/** RAMFS filesystem type structure. */
static fs_type_t ramfs_fs_type = {
	.name = "ramfs",
	.description = "RAM-based temporary filesystem",
	.mount = ramfs_mount,
};

/** Register RAMFS with the VFS. */
static __init_text void ramfs_init(void) {
	status_t ret;

	ret = fs_type_register(&ramfs_fs_type);
	if(ret != STATUS_SUCCESS)
		fatal("Could not register ramfs filesystem type (%d)", ret);
}

INITCALL(ramfs_init);
