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
 * @brief		Filesystem layer.
 *
 * There are two main components to the filesystem layer: the directory cache
 * and the node cache. The directory cache holds the filesystem's view of the
 * directory tree, and maps names within directories to nodes. When a directory
 * entry is unused (there are no open handles referring to it and it is not in
 * use by any lookup), it does not hold a valid node pointer. It only holds a
 * node ID. An unused entry is instantiated when it is reached by a lookup,
 * which causes the node it refers to to be looked up and its reference count
 * increased to 1.
 *
 * The node cache maps node IDs to node structures. Multiple directory entries
 * can refer to the same node. The node structure is mostly just a container
 * for data used by the filesystem implementation.
 *
 * The default behaviour of both the node cache and directory cache is to hold
 * entries that are not actually in use anywhere, in order to make lookups
 * faster. Unneeded entries are trimmed when the system is under memory pressure
 * in LRU order. Filesystem implementations can override this behaviour, to
 * either never free unused entries, or never keep them. The former behaviour
 * is used by ramfs, for example - it exists entirely within the filesystem
 * caches therefore must not free unused entries.
 *
 * Locking order:
 *  - Lock down the directory entry tree (i.e. parent before child).
 *  - Directory entry before mount.
 */

#include <io/device.h>
#include <io/fs.h>
#include <io/request.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>
#include <mm/vm_cache.h>

#include <proc/process.h>

#include <security/security.h>

#include <assert.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

/** Define to enable (very) verbose debug output. */
//#define DEBUG_FS

#ifdef DEBUG_FS
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)
#endif

/** Filesystem lookup behaviour flags. */
#define FS_LOOKUP_FOLLOW	(1<<0)	/**< If final path component is a symlink, follow it. */
#define FS_LOOKUP_LOCK		(1<<1)	/**< Return a locked entry. */

static file_ops_t fs_file_ops;

/** List of registered FS types (protected by fs_mount_lock). */
static LIST_DECLARE(fs_types);

/** List of all mounts. */
static mount_id_t next_mount_id = 1;
static LIST_DECLARE(fs_mount_list);
static MUTEX_DECLARE(fs_mount_lock, 0);

/** Cache of filesystem node structures. */
static slab_cache_t *fs_node_cache;
static slab_cache_t *fs_dentry_cache;

/** Mount at the root of the filesystem. */
fs_mount_t *root_mount = NULL;

/** Look up a filesystem type (fs_mount_lock must be held).
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static fs_type_t *fs_type_lookup(const char *name) {
	fs_type_t *type;

	LIST_FOREACH(&fs_types, iter) {
		type = list_entry(iter, fs_type_t, header);

		if(strcmp(type->name, name) == 0)
			return type;
	}

	return NULL;
}

/** Register a new filesystem type.
 * @param type		Pointer to type structure to register.
 * @return		Status code describing result of the operation. */
status_t fs_type_register(fs_type_t *type) {
	/* Check whether the structure is valid. */
	if(!type || !type->name || !type->description || !type->mount)
		return STATUS_INVALID_ARG;

	mutex_lock(&fs_mount_lock);

	/* Check if this type already exists. */
	if(fs_type_lookup(type->name)) {
		mutex_unlock(&fs_mount_lock);
		return STATUS_ALREADY_EXISTS;
	}

	refcount_set(&type->count, 0);
	list_init(&type->header);
	list_append(&fs_types, &type->header);

	kprintf(LOG_NOTICE, "fs: registered filesystem type %s (%s)\n",
		type->name, type->description);
	mutex_unlock(&fs_mount_lock);
	return STATUS_SUCCESS;
}

/**
 * Remove a filesystem type.
 *
 * Removes a previously registered filesystem type. Will not succeed if the
 * filesystem type is in use by any mounts.
 *
 * @param type		Type to remove.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_type_unregister(fs_type_t *type) {
	mutex_lock(&fs_mount_lock);

	/* Check that the type is actually there. */
	if(fs_type_lookup(type->name) != type) {
		mutex_unlock(&fs_mount_lock);
		return STATUS_NOT_FOUND;
	} else if(refcount_get(&type->count) > 0) {
		mutex_unlock(&fs_mount_lock);
		return STATUS_IN_USE;
	}

	list_remove(&type->header);
	mutex_unlock(&fs_mount_lock);
	return STATUS_SUCCESS;
}

/** Look up a mount by ID (fs_mount_lock must be held).
 * @param id		ID of mount to look up.
 * @return		Pointer to mount if found, NULL if not. */
static fs_mount_t *fs_mount_lookup(mount_id_t id) {
	fs_mount_t *mount;

	LIST_FOREACH(&fs_mount_list, iter) {
		mount = list_entry(iter, fs_mount_t, header);
		if(mount->id == id)
			return mount;
	}

	return NULL;
}

/**
 * Node functions.
 */

/** Allocate a node structure.
 * @param mount		Mount for the node.
 * @return		Pointer to allocated node structure. Reference count
 *			will be set to 1. */
static fs_node_t *fs_node_alloc(fs_mount_t *mount) {
	fs_node_t *node;

	node = slab_cache_alloc(fs_node_cache, MM_KERNEL);
	refcount_set(&node->count, 1);
	node->file.ops = &fs_file_ops;
	node->flags = 0;
	node->mount = mount;
	return node;
}

/**
 * Free an unused node.
 *
 * Free an unused node structure. The node's mount must be locked. If the node
 * is not marked as removed, the node's flush operation will be called, and the
 * node will not be freed if this fails. Removed nodes will always be freed
 * without error.
 *
 * @param node		Node to free.
 *
 * @return		Status code describing result of the operation. Cannot
 *			fail if node has FS_NODE_REMOVED set.
 */
static status_t fs_node_free(fs_node_t *node) {
	fs_mount_t *mount = node->mount;
	status_t ret;

	assert(refcount_get(&node->count) == 0);
	assert(mutex_held(&mount->lock));

	if(!fs_node_is_read_only(node) && !(node->flags & FS_NODE_REMOVED)) {
		if(node->ops->flush) {
			ret = node->ops->flush(node);
			if(ret != STATUS_SUCCESS)
				return ret;
		}
	}

	if(node->ops->free)
		node->ops->free(node);

	avl_tree_remove(&mount->nodes, &node->tree_link);

	dprintf("fs: freed node %" PRIu16 ":%" PRIu64 " (%p)\n", mount->id,
		node->id, node);

	slab_cache_free(fs_node_cache, node);
	return STATUS_SUCCESS;
}

/** Release a node.
 * @param node		Node to release. */
static void fs_node_release(fs_node_t *node) {
	fs_mount_t *mount = node->mount;

	if(refcount_dec(&node->count) > 0)
		return;

	/* Recheck after locking in case somebody has taken the node. */
	mutex_lock(&mount->lock);
	if(refcount_get(&node->count) > 0) {
		mutex_unlock(&mount->lock);
		return;
	}

	/* Free the node straight away if it is removed. */
	if(node->flags & FS_NODE_REMOVED)
		fs_node_free(node);

	/* TODO: Unused list. */

	mutex_unlock(&mount->lock);
}

/** Get information about a node.
 * @param node		Node to get information for.
 * @param info		Structure to store information in. */
static void fs_node_info(fs_node_t *node, file_info_t *info) {
	memset(info, 0, sizeof(*info));

	assert(node->ops->info);
	node->ops->info(node, info);

	info->id = node->id;
	info->mount = node->mount->id;
	info->type = node->file.type;
}

/**
 * Directory cache functions.
 */

/** Constructor for directory entry objects.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void fs_dentry_ctor(void *obj, void *data) {
	fs_dentry_t *entry = obj;

	mutex_init(&entry->lock, "fs_dentry_lock", 0);
	radix_tree_init(&entry->entries);
	list_init(&entry->unused_link);
}

/** Allocate a new directory entry structure.
 * @param name		Name of entry to create.
 * @param mount		Mount that the entry is on.
 * @param parent	Parent entry.
 * @return		Pointer to created entry structure. Reference count
 *			will be set to 0. */
static fs_dentry_t *fs_dentry_alloc(const char *name, fs_mount_t *mount,
	fs_dentry_t *parent)
{
	fs_dentry_t *entry;

	entry = slab_cache_alloc(fs_dentry_cache, MM_KERNEL);
	refcount_set(&entry->count, 0);
	entry->flags = 0;
	entry->name = kstrdup(name, MM_KERNEL);
	entry->mount = mount;
	entry->node = NULL;
	entry->parent = parent;
	entry->mounted = NULL;
	return entry;
}

