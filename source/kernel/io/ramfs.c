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
 * @brief		RAM-based temporary filesystem.
 */

#include <io/entry_cache.h>
#include <io/fs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/vm_cache.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** RamFS mount information structure. */
typedef struct ramfs_mount {
	mutex_t lock;			/**< Lock to protect ID allocation. */
	node_id_t next_id;		/**< Next node ID. */
} ramfs_mount_t;

/** RamFS node information structure. */
typedef struct ramfs_node {
	union {
		vm_cache_t *cache;	/**< Data cache. */
		entry_cache_t *entries;	/**< Directory entry store. */
		char *target;		/**< Symbolic link destination. */
	};

	useconds_t created;		/**< Time of creation. */
	useconds_t accessed;		/**< Time of last access. */
	useconds_t modified;		/**< Time last modified. */
} ramfs_node_t;

/** Free a RamFS node.
 * @param node		Node to free. */
static void ramfs_node_free(fs_node_t *node) {
	ramfs_node_t *data = node->data;

	/* Destroy the data caches. */
	switch(node->type) {
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

/** Create a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @param security	Security attributes for the node.
 * @param nodep		Where to store pointer to node structure for created
 *			entry.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_create(fs_node_t *parent, const char *name, file_type_t type,
                                  const char *target, object_security_t *security,
                                  fs_node_t **nodep) {
	ramfs_mount_t *mount = parent->mount->data;
	ramfs_node_t *pdata = parent->data, *data;
	node_id_t id;

	assert(parent->type == FILE_TYPE_DIR);

	/* Create the information structure. */
	data = kmalloc(sizeof(ramfs_node_t), MM_SLEEP);
	data->created = time_since_epoch();
	data->accessed = time_since_epoch();
	data->modified = time_since_epoch();

	/* Allocate a unique ID for the node. */
	mutex_lock(&mount->lock);
	id = mount->next_id++;
	mutex_unlock(&mount->lock);

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
		data->target = kstrdup(target, MM_SLEEP);
		break;
	default:
		kfree(data);
		return STATUS_NOT_SUPPORTED;
	}

	entry_cache_insert(pdata->entries, name, id);
	*nodep = fs_node_alloc(parent->mount, id, type, security, parent->ops, data);
	return STATUS_SUCCESS;
}

/** Unlink a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name of the node in the directory.
 * @param node		Node structure describing the node being created.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_unlink(fs_node_t *parent, const char *name, fs_node_t *node) {
	ramfs_node_t *pdata = parent->data;

	assert(parent->type == FILE_TYPE_DIR);

	entry_cache_remove(pdata->entries, name);
	fs_node_remove(node);
	return STATUS_SUCCESS;
}

/** Get information about a RamFS node.
 * @param node		Node to get information on.
 * @param infop		Information structure to fill in. */
static void ramfs_node_info(fs_node_t *node, file_info_t *infop) {
	ramfs_node_t *data = node->data;

	infop->links = 1;
	infop->block_size = PAGE_SIZE;
	infop->created = data->created;
	infop->accessed = data->accessed;
	infop->modified = data->modified;

	switch(node->type) {
	case FILE_TYPE_REGULAR:
		infop->size = data->cache->size;
		break;
	case FILE_TYPE_SYMLINK:
		infop->size = strlen(data->target);
		break;
	default:
		infop->size = 0;
		break;
	}
}

/** Update security attributes of a RamFS node.
 * @param node		Node to set for.
 * @param security	New security attributes to set.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_set_security(fs_node_t *node, const object_security_t *security) {
	/* Do not need to do anything here. However, this function must be
	 * given in the operations structure so that the FS layer knows that
	 * we support security attributes. */
	return STATUS_SUCCESS;
}

/** Read from a RamFS file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into file to read from.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_read(fs_node_t *node, void *buf, size_t count, offset_t offset,
                                bool nonblock, size_t *bytesp) {
	ramfs_node_t *data = node->data;

	assert(node->type == FILE_TYPE_REGULAR);
	return vm_cache_read(data->cache, buf, count, offset, nonblock, bytesp);
}

/** Write to a RamFS file.
 * @param node		Node to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset into file to write to.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_write(fs_node_t *node, const void *buf, size_t count, offset_t offset,
                                 bool nonblock, size_t *bytesp) {
	ramfs_node_t *data = node->data;
	status_t ret;

	assert(node->type == FILE_TYPE_REGULAR);

	if((offset + count) > data->cache->size) {
		vm_cache_resize(data->cache, offset + count);
	}

	ret = vm_cache_write(data->cache, buf, count, offset, nonblock, bytesp);
	if(*bytesp) {
		data->modified = time_since_epoch();
	}

	return ret;
}

/** Get the data cache for a RamFS file.
 * @param node		Node to get cache for.
 * @return		Pointer to node's VM cache. */
static vm_cache_t *ramfs_node_get_cache(fs_node_t *node) {
	ramfs_node_t *data = node->data;

	assert(node->type == FILE_TYPE_REGULAR);
	return data->cache;
}