/** Free a directory entry structure.
 * @param entry		Entry to free. */
static void fs_dentry_free(fs_dentry_t *entry) {
	dprintf("fs: freed entry '%s' (%p) on mount %" PRIu16 "\n", entry->name,
		entry, entry->mount->id);

	kfree(entry->name);
	slab_cache_free(fs_dentry_cache, entry);
}

/** Increase the reference count of a directory entry.
 * @note		Should not be used on unused entries.
 * @param entry		Entry to increase reference count of. */
void fs_dentry_retain(fs_dentry_t *entry) {
	if(unlikely(refcount_inc(&entry->count) == 1)) {
		fatal("Retaining unused directory entry %p ('%s')\n", entry,
			entry->name);
	}
}

/** Decrease the reference count of a locked directory entry.
 * @param entry		Entry to decrease reference count of. Will be unlocked
 *			upon return. */
static void fs_dentry_release_locked(fs_dentry_t *entry) {
	bool removed;

	if(refcount_dec(&entry->count) > 0) {
		mutex_unlock(&entry->lock);
		return;
	}

	assert(entry->node);
	assert(!entry->mounted);

	fs_node_release(entry->node);
	entry->node = NULL;

	removed = !entry->parent;
	mutex_unlock(&entry->lock);

	/* If the parent is NULL, that means we have been unlinked, therefore
	 * we should free the entry immediately. */
	if(removed)
		fs_dentry_free(entry);

	/* TODO: Move node and entry to unused lists. Don't if they have the
	 * keep flag set. Though, we do need used and unused lists per mount
	 * to aid in clean up when unmounting. */
}

/** Decrease the reference count of a directory entry.
 * @param entry		Entry to decrease reference count of. */
void fs_dentry_release(fs_dentry_t *entry) {
	mutex_lock(&entry->lock);
	fs_dentry_release_locked(entry);
}

/** Instantiate a directory entry.
 * @param entry		Entry to instantiate. Will be locked upon return if
 *			successful.
 * @return		Status code describing result of the operation. */
static status_t fs_dentry_instantiate(fs_dentry_t *entry) {
	fs_mount_t *mount;
	fs_node_t *node;
	status_t ret;

	mutex_lock(&entry->lock);

	if(refcount_inc(&entry->count) != 1) {
		assert(entry->node);
		return STATUS_SUCCESS;
	}

	mount = entry->mount;
	mutex_lock(&mount->lock);

	/* Check if the node is cached in the mount. */
	node = avl_tree_lookup(&mount->nodes, entry->id, fs_node_t, tree_link);
	if(node) {
		refcount_inc(&node->count);
	} else {
		/* Node is not cached, we must read it from the filesystem. */
		assert(mount->ops->read_node);

		node = fs_node_alloc(entry->mount);
		node->id = entry->id;

		ret = mount->ops->read_node(mount, node);
		if(ret != STATUS_SUCCESS) {
			slab_cache_free(fs_node_cache, node);
			mutex_unlock(&mount->lock);
			mutex_unlock(&entry->lock);
			return ret;
		}

		/* Attach the node to the node tree. */
		avl_tree_insert(&mount->nodes, node->id, &node->tree_link);
	}

	mutex_unlock(&mount->lock);
	entry->node = node;
	return STATUS_SUCCESS;
}

/**
 * Look up a child entry in a directory.
 *
 * Looks up a child entry in a directory, looking it up on the filesystem if it
 * cannot be found. This function does not handle '.' and '..' entries, an
 * assertion exists to check that these are not passed. Symbolic links are not
 * followed.
 *
 * @param parent	Entry to look up in (must be instantiated and locked).
 * @param name		Name of the entry to look up.
 * @param entryp	Where to store pointer to entry structure.
 *			Will not be instantiated, call fs_dentry_instantiate()
 *			after successful return.
 *
 * @return		Status code describing the result of the operation.
 */
static status_t fs_dentry_lookup(fs_dentry_t *parent, const char *name,
	fs_dentry_t **entryp)
{
	fs_dentry_t *entry;
	status_t ret;

	assert(mutex_held(&parent->lock));
	assert(parent->node);
	assert(strcmp(name, ".") != 0);
	assert(strcmp(name, "..") != 0);

	entry = radix_tree_lookup(&parent->entries, name);
	if(!entry) {
		if(!parent->node->ops->lookup)
			return STATUS_NOT_FOUND;

		entry = fs_dentry_alloc(name, parent->mount, parent);

		ret = parent->node->ops->lookup(parent->node, entry);
		if(ret != STATUS_SUCCESS) {
			slab_cache_free(fs_dentry_cache, entry);
			return ret;
		}

		radix_tree_insert(&parent->entries, name, entry);
	}

	*entryp = entry;
	return STATUS_SUCCESS;
}

/** Look up an entry in the filesystem.
 * @param path		Path string to look up (will be modified).
 * @param entry		Instantiated entry to begin lookup at (NULL for current
 *			working directory). Will be released upon return.
 * @param flags		Lookup behaviour flags.
 * @param nest		Symbolic link nesting count.
 * @param entryp	Where to store pointer to entry found (referenced,
 *			and locked).
 * @return		Status code describing result of the operation. */
static status_t fs_lookup_internal(char *path, fs_dentry_t *entry,
	unsigned flags, unsigned nest, fs_dentry_t **entryp)
{
	fs_dentry_t *prev;
	fs_node_t *node;
	char *tok, *link;
	bool follow;
	status_t ret;

	if(path[0] == '/') {
		/* Drop the entry we were provided, if any. */
		if(entry)
			fs_dentry_release(entry);

		/* Strip off all '/' characters at the start of the path. */
		while(path[0] == '/')
			path++;

		/* Start from the root directory of the current process. */
		assert(curr_proc->ioctx.root_dir);
		entry = curr_proc->ioctx.root_dir;
		fs_dentry_retain(entry);

		if(path[0] || flags & FS_LOOKUP_LOCK)
			mutex_lock(&entry->lock);

		/* Return the root if we've reached the end of the path. */
		if(!path[0]) {
			*entryp = entry;
			return STATUS_SUCCESS;
		}
	} else {
		if(!entry) {
			/* Start from the current working directory. */
			assert(curr_proc->ioctx.curr_dir);
			entry = curr_proc->ioctx.curr_dir;
			fs_dentry_retain(entry);
		}

		mutex_lock(&entry->lock);
	}

	/* Loop through each element of the path string. The starting entry
	 * should already be instantiated. */
	prev = NULL;
	while(true) {
		assert(entry->node);
		node = entry->node;
		tok = strsep(&path, "/");

		/* If the current entry is a symlink and this is not the last
		 * element of the path, or the caller wishes to follow the link,
		 * follow it. */
		follow = tok || flags & FS_LOOKUP_FOLLOW;
		if(node->file.type == FILE_TYPE_SYMLINK && follow) {
			/* The previous entry should be the link's parent. */
			assert(prev);
			assert(prev == entry->parent);

			if(++nest > FS_NESTED_LINK_MAX) {
				ret = STATUS_SYMLINK_LIMIT;
				goto err_release_prev;
			}

			assert(node->ops->read_symlink);
			ret = node->ops->read_symlink(node, &link);
			if(ret != STATUS_SUCCESS)
				goto err_release_prev;

			dprintf("fs: following symbolic link '%s' (%" PRIu16
				":%" PRIu64 ") in '%s' (%" PRIu64 ":%" PRIu16
				") to '%s' (nest: %u)\n", entry->name,
				entry->mount->id, node->id, prev->name,
				prev->mount->id, prev->node->id, nest);

			/* Don't need this entry any more. The previous
			 * iteration of the loop left a reference on the
			 * previous entry. */
			fs_dentry_release_locked(entry);

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. TODO:
			 * although we have a limit on this, perhaps it would
			 * be better to avoid recursion altogether. */
			ret = fs_lookup_internal(link, prev,
				FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK,
				nest, &entry);
			if(ret != STATUS_SUCCESS) {
				kfree(link);
				return ret;
			}

			/* Entry is locked and instantiated upon return. */
			assert(entry->node);
			node = entry->node;

			dprintf("fs: followed '%s' to '%s' (%" PRIu16 ":%"
				PRIu64 ")\n", link, entry->name,
				entry->mount->id, node->id);

			kfree(link);
		} else if(node->file.type == FILE_TYPE_SYMLINK) {
			/* Release the previous entry. */
			fs_dentry_release(prev);
		}

		if(!tok) {
			/* The last token was the last element of the path
			 * string, return the entry we're currently on. */
			if(!(flags & FS_LOOKUP_LOCK))
				mutex_unlock(&entry->lock);
			*entryp = entry;
			return STATUS_SUCCESS;
		} else if(node->file.type != FILE_TYPE_DIR) {
			/* The previous token was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			ret = STATUS_NOT_DIR;
			goto err_release;
		} else if(!tok[0] || (tok[0] == '.' && !tok[1])) {
			/* Zero-length path component or current directory,
			 * do nothing. */
			continue;
		}

		/* We're trying to descend into the directory, check for
		 * execute permission. */
		if(!file_access(&node->file, FILE_RIGHT_EXECUTE)) {
			ret = STATUS_ACCESS_DENIED;
			goto err_release;
		}

		prev = entry;

		if(tok[0] == '.' && tok[1] == '.' && !tok[2]) {
			/* Do not allow the lookup to ascend past the process'
			 * root directory. */
			if(entry == curr_proc->ioctx.root_dir)
				continue;

			assert(entry != root_mount->root);

			if(entry == entry->mount->root) {
				/* We're at the root of the mount. The entry
				 * parent pointer is NULL in this case. Move
				 * over onto the mountpoint's parent. */
				entry = entry->mount->mountpoint->parent;
			} else {
				entry = entry->parent;
			}
		} else {
			/* Try to find the entry in the child. */
			ret = fs_dentry_lookup(entry, tok, &entry);
			if(ret != STATUS_SUCCESS)
				goto err_release;

			if(entry->mounted)
				entry = entry->mounted->root;
		}

		/* TODO: If unused, should pull off unused list before
		 * unlocking parent so that it can't get freed between the
		 * unlock and relock in instantiate. Not in fs_dentry_lookup(),
		 * not right for unlink. */
		mutex_unlock(&prev->lock);

		ret = fs_dentry_instantiate(entry);
		if(ret != STATUS_SUCCESS) {
			/* TODO: Then should move to unused list here. */
			fs_dentry_release(prev);
			return ret;
		}

		/* Do not release the previous entry if the new node is a
		 * symbolic link, as the symbolic link lookup requires it. */
		if(entry->node->file.type != FILE_TYPE_SYMLINK)
			fs_dentry_release(prev);
	}

err_release_prev:
	fs_dentry_release(prev);
err_release:
	fs_dentry_release_locked(entry);
	return ret;
}

/**
 * Look up a filesystem entry.
 *
 * Looks up an entry in the filesystem. If the path is a relative path (one
 * that does not begin with a '/' character), then it will be looked up
 * relative to the current directory in the current process' I/O context.
 * Otherwise, the starting '/' character will be taken off and the path will be
 * looked up relative to the current I/O context's root.
 *
 * @param path		Path string to look up.
 * @param flags		Lookup behaviour flags.
 * @param entryp	Where to store pointer to entry found (instantiated).
 *
 * @return		Status code describing result of the operation.
 */
static status_t fs_lookup(const char *path, unsigned flags, fs_dentry_t **entryp) {
	char *dup;
	status_t ret;

	assert(path);
	assert(entryp);

	if(!path[0])
		return STATUS_INVALID_ARG;

	/* Take the I/O context lock for reading across the entire lookup to
	 * prevent other threads from changing the root directory of the process
	 * while the lookup is being performed. */
	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Duplicate path so that fs_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_KERNEL);

	/* Look up the path string. */
	ret = fs_lookup_internal(dup, NULL, flags, 0, entryp);
	kfree(dup);
	rwlock_unlock(&curr_proc->ioctx.lock);
	return ret;
}

/**
 * Internal implementation functions.
 */

/** Common creation code.
 * @param path		Path to node to create.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @param entryp	Where to store pointer to created entry (can be NULL).
 * @return		Status code describing result of the operation. */
static status_t fs_create(const char *path, file_type_t type,
	const char *target, fs_dentry_t **entryp)
{
	char *dir, *name;
	fs_dentry_t *parent, *entry;
	fs_node_t *node;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_KERNEL);
	name = kbasename(path, MM_KERNEL);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = STATUS_ALREADY_EXISTS;
		goto out_free_name;
	}

	dprintf("fs: create '%s': dirname = '%s', basename = '%s'\n",
		path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		ret = STATUS_ALREADY_EXISTS;
		goto out_free_name;
	}

	/* Look up the parent entry. */
	ret = fs_lookup(dir, FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK, &parent);
	if(ret != STATUS_SUCCESS)
		goto out_free_name;

	if(parent->node->file.type != FILE_TYPE_DIR) {
		ret = STATUS_NOT_DIR;
		goto out_release_parent;
	}

	/* Check if the name we're creating already exists. */
	ret = fs_dentry_lookup(parent, name, &entry);
	if(ret != STATUS_NOT_FOUND) {
		if(ret == STATUS_SUCCESS) {
			/* FIXME: Need to move back to unused list here as is
			 * not instantiated. Or perhaps not pull it off in
			 * fs_dentry_lookup() and do it in instantiate()? */
			ret = STATUS_ALREADY_EXISTS;
		}

		goto out_release_parent;
	}

	/* Check that we are on a writable filesystem, that we have write
	 * permission to the directory, and that the FS supports node
	 * creation. */
	if(fs_node_is_read_only(parent->node)) {
		ret = STATUS_READ_ONLY;
		goto out_release_parent;
	} else if(!file_access(&parent->node->file, FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out_release_parent;
	} else if(!parent->node->ops->create) {
		ret = STATUS_NOT_SUPPORTED;
		goto out_release_parent;
	}

	entry = fs_dentry_alloc(name, parent->mount, parent);
	node = fs_node_alloc(parent->mount);
	node->file.type = type;

	ret = parent->node->ops->create(parent->node, entry, node, target);
	if(ret != STATUS_SUCCESS)
		goto out_free_child;

	/* Attach the node to the mount. */
	mutex_lock(&parent->mount->lock);
	avl_tree_insert(&parent->mount->nodes, node->id, &node->tree_link);
	mutex_unlock(&parent->mount->lock);

	/* Instantiate the directory entry and attach to the parent. */
	refcount_set(&entry->count, 1);
	entry->node = node;
	radix_tree_insert(&parent->entries, name, entry);

	dprintf("fs: created '%s': node %" PRIu64 " (%p) in %" PRIu64 " (%p) "
		"on %" PRIu16 " (%p)\n", path, node->id, node, parent->node->id,
		parent->node, parent->mount->id);

	fs_dentry_release_locked(parent);

	if(entryp) {
		*entryp = entry;
	} else {
		fs_dentry_release(entry);
	}

	ret = STATUS_SUCCESS;
	goto out_free_name;

out_free_child:
	slab_cache_free(fs_node_cache, node);
	slab_cache_free(fs_dentry_cache, entry);
out_release_parent:
	fs_dentry_release_locked(parent);
out_free_name:
	kfree(dir);
	kfree(name);
	return ret;
}

/**
 * File operations.
 */

/** Close a FS handle.
 * @param handle	File handle structure. */
static void fs_file_close(file_handle_t *handle) {
	if(handle->node->ops->close)
		handle->node->ops->close(handle);

	/* Just release the directory entry, we don't have an extra reference
	 * on the node as the entry has one for us. */
	fs_dentry_release(handle->entry);
}

/** Signal that a file event is being waited for.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t fs_file_wait(file_handle_t *handle, unsigned event, void *wait) {
	/* TODO. */
	return STATUS_NOT_IMPLEMENTED;
}

/** Stop waiting for a file.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer. */
static void fs_file_unwait(file_handle_t *handle, unsigned event, void *wait) {
	/* TODO. */
}

/** Perform I/O on a file.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t fs_file_io(file_handle_t *handle, io_request_t *request) {
	return (handle->node->ops->io)
		? handle->node->ops->io(handle, request)
		: STATUS_NOT_SUPPORTED;
}

/** Map a file into memory.
 * @param handle	File handle structure.
 * @param region	Region being mapped.
 * @return		Status code describing result of the operation. */
static status_t fs_file_map(file_handle_t *handle, vm_region_t *region) {
	fs_node_t *node = (fs_node_t *)handle->file;

	if(!node->ops->get_cache)
		return STATUS_NOT_SUPPORTED;

	region->private = node->ops->get_cache(handle);
	region->ops = &vm_cache_region_ops;
	return STATUS_SUCCESS;
}