/** Resize a RamFS file.
 * @param node		Node to resize.
 * @param size		New size of the node.
 * @return		Always returns STATUS_SUCCESS. */
static status_t ramfs_node_resize(fs_node_t *node, offset_t size) {
	ramfs_node_t *data = node->data;

	assert(node->type == FILE_TYPE_REGULAR);

	vm_cache_resize(data->cache, size);
	data->modified = time_since_epoch();
	return STATUS_SUCCESS;
}

/** Read a RamFS directory entry.
 * @param node		Node to read from.
 * @param index		Index of entry to read.
 * @param entryp	Where to store pointer to directory entry structure.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_read_entry(fs_node_t *node, offset_t index, dir_entry_t **entryp) {
	ramfs_node_t *data = node->data;
	dir_entry_t *entry;
	offset_t i = 0;

	assert(node->type == FILE_TYPE_DIR);

	mutex_lock(&data->entries->lock);

	RADIX_TREE_FOREACH(&data->entries->entries, iter) {
		entry = radix_tree_entry(iter, dir_entry_t);

		if(i++ == index) {
			*entryp = kmemdup(entry, entry->length, MM_SLEEP);
			mutex_unlock(&data->entries->lock);
			return STATUS_SUCCESS;
		}
	}

	mutex_unlock(&data->entries->lock);
	return STATUS_NOT_FOUND;
}

/** Look up a RamFS directory entry.
 * @param node		Node to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry points to.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_lookup_entry(fs_node_t *node, const char *name, node_id_t *idp) {
	ramfs_node_t *data = node->data;

	assert(node->type == FILE_TYPE_DIR);
	return entry_cache_lookup(data->entries, name, idp);
}

/** Read the destination of a RamFS symbolic link.
 * @param node		Node to read from.
 * @param destp		Where to store pointer to string containing link
 *			destination.
 * @return		Status code describing result of the operation. */
static status_t ramfs_node_read_link(fs_node_t *node, char **destp) {
	ramfs_node_t *data = node->data;

	assert(node->type == FILE_TYPE_SYMLINK);

	*destp = kstrdup(data->target, MM_SLEEP);
	return STATUS_SUCCESS;
}

/** RamFS node operations structure. */
static fs_node_ops_t ramfs_node_ops = {
	.free = ramfs_node_free,
	.create = ramfs_node_create,
	.unlink = ramfs_node_unlink,
	.info = ramfs_node_info,
	.set_security = ramfs_node_set_security,
	.read = ramfs_node_read,
	.write = ramfs_node_write,
	.get_cache = ramfs_node_get_cache,
	.resize = ramfs_node_resize,
	.read_entry = ramfs_node_read_entry,
	.lookup_entry = ramfs_node_lookup_entry,
	.read_link = ramfs_node_read_link,
};

/** Unmount a RamFS.
 * @param mount		Mount that's being unmounted. */
static void ramfs_unmount(fs_mount_t *mount) {
	kfree(mount->data);
}

/** RamFS mount operations structure. */
static fs_mount_ops_t ramfs_mount_ops = {
	.unmount = ramfs_unmount,
};

/** Mount a RamFS filesystem.
 * @param mount		Mount structure for the FS.
 * @param opts		Array of mount options.
 * @param count		Number of options.
 * @return		Status code describing result of the operation. */
static status_t ramfs_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
	object_acl_t acl;
	object_security_t security = { 0, 0, &acl };
	ramfs_mount_t *data;
	ramfs_node_t *ndata;

	mount->ops = &ramfs_mount_ops,
	mount->data = data = kmalloc(sizeof(ramfs_mount_t), MM_SLEEP);
	mutex_init(&data->lock, "ramfs_mount_lock", 0);
	data->next_id = 1;

	/* Create the root directory, and add '.' and '..' entries. */
	ndata = kmalloc(sizeof(ramfs_node_t), MM_SLEEP);
	ndata->created = time_since_epoch();
	ndata->accessed = time_since_epoch();
	ndata->modified = time_since_epoch();
	ndata->entries = entry_cache_create(NULL, NULL);
	entry_cache_insert(ndata->entries, ".", 0);
	entry_cache_insert(ndata->entries, "..", 0);
	object_acl_init(&acl);
	object_acl_add_entry(&acl, ACL_ENTRY_USER, -1, DEFAULT_DIR_RIGHTS_OWNER);
	object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, -1, DEFAULT_DIR_RIGHTS_OTHERS);
	mount->root = fs_node_alloc(mount, 0, FILE_TYPE_DIR, &security, &ramfs_node_ops, ndata);
	return STATUS_SUCCESS;
}

/** RamFS filesystem type structure. */
static fs_type_t ramfs_fs_type = {
	.name = "ramfs",
	.description = "RAM-based temporary filesystem",
	.mount = ramfs_mount,
};

/** Register RamFS with the VFS. */
static void __init_text ramfs_init(void) {
	status_t ret;

	ret = fs_type_register(&ramfs_fs_type);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not register RamFS filesystem type (%d)", ret);
	}
}
INITCALL(ramfs_init);