/** Read the next directory entry.
 * @param handle	File handle structure.
 * @param entryp	Where to store pointer to directory entry structure.
 * @return		Status code describing result of the operation. */
static status_t fs_file_read_dir(file_handle_t *handle, dir_entry_t **entryp) {
	dir_entry_t *entry;
	fs_mount_t *mount;
	fs_dentry_t *child;
	status_t ret;

	if(!handle->node->ops->read_dir)
		return STATUS_NOT_SUPPORTED;

	ret = handle->node->ops->read_dir(handle, &entry);
	if(ret != STATUS_SUCCESS)
		return ret;

	mutex_lock(&handle->entry->lock);
	mount = handle->entry->mount;

	/* Fix up the entry. */
	entry->mount = mount->id;
	if(handle->entry == mount->root && strcmp(entry->name, "..") == 0) {
		/* This is the '..' entry, and the directory is the root of its
		 * mount. Change the node and mount IDs to be those of the
		 * mountpoint, if any. */
		if(mount->mountpoint) {
			entry->id = mount->mountpoint->id;
			entry->mount = mount->mountpoint->mount->id;
		}
	} else {
		/* Check if the entry refers to a mountpoint. In this case we
		 * need to change the IDs to those of the mount root, rather
		 * than the mountpoint. If we don't have an entry in the cache
		 * with the same name as this entry, then it won't be a
		 * mountpoint (mountpoints are always in the cache). */
		child = radix_tree_lookup(&handle->entry->entries, entry->name);
		if(child && child->mounted) {
			entry->id = child->mounted->root->id;
			entry->mount = child->mounted->id;
		}
	}

	mutex_unlock(&handle->entry->lock);

	*entryp = entry;
	return STATUS_SUCCESS;
}

/** Modify the size of a file.
 * @param handle	File handle structure.
 * @param size		New size of the file.
 * @return		Status code describing result of the operation. */
static status_t fs_file_resize(file_handle_t *handle, offset_t size) {
	return (handle->node->ops->resize)
		? handle->node->ops->resize(handle->node, size)
		: STATUS_NOT_SUPPORTED;
}

/** Get information about a file.
 * @param handle	File handle structure.
 * @param info		Information structure to fill in. */
static void fs_file_info(file_handle_t *handle, file_info_t *info) {
	fs_node_info(handle->node, info);
}

/** Flush changes to a file.
 * @param handle	File handle structure.
 * @return		Status code describing result of the operation. */
static status_t fs_file_sync(file_handle_t *handle) {
	status_t ret = STATUS_SUCCESS;

	if(!fs_node_is_read_only(handle->node) && handle->node->ops->flush)
		ret = handle->node->ops->flush(handle->node);

	return ret;
}

/** FS file object operations. */
static file_ops_t fs_file_ops = {
	.close = fs_file_close,
	.wait = fs_file_wait,
	.unwait = fs_file_unwait,
	.io = fs_file_io,
	.map = fs_file_map,
	.read_dir = fs_file_read_dir,
	.resize = fs_file_resize,
	.info = fs_file_info,
	.sync = fs_file_sync,
};

/**
 * Public kernel interface.
 */

/**
 * Open a handle to a filesystem entry.
 *
 * Opens a handle to an entry in the filesystem, optionally creating it if it
 * doesn't exist. If the entry does not exist and it is specified to create it,
 * it will be created as a regular file.
 *
 * @param path		Path to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param create	Whether to create the file. If FS_OPEN, the file will
 *			not be created if it doesn't exist. If FS_CREATE, it
 *			will be created if it doesn't exist. If FS_MUST_CREATE,
 *			it must be created, and an error will be returned if it
 *			already exists.
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_open(const char *path, uint32_t rights, uint32_t flags,
	unsigned create, object_handle_t **handlep)
{
	fs_dentry_t *entry;
	fs_node_t *node;
	file_handle_t *handle;
	status_t ret;

	assert(path);
	assert(handlep);

	if(create != FS_OPEN && create != FS_CREATE && create != FS_MUST_CREATE)
		return STATUS_INVALID_ARG;

	/* Look up the filesystem entry. */
	ret = fs_lookup(path, FS_LOOKUP_FOLLOW, &entry);
	if(ret != STATUS_SUCCESS) {
		if(ret != STATUS_NOT_FOUND || create == FS_OPEN)
			return ret;

		/* Caller wants to create the node. */
		ret = fs_create(path, FILE_TYPE_REGULAR, NULL, &entry);
		if(ret != STATUS_SUCCESS)
			return ret;

		node = entry->node;
	} else if(create == FS_MUST_CREATE) {
		fs_dentry_release(entry);
		return STATUS_ALREADY_EXISTS;
	} else {
		node = entry->node;

		/* FIXME: We should handle other types here too as well. Devices
		 * will eventually be redirected to the device layer, pipes
		 * should be openable and get directed into the pipe
		 * implementation. */
		switch(node->file.type) {
		case FILE_TYPE_REGULAR:
		case FILE_TYPE_DIR:
			break;
		default:
			fs_dentry_release(entry);
			return STATUS_NOT_SUPPORTED;
		}

		/* Check for correct access rights. We don't do this when we
		 * have first created the file: we allow the requested access
		 * regardless of the ACL upon first creation. TODO: The read-
		 * only FS check should be moved to an access() hook when ACLs
		 * are implemented. */
		if(rights && !file_access(&node->file, rights)) {
			fs_dentry_release(entry);
			return STATUS_ACCESS_DENIED;
		} else if(rights & FILE_RIGHT_WRITE && fs_node_is_read_only(node)) {
			fs_dentry_release(entry);
			return STATUS_READ_ONLY;
		}
	}

	handle = file_handle_alloc(&node->file, rights, flags);
	handle->entry = entry;

	/* Call the FS' open hook, if any. */
	if(node->ops->open) {
		ret = node->ops->open(handle);
		if(ret != STATUS_SUCCESS) {
			file_handle_free(handle);
			fs_dentry_release(entry);
			return ret;
		}
	}

	*handlep = file_handle_create(handle);
	return STATUS_SUCCESS;
}

/**
 * Create a directory.
 *
 * Creates a new directory in the file system. This function cannot open a
 * handle to the created directory. The reason for this is that it is unlikely
 * that anything useful can be done on the new handle, for example reading
 * entries from a new directory will only give '.' and '..' entries.
 *
 * @param path		Path to directory to create.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_create_dir(const char *path) {
	return fs_create(path, FILE_TYPE_DIR, NULL, NULL);
}

/**
 * Create a FIFO.
 *
 * Creates a new FIFO in the filesystem. A FIFO is a named pipe. Opening it
 * with FILE_RIGHT_READ will give access to the read end, and FILE_RIGHT_WRITE
 * gives access to the write end.
 *
 * @param path		Path to FIFO to create.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_create_fifo(const char *path) {
	return fs_create(path, FILE_TYPE_FIFO, NULL, NULL);
}

/**
 * Create a symbolic link.
 *
 * Create a new symbolic link in the filesystem. The link target can be on any
 * mount (not just the same one as the link itself), and does not have to exist.
 * If it is a relative path, it is relative to the directory containing the
 * link.
 *
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_create_symlink(const char *path, const char *target) {
	return fs_create(path, FILE_TYPE_SYMLINK, target, NULL);
}

/**
 * Get the target of a symbolic link.
 *
 * Reads the target of a symbolic link and returns it as a pointer to a string
 * allocated with kmalloc(). Should be freed with kfree() when no longer
 * needed.
 *
 * @param path		Path to the symbolic link to read.
 * @param targetp	Where to store pointer to link target string.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_read_symlink(const char *path, char **targetp) {
	fs_dentry_t *entry;
	status_t ret;

	assert(path);
	assert(targetp);

	/* Find the link node. */
	ret = fs_lookup(path, 0, &entry);
	if(ret != STATUS_SUCCESS)
		return ret;

	if(entry->node->file.type != FILE_TYPE_SYMLINK) {
		fs_dentry_release(entry);
		return STATUS_NOT_SYMLINK;
	} else if(!entry->node->ops->read_symlink) {
		fs_dentry_release(entry);
		return STATUS_NOT_SUPPORTED;
	}

	ret = entry->node->ops->read_symlink(entry->node, targetp);
	fs_dentry_release(entry);
	return ret;
}

/** Parse mount arguments.
 * @param str		Options string.
 * @param optsp		Where to store options structure array.
 * @param countp	Where to store number of arguments in array. */
static void parse_mount_opts(const char *str, fs_mount_option_t **optsp,
	size_t *countp)
{
	fs_mount_option_t *opts = NULL;
	char *dup, *name, *value;
	size_t count = 0;

	if(str) {
		/* Duplicate the string to allow modification with strsep(). */
		dup = kstrdup(str, MM_KERNEL);

		while((value = strsep(&dup, ","))) {
			name = strsep(&value, "=");
			if(strlen(name) == 0) {
				continue;
			} else if(value && strlen(value) == 0) {
				value = NULL;
			}

			opts = krealloc(opts, sizeof(*opts) * (count + 1),
				MM_KERNEL);
			opts[count].name = kstrdup(name, MM_KERNEL);
			opts[count].value = (value)
				? kstrdup(value, MM_KERNEL) : NULL;
			count++;
		}

		kfree(dup);
	}

	*optsp = opts;
	*countp = count;
}

/** Free a mount options array.
 * @param opts		Array of options.
 * @param count		Number of options. */
static void free_mount_opts(fs_mount_option_t *opts, size_t count) {
	size_t i;

	for(i = 0; i < count; i++) {
		kfree((char *)opts[i].name);
		if(opts[i].value)
			kfree((char *)opts[i].value);
	}

	kfree(opts);
}

/**
 * Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 * The flags argument specifies generic mount options, the opts string is
 * passed into the filesystem driver to specify options specific to the
 * filesystem type.
 *
 * @param device	Device tree path to device filesystem resides on (can
 *			be NULL if the filesystem does not require a device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type (if not specified, device will
 *			be probed to determine the correct type - in this case,
 *			a device must be specified).
 * @param opts		Options string.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_mount(const char *device, const char *path, const char *type,
	uint32_t flags, const char *opts)
{
	fs_mount_option_t *opt_array;
	size_t opt_count;
	fs_dentry_t *mountpoint;
	fs_mount_t *mount;
	status_t ret;

	assert(path);
	assert(device || type);

	if(!security_check_priv(PRIV_FS_MOUNT))
		return STATUS_PERM_DENIED;

	/* Parse the options string. */
	parse_mount_opts(opts, &opt_array, &opt_count);

	/* Lock the mount lock across the entire operation, so that only one
	 * mount can take place at a time. */
	mutex_lock(&fs_mount_lock);

	/* If the root filesystem is not yet mounted, the only place we can
	 * mount is '/'. */
	if(!root_mount) {
		assert(curr_proc == kernel_proc);
		if(strcmp(path, "/") != 0)
			fatal("Root filesystem is not yet mounted");

		mountpoint = NULL;
	} else {
		/* Look up the destination mountpoint. */
		ret = fs_lookup(path, 0, &mountpoint);
		if(ret != STATUS_SUCCESS)
			goto err_unlock;

		/* Check that it is not being used as a mount point already. */
		if(mountpoint->mount->root == mountpoint) {
			ret = STATUS_IN_USE;
			goto err_release_mp;
		}
	}

	/* Initialize the mount structure. */
	mount = kmalloc(sizeof(*mount), MM_KERNEL | MM_ZERO);
	mutex_init(&mount->lock, "fs_mount_lock", 0);
	avl_tree_init(&mount->nodes);
	list_init(&mount->header);
	mount->flags = flags;
	mount->mountpoint = mountpoint;

	/* If a type is specified, look it up. */
	if(type) {
		mount->type = fs_type_lookup(type);
		if(!mount->type) {
			ret = STATUS_NOT_FOUND;
			goto err_free_mount;
		}
	}

	/* Look up the device if the type needs one or we need to probe. */
	if(!type || mount->type->probe) {
		if(!device) {
			ret = STATUS_INVALID_ARG;
			goto err_free_mount;
		}

		fatal("TODO: Devices");
	}

	/* Allocate a mount ID. */
	if(next_mount_id == UINT16_MAX) {
		ret = STATUS_FS_FULL;
		goto err_free_mount;
	}
	mount->id = next_mount_id++;

	/* Create root directory entry. It will be filled in by the FS' mount
	 * operation. */
	mount->root = fs_dentry_alloc("", mount, NULL);

	/* Call the filesystem's mount operation. */
	ret = mount->type->mount(mount, opt_array, opt_count);
	if(ret != STATUS_SUCCESS)
		goto err_free_root;

	assert(mount->ops);

	/* Get the root node. */
	ret = fs_dentry_instantiate(mount->root);
	if(ret != STATUS_SUCCESS)
		goto err_unmount;

	/* Instantiating leaves the entry locked. */
	mutex_unlock(&mount->root->lock);

	/* Make the mountpoint point to the new mount. */
	if(mount->mountpoint)
		mount->mountpoint->mounted = mount;

	refcount_inc(&mount->type->count);
	list_append(&fs_mount_list, &mount->header);
	if(!root_mount) {
		root_mount = mount;

		/* Give the kernel process a correct current/root directory. */
		fs_dentry_retain(root_mount->root);
		curr_proc->ioctx.root_dir = root_mount->root;
		fs_dentry_retain(root_mount->root);
		curr_proc->ioctx.curr_dir = root_mount->root;
	}

	dprintf("fs: mounted %s%s%s on %s (mount: %p, root: %p)\n",
		mount->type->name, (device) ? ":" : "",
		(device) ? device : "", path, mount, mount->root);

	mutex_unlock(&fs_mount_lock);
	free_mount_opts(opt_array, opt_count);
	return STATUS_SUCCESS;

err_unmount:
	if(mount->ops->unmount)
		mount->ops->unmount(mount);
err_free_root:
	slab_cache_free(fs_dentry_cache, mount->root);
err_free_mount:
	kfree(mount);
err_release_mp:
	if(mountpoint)
		fs_dentry_release(mountpoint);
err_unlock:
	mutex_unlock(&fs_mount_lock);
	free_mount_opts(opt_array, opt_count);
	return ret;
}

/**
 * Unmount a filesystem.
 *
 * Flushes all modifications to a filesystem if it is not read-only and
 * unmounts it. If any nodes in the filesystem are busy, then the operation
 * will fail.
 *
 * @param path		Path to mount point of filesystem.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_unmount(const char *path) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get information about a filesystem entry.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t fs_info(const char *path, bool follow, file_info_t *info) {
	fs_dentry_t *entry;
	status_t ret;

	assert(path);
	assert(info);

	ret = fs_lookup(path, (follow) ? FS_LOOKUP_FOLLOW : 0, &entry);
	if(ret != STATUS_SUCCESS)
		return ret;

	fs_node_info(entry->node, info);
	fs_dentry_release(entry);
	return STATUS_SUCCESS;
}

/**
 * Create a link on the filesystem.
 *
 * Creates a new hard link in the filesystem referring to the same underlying
 * node as the source link. Both paths must exist on the same mount. If the
 * source path refers to a symbolic link, the new link will refer to the node
 * pointed to by the symbolic link, not the symbolic link itself.
 *
 * @param source	Path to source.
 * @param dest		Path to new link.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_link(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
}

/**
 * Decrease the link count of a filesystem node.
 *
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path		Path to node to decrease link count of.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_unlink(const char *path) {
	char *dir, *name;
	fs_dentry_t *parent, *entry;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_KERNEL);
	name = kbasename(path, MM_KERNEL);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = STATUS_IN_USE;
		goto out_free_name;
	}

	dprintf("fs: unlink '%s': dirname = '%s', basename = '%s'\n",
		path, dir, name);

	if(strcmp(name, ".") == 0) {
		/* Trying to unlink '.' is invalid, it means "remove the '.'
		 * entry from the directory", rather than "remove the entry
		 * referring to the directory in the parent". */
		ret = STATUS_INVALID_ARG;
		goto out_free_name;
	} else if(strcmp(name, "..") == 0) {
		ret = STATUS_NOT_EMPTY;
		goto out_free_name;
	}

	/* Look up the parent entry. */
	ret = fs_lookup(dir, FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK, &parent);
	if(ret != STATUS_SUCCESS)
		goto out_free_name;

	if(parent->node->file.type != FILE_TYPE_DIR) {
		ret = STATUS_NOT_DIR;
		goto out_release_parent;
	}

	/* Look up the child entry. */
	ret = fs_dentry_lookup(parent, name, &entry);
	if(ret != STATUS_SUCCESS)
		goto out_release_parent;
	ret = fs_dentry_instantiate(entry);
	if(ret != STATUS_SUCCESS) {
		/* TODO: Move to unused list. */
		goto out_release_parent;
	}

	/* Check whether we can unlink the entry. */
	if(entry->mounted) {
		ret = STATUS_IN_USE;
		goto out_release_entry;
	} else if(fs_node_is_read_only(parent->node)) {
		ret = STATUS_READ_ONLY;
		goto out_release_entry;
	} else if(!file_access(&parent->node->file, FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out_release_entry;
	} else if(!parent->node->ops->unlink) {
		ret = STATUS_NOT_SUPPORTED;
		goto out_release_entry;
	}

	/* If the node being unlinked is a directory, check whether we have
	 * anything in the cache for it. While this is not a sufficient
	 * emptiness check (there may be entries we haven't got cached), it
	 * avoids a call out to the FS if we know that it is not empty already.
	 * Also, ramfs relies on this check being here, as it exists entirely
	 * in the cache. */
	if(!radix_tree_empty(&entry->entries)) {
		ret = STATUS_NOT_EMPTY;
		goto out_release_entry;
	}

	ret = parent->node->ops->unlink(parent->node, entry, entry->node);
	if(ret != STATUS_SUCCESS)
		goto out_release_entry;

	radix_tree_remove(&parent->entries, entry->name, NULL);
	entry->parent = NULL;
out_release_entry:
	fs_dentry_release_locked(entry);
out_release_parent:
	fs_dentry_release_locked(parent);
out_free_name:
	kfree(dir);
	kfree(name);
	return ret;
}

/**
 * Rename a link on the filesystem.
 *
 * Renames a link on the filesystem. This first creates a new link referring to
 * the same underlying filesystem node as the source link, and then removes
 * the source link. Both paths must exist on the same mount. If the specified
 * destination path exists, it is first removed.
 *
 * @param source	Path to original link.
 * @param dest		Path for new link.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_rename(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
}

/**
 * Flush all filesystem caches.
 *
 * Flushes all cached filesystem modifications that have yet to be written to
 * the disk.
 */
status_t fs_sync(void) {
	return STATUS_NOT_IMPLEMENTED;
}

/**
 * Debugger commands.
 */

/** Print information about mounted filesystems.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_mount(int argc, char **argv, kdb_filter_t *filter) {
	fs_mount_t *mount;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<mount ID|addr>]\n\n", argv[0]);

		kdb_printf("Given a mount ID or an address of a mount structure, prints out details of that\n");
		kdb_printf("mount, or given no arguments, prints out a list of all mounted filesystems.\n");
		return KDB_SUCCESS;
	} else if(argc != 1 && argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(argc == 2) {
		if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;

		if(val >= KERNEL_BASE) {
			mount = (fs_mount_t *)((ptr_t)val);
		} else {
			mount = fs_mount_lookup(val);
			if(!mount) {
				kdb_printf("Invalid mount ID.\n");
				return KDB_FAILURE;
			}
		}

		kdb_printf("Mount %p (%" PRIu16 ")\n", mount, mount->id);
		kdb_printf("=================================================\n");
		kdb_printf("type:       ");
		if (mount->type) {
			kdb_printf("%s (%s)\n", mount->type->name, mount->type->description);
		} else {
			kdb_printf("none\n");
		}
		kdb_printf("lock:       %d (%" PRId32 ")\n",
			atomic_get(&mount->lock.value),
			(mount->lock.holder) ? mount->lock.holder->id : -1);
		kdb_printf("flags:      0x%x\n", mount->flags);
		kdb_printf("ops:        %ps\n", mount->ops);
		kdb_printf("private:    %p\n", mount->private);
		kdb_printf("device:     %p\n", mount->device);
		kdb_printf("root:       %p\n", mount->root);
		kdb_printf("mountpoint: %p ('%s')\n", mount->mountpoint,
			(mount->mountpoint) ? mount->mountpoint->name : "<root>");
	} else {
		kdb_printf("ID  Type       Flags    Device             Mountpoint\n");
		kdb_printf("==  ====       =====    ======             ==========\n");

		LIST_FOREACH(&fs_mount_list, iter) {
			mount = list_entry(iter, fs_mount_t, header);

			kdb_printf("%-3" PRIu16 " %-10s 0x%-6x %-18p %p ('%s')\n",
				mount->id, (mount->type) ? mount->type->name : "none",
				mount->flags, mount->device, mount->mountpoint,
				(mount->mountpoint) ? mount->mountpoint->name : "<root>");
		}
	}

	return KDB_SUCCESS;
}

/** Display the children of a directory entry.
 * @param entry		Entry to start from.
 * @param descend	Whether to descend into children. */
static void dump_children(fs_dentry_t *entry, bool descend) {
	fs_dentry_t *child = NULL;
	fs_dentry_t *prev = NULL;
	unsigned depth = 0;

	kdb_printf("Entry              Count  Flags    Mount Node     Name\n");
	kdb_printf("=====              =====  =====    ===== ====     ====\n");

	/* We're in the debugger and descending through a potentially very
	 * large tree. Don't use recursion, we really don't want to overrun
	 * the stack. */
	while(true) {
		RADIX_TREE_FOREACH(&entry->entries, iter) {
			child = radix_tree_entry(iter, fs_dentry_t);

			if(prev) {
				if(child == prev)
					prev = NULL;
				child = NULL;
				continue;
			}

			kdb_printf("%-18p %-6d 0x%-6x %-5" PRId16 " %-8" PRIu64
				" %*s%s\n", child, refcount_get(&child->count),
				child->flags, (child->mount) ? child->mount->id : -1,
				child->id, depth * 2, "", child->name);

			if(!descend || radix_tree_empty(&child->entries)) {
				child = NULL;
				continue;
			}

			if(child->parent != entry) {
				kdb_printf("-- Incorrect parent %p\n", child->parent);
				child = NULL;
				continue;
			} else if(child->mounted) {
				if(child->mounted->mountpoint != entry) {
					kdb_printf("-- Incorrect mountpoint %p\n",
						child->mounted->mountpoint);
					child = NULL;
					continue;
				}

				child = child->mounted->root;
			}

			break;
		}

		if(child) {
			/* Go to child. */
			depth++;
			entry = child;
			prev = NULL;
		} else {
			/* Go back to parent. */
			if(depth == 0)
				return;

			if(entry == entry->mount->root) {
				prev = entry->mount->mountpoint;
			} else {
				prev = entry;
			}

			entry = prev->parent;
			depth--;
		}
	}
}

/** Print information about the directory cache.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_dentry(int argc, char **argv, kdb_filter_t *filter) {
	fs_dentry_t *entry;
	uint64_t val;
	int idx;
	bool descend = false;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [--descend] [<addr>]\n\n", argv[0]);

		kdb_printf("Given the address of a directory entry structure, prints out details of that\n");
		kdb_printf("entry. If the `--descend' argument is given, the entire directory cache tree\n");
		kdb_printf("below the given entry will be dumped rather than just its immediate children.\n");
		kdb_printf("Given no address, the starting point will be the root.\n");
		return KDB_SUCCESS;
	} else if(argc > 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(argc > 1 && argv[1][0] == '-') {
		if(strcmp(argv[1], "--descend") == 0) {
			descend = true;
		} else {
			kdb_printf("Unrecognized option. See 'help %s' for help.\n", argv[0]);
			return KDB_FAILURE;
		}

		idx = 2;
	} else {
		idx = 1;
	}

	if(idx < argc) {
		if(kdb_parse_expression(argv[idx], &val, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;

		entry = (fs_dentry_t *)((ptr_t)val);
	} else {
		entry = root_mount->root;
	}

	kdb_printf("Entry %p ('%s')\n", entry, entry->name);
	kdb_printf("=================================================\n");
	kdb_printf("lock:    %d (%" PRId32 ")\n",
		atomic_get(&entry->lock.value),
		(entry->lock.holder) ? entry->lock.holder->id : -1);
	kdb_printf("count:   %d\n", refcount_get(&entry->count));
	kdb_printf("flags:   0x%x\n", entry->flags);
	kdb_printf("mount:   %p%c", entry->mount, (entry->mount) ? ' ' : '\n');
	if(entry->mount)
		kdb_printf("(%" PRIu16 ")\n", entry->mount->id);
	kdb_printf("id:      %" PRIu64 "\n", entry->id);
	kdb_printf("node:    %p%c", entry->node, (entry->node) ? ' ' : '\n');
	if(entry->node)
		kdb_printf("(%" PRIu64 ")\n", entry->node->id);
	kdb_printf("parent:  %p%c", entry->parent, (entry->parent) ? ' ' : '\n');
	if(entry->parent)
		kdb_printf("('%s')\n", entry->parent->name);
	kdb_printf("mounted: %p%c", entry->mounted, (entry->mounted) ? ' ' : '\n');
	if(entry->mounted)
		kdb_printf("(%" PRIu16 ")\n", entry->mounted->id);

	if(!radix_tree_empty(&entry->entries)) {
		kdb_printf("\n");
		dump_children(entry, descend);
	}

	return KDB_SUCCESS;
}

/** Convert a file type to a string.
 * @param type		File type.
 * @return		String representation of file type. */
static inline const char *file_type_name(file_type_t type) {
	switch(type) {
	case FILE_TYPE_REGULAR:
		return "FILE_TYPE_REGULAR";
	case FILE_TYPE_DIR:
		return "FILE_TYPE_DIR";
	case FILE_TYPE_SYMLINK:
		return "FILE_TYPE_SYMLINK";
	case FILE_TYPE_BLOCK:
		return "FILE_TYPE_BLOCK";
	case FILE_TYPE_CHAR:
		return "FILE_TYPE_CHAR";
	case FILE_TYPE_FIFO:
		return "FILE_TYPE_FIFO";
	case FILE_TYPE_SOCKET:
		return "FILE_TYPE_SOCKET";
	default:
		return "Invalid";
	}
}

/** Print information about a node.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_node(int argc, char **argv, kdb_filter_t *filter) {
	fs_node_t *node = NULL;
	fs_mount_t *mount;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <mount ID>\n", argv[0]);
		kdb_printf("       %s <mount ID> <node ID>\n", argv[0]);
		kdb_printf("       %s <addr>\n\n", argv[0]);

		kdb_printf("The first form of this command prints a list of all nodes currently in memory\n");
		kdb_printf("for the specified mount. The second two forms prints details of a single node\n");
		kdb_printf("currently in memory, specified by either a mount ID and node ID pair, or the\n");
		kdb_printf("address of a node structure\n");
		return KDB_SUCCESS;
	} else if(argc != 2 && argc != 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	if(val >= KERNEL_BASE) {
		node = (fs_node_t *)((ptr_t)val);
	} else {
		mount = fs_mount_lookup(val);
		if(!mount) {
			kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
			return KDB_FAILURE;
		}

		if(argc == 3) {
			if(kdb_parse_expression(argv[2], &val, NULL) != KDB_SUCCESS)
				return KDB_FAILURE;

			node = avl_tree_lookup(&mount->nodes, val, fs_node_t, tree_link);
			if(!node) {
				kdb_printf("Unknown node ID %" PRIu64 ".\n", val);
				return KDB_FAILURE;
			}
		}
	}

	if(node) {
		/* Print out basic node information. */
		kdb_printf("Node %p (%" PRIu16 ":%" PRIu64 ")\n", node,
			node->mount->id, node->id);
		kdb_printf("=================================================\n");
		kdb_printf("count:   %d\n", refcount_get(&node->count));
		kdb_printf("type:    %d (%s)\n", node->file.type,
			file_type_name(node->file.type));
		kdb_printf("flags:   0x%x\n", node->flags);
		kdb_printf("ops:     %ps\n", node->ops);
		kdb_printf("private: %p\n", node->private);
		kdb_printf("mount:   %p%c", node->mount, (node->mount) ? ' ' : '\n');
		if(node->mount)
			kdb_printf("(%" PRIu16 ")\n", node->mount->id);
	} else {
		kdb_printf("ID       Count Flags    Type              Private\n");
		kdb_printf("==       ===== =====    ====              =======\n");

		AVL_TREE_FOREACH(&mount->nodes, iter) {
			node = avl_tree_entry(iter, fs_node_t, tree_link);

			kdb_printf("%-8" PRIu64 " %-5d 0x%-6x %-17s %p\n",
				node->id, refcount_get(&node->count), node->flags,
				file_type_name(node->file.type), node->private);
		}
	}

	return KDB_SUCCESS;
}

/** Initialize the filesystem layer. */
__init_text void fs_init(void) {
	fs_node_cache = object_cache_create("fs_node_cache", fs_node_t, NULL,
		NULL, NULL, 0, MM_BOOT);
	fs_dentry_cache = object_cache_create("fs_dentry_cache", fs_dentry_t,
		fs_dentry_ctor, NULL, NULL, 0, MM_BOOT);

	/* Register the KDB commands. */
	kdb_register_command("mount",
		"Display information about mounted filesystems.",
		kdb_cmd_mount);
	kdb_register_command("dentry",
		"Display information about the directory cache.",
		kdb_cmd_dentry);
	kdb_register_command("node",
		"Display information about a filesystem node.",
		kdb_cmd_node);
}

/** Shut down the filesystem layer. */
void fs_shutdown(void) {
#if 0
	fs_mount_t *mount;
	status_t ret;

	/* Drop references to the kernel process' root and current directories. */
	fs_node_release(curr_proc->ioctx.root_dir);
	curr_proc->ioctx.root_dir = NULL;
	fs_node_release(curr_proc->ioctx.curr_dir);
	curr_proc->ioctx.curr_dir = NULL;

	/* We must unmount all filesystems in the correct order, so that a FS
	 * will be unmounted before the FS that it is mounted on. This is
	 * actually easy to do: when a filesystem is mounted, it is appended to
	 * the mounts list. This means that the FS it is mounted on will always
	 * be before it in the list. So, we just need to iterate over the list
	 * in reverse. */
	LIST_FOREACH_REVERSE_SAFE(&mount_list, iter) {
		mount = list_entry(iter, fs_mount_t, header);

		ret = fs_unmount_internal(mount, NULL);
		if(ret != STATUS_SUCCESS) {
			if(ret == STATUS_IN_USE) {
				fatal("Mount %p in use during shutdown", mount);
			} else {
				fatal("Failed to unmount %p (%d)", mount, ret);
			}
		}
	}
#endif
}

/**
 * Open a handle to a filesystem entry.
 *
 * Opens a handle to an entry in the filesystem, optionally creating it if it
 * doesn't exist. If the entry does not exist and it is specified to create it,
 * it will be created as a regular file.
 *
 * @param path		Path to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param create	Whether to create the file. If FS_OPEN, the file will
 *			not be created if it doesn't exist. If FS_CREATE, it
 *			will be created if it doesn't exist. If FS_MUST_CREATE,
 *			it must be created, and an error will be returned if it
 *			already exists.
 * @param handlep	Where to store created handle.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_open(const char *path, uint32_t rights, uint32_t flags,
	unsigned create, handle_t *handlep)
{
	object_handle_t *handle;
	char *kpath = NULL;
	status_t ret;

	if(!path || !handlep)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_open(kpath, rights, flags, create, &handle);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(handle, NULL, handlep);
	object_handle_release(handle);
	kfree(kpath);
	return ret;
}

/**
 * Create a directory.
 *
 * Creates a new directory in the file system. This function cannot open a
 * handle to the created directory. The reason for this is that it is unlikely
 * that anything useful can be done on the new handle, for example reading
 * entries from a new directory will only give '.' and '..' entries.
 *
 * @param path		Path to directory to create.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_create_dir(const char *path) {
	char *kpath;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_create_dir(kpath);
	kfree(kpath);
	return ret;
}

/**
 * Create a FIFO.
 *
 * Creates a new FIFO in the filesystem. A FIFO is a named pipe. Opening it
 * with FILE_RIGHT_READ will give access to the read end, and FILE_RIGHT_WRITE
 * gives access to the write end.
 *
 * @param path		Path to FIFO to create.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_create_fifo(const char *path) {
	char *kpath;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_create_fifo(kpath);
	kfree(kpath);
	return ret;
}

/**
 * Create a symbolic link.
 *
 * Create a new symbolic link in the filesystem. The link target can be on any
 * mount (not just the same one as the link itself), and does not have to exist.
 * If it is a relative path, it is relative to the directory containing the
 * link.
 *
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_create_symlink(const char *path, const char *target) {
	char *kpath, *ktarget;
	status_t ret;

	if(!path || !target)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = strndup_from_user(target, FS_PATH_MAX, &ktarget);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = fs_create_symlink(kpath, ktarget);
	kfree(ktarget);
	kfree(kpath);
	return ret;
}

/**
 * Get the destination of a symbolic link.
 *
 * Reads the destination of a symbolic link into a buffer. A NULL byte will
 * always be placed at the end of the string.
 *
 * @param path		Path to the symbolic link to read.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer. If this is too small, the function will
 *			return STATUS_TOO_SMALL.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_read_symlink(const char *path, char *buf, size_t size) {
	char *kpath, *kbuf;
	size_t len;
	status_t ret;

	if(!path || !buf)
		return STATUS_INVALID_ARG;

	if(!size)
		return STATUS_TOO_SMALL;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_read_symlink(kpath, &kbuf);
	if(ret == STATUS_SUCCESS) {
		len = strlen(kbuf) + 1; 
		if(len > size) {
			ret = STATUS_TOO_SMALL;
		} else {
			ret = memcpy_to_user(buf, kbuf, len);
		}

		kfree(kbuf);
	}

	kfree(kpath);
	return ret;
}

/**
 * Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 * The flags argument specifies generic mount options, the opts string is
 * passed into the filesystem driver to specify options specific to the
 * filesystem type.
 *
 * @param device	Device tree path to device filesystem resides on (can
 *			be NULL if the filesystem does not require a device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type (if not specified, device will
 *			be probed to determine the correct type - in this case,
 *			a device must be specified).
 * @param opts		Options string.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_mount(const char *device, const char *path, const char *type,
	uint32_t flags, const char *opts)
{
	char *kdevice = NULL, *kpath = NULL, *ktype = NULL, *kopts = NULL;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	if(device) {
		ret = strndup_from_user(device, FS_PATH_MAX, &kdevice);
		if(ret != STATUS_SUCCESS)
			goto out;
	}

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		goto out;

	if(type) {
		ret = strndup_from_user(type, FS_PATH_MAX, &ktype);
		if(ret != STATUS_SUCCESS)
			goto out;
	}

	if(opts) {
		ret = strndup_from_user(opts, FS_PATH_MAX, &kopts);
		if(ret != STATUS_SUCCESS)
			goto out;
	}

	ret = fs_mount(kdevice, kpath, ktype, flags, kopts);
out:
	if(kdevice) kfree(kdevice);
	if(kpath) kfree(kpath);
	if(ktype) kfree(ktype);
	if(kopts) kfree(kopts);
	return ret;
}

/** Get information on mounted filesystems.
 * @param infos		Array of mount information structures to fill in. If
 *			NULL, the function will only return the number of
 *			mounted filesystems.
 * @param countp	If infos is not NULL, this should point to a value
 *			containing the size of the provided array. Upon
 *			successful completion, the value will be updated to
 *			be the number of structures filled in. If infos is NULL,
 *			the number of mounted filesystems will be stored here.
 * @return		Status code describing result of the operation. */
status_t kern_fs_mount_info(mount_info_t *infos, size_t *countp) {
	return STATUS_NOT_IMPLEMENTED;
}

/**
 * Unmounts a filesystem.
 *
 * Flushes all modifications to a filesystem if it is not read-only and
 * unmounts it. If any nodes in the filesystem are busy, then the operation
 * will fail.
 *
 * @param path		Path to mount point of filesystem.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_unmount(const char *path) {
	char *kpath;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_unmount(kpath);
	kfree(kpath);
	return ret;
}

/**
 * Get the path to a file or directory.
 *
 * Given a handle to a file or directory, this function will return the
 * absolute path that was used to open the handle. If the handle specified is
 * INVALID_HANDLE, the path to the current directory will be returned.
 *
 * @param handle	Handle to get path from.
 * @param buf		Buffer to write path string to.
 * @param size		Size of buffer. If this is too small, STATUS_TOO_SMALL
 *			will be returned.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_path(handle_t from, char *buf, size_t size) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		Status code describing result of the operation. */
status_t kern_fs_set_curr_dir(const char *path) {
	fs_dentry_t *entry;
	char *kpath;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_lookup(kpath, FS_LOOKUP_FOLLOW, &entry);
	if(ret != STATUS_SUCCESS) {
		goto out_free;
	} else if(entry->node->file.type != FILE_TYPE_DIR) {
		ret = STATUS_NOT_DIR;
		goto out_release;
	}

	/* Must have execute permission to use as working directory. */
	if(!file_access(&entry->node->file, FILE_RIGHT_EXECUTE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out_release;
	}

	/* Release after setting, it is retained by io_context_set_curr_dir(). */
	io_context_set_curr_dir(&curr_proc->ioctx, entry);
out_release:
	fs_dentry_release(entry);
out_free:
	kfree(kpath);
	return ret;
}

/**
 * Set the root directory.
 *
 * Sets both the current directory and the root directory for the calling
 * process to the directory specified. Any processes spawned by the process
 * after this call will also have the same root directory. Note that this
 * function is not entirely the same as chroot() on a UNIX system: it enforces
 * the new root by changing the current directory to it, and then does not let
 * the process ascend out of it using '..' in a path. On UNIX systems, however,
 * the root user is allowed to ascend out via '..'.
 *
 * @param path		Path to directory to change to.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_set_root_dir(const char *path) {
	fs_dentry_t *entry;
	char *kpath;
	status_t ret;

	if(!path)
		return STATUS_INVALID_ARG;

	if(!security_check_priv(PRIV_FS_SETROOT))
		return STATUS_PERM_DENIED;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_lookup(kpath, FS_LOOKUP_FOLLOW, &entry);
	if(ret != STATUS_SUCCESS) {
		goto out_free;
	} else if(entry->node->file.type != FILE_TYPE_DIR) {
		ret = STATUS_NOT_DIR;
		goto out_release;
	}

	/* Must have execute permission to use as working directory. */
	if(!file_access(&entry->node->file, FILE_RIGHT_EXECUTE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out_release;
	}

	/* Release after setting, it is retained by io_context_set_curr_dir(). */
	io_context_set_root_dir(&curr_proc->ioctx, entry);
out_release:
	fs_dentry_release(entry);
out_free:
	kfree(kpath);
	return ret;
}

/** Get information about a node.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t kern_fs_info(const char *path, bool follow, file_info_t *info) {
	file_info_t kinfo;
	status_t ret;
	char *kpath;

	if(!path || !info)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_info(kpath, follow, &kinfo);
	if(ret == STATUS_SUCCESS)
		ret = memcpy_to_user(info, &kinfo, sizeof(*info));

	kfree(kpath);
	return ret;
}

/**
 * Create a link on the filesystem.
 *
 * Creates a new hard link in the filesystem referring to the same underlying
 * node as the source link. Both paths must exist on the same mount. If the
 * source path refers to a symbolic link, the new link will refer to the node
 * pointed to by the symbolic link, not the symbolic link itself.
 *
 * @param source	Path to source.
 * @param dest		Path to new link.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_link(const char *source, const char *dest) {
	char *ksource, *kdest;
	status_t ret;

	if(!source || !dest)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(source, FS_PATH_MAX, &ksource);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = strndup_from_user(dest, FS_PATH_MAX, &kdest);
	if(ret != STATUS_SUCCESS) {
		kfree(ksource);
		return ret;
	}

	ret = fs_link(ksource, kdest);
	kfree(kdest);
	kfree(ksource);
	return ret;
}

/**
 * Decrease the link count of a filesystem node.
 *
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path		Path to node to decrease link count of.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_unlink(const char *path) {
	status_t ret;
	char *kpath;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_unlink(kpath);
	kfree(kpath);
	return ret;
}

/**
 * Rename a link on the filesystem.
 *
 * Renames a link on the filesystem. This first creates a new link referring to
 * the same underlying filesystem node as the source link, and then removes
 * the source link. Both paths must exist on the same mount. If the specified
 * destination path exists, it is first removed.
 *
 * @param source	Path to original link.
 * @param dest		Path for new link.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_rename(const char *source, const char *dest) {
	char *ksource, *kdest;
	status_t ret;

	if(!source || !dest)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(source, FS_PATH_MAX, &ksource);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = strndup_from_user(dest, FS_PATH_MAX, &kdest);
	if(ret != STATUS_SUCCESS) {
		kfree(ksource);
		return ret;
	}

	ret = fs_rename(ksource, kdest);
	kfree(kdest);
	kfree(ksource);
	return ret;
}

/**
 * Flush all filesystem caches.
 *
 * Flushes all cached filesystem modifications that have yet to be written to
 * the disk.
 */
status_t kern_fs_sync(void) {
	return fs_sync();
}
