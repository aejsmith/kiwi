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
 * Internally, the filesystem works with nodes, identified by integer IDs.
 * Filesystem implementations handle the contents of directories and are
 * expected to provide a mapping from names to node IDs. A cache of known nodes
 * on a filesystem is maintained in order to quickly look up nodes. Known nodes
 * are also placed on used (some handle is open to the node) and unused lists.
 * Unused nodes are freed as needed in order to free up memory.
 *
 * Aside from the node cache, no caching is performed by the filesystem layer.
 * The filesystem implementation should implement caching using page caches,
 * entry caches and file maps if necessary.
 *
 * @todo		Should we flush nodes as they're moved to the unused
 *			list so we can evict unused nodes when trying to
 *			reclaim memory without having to flush them?
 */

#include <arch/memory.h>

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

#include <assert.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

/** Define to enable (very) verbose debug output. */
#define DEBUG_FS

#ifdef DEBUG_FS
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)
#endif

KBOOT_BOOLEAN_OPTION("force_fsimage", "Force filesystem image usage", false);

static file_ops_t fs_file_ops;

/** List of registered FS types. */
static LIST_DECLARE(fs_types);
static MUTEX_DECLARE(fs_types_lock, 0);

/** List of all mounts. */
static mount_id_t next_mount_id = 1;
static LIST_DECLARE(mount_list);
static MUTEX_DECLARE(mounts_lock, 0);

/** List of unused nodes (LRU first). */
static LIST_DECLARE(unused_nodes_list);
static size_t unused_nodes_count = 0;
static MUTEX_DECLARE(unused_nodes_lock, 0);

/** Cache of filesystem node structures. */
static slab_cache_t *fs_node_cache;

/** Mount at the root of the filesystem. */
fs_mount_t *root_mount = NULL;

/**
 * Filesystem type management functions.
 */

/** Look up a filesystem type with lock already held.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static fs_type_t *fs_type_lookup_unsafe(const char *name) {
	fs_type_t *type;

	LIST_FOREACH(&fs_types, iter) {
		type = list_entry(iter, fs_type_t, header);

		if(strcmp(type->name, name) == 0)
			return type;
	}

	return NULL;
}

/** Look up a filesystem type and reference it.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static fs_type_t *fs_type_lookup(const char *name) {
	fs_type_t *type;

	mutex_lock(&fs_types_lock);

	type = fs_type_lookup_unsafe(name);
	if(type)
		refcount_inc(&type->count);

	mutex_unlock(&fs_types_lock);
	return type;
}

#if 0
/** Determine which filesystem type a device contains.
 * @param handle	Handle to device to probe.
 * @param uuid		If not NULL, the filesystem's UUID will also be checked
 *			and a type will only be returned if the filesystem
 *			contains a recognised type AND has the specified UUID.
 * @return		Pointer to type structure, or NULL if not recognised.
 *			If found, type will be referenced. */
static fs_type_t *fs_type_probe(object_handle_t *handle, const char *uuid) {
	fs_type_t *type;

	mutex_lock(&fs_types_lock);

	LIST_FOREACH(&fs_types, iter) {
		type = list_entry(iter, fs_type_t, header);

		if(!type->probe) {
			continue;
		} else if(type->probe(handle, uuid)) {
			refcount_inc(&type->count);
			mutex_unlock(&fs_types_lock);
			return type;
		}
	}

	mutex_unlock(&fs_types_lock);
	return NULL;
}
#endif

/** Register a new filesystem type.
 * @param type		Pointer to type structure to register.
 * @return		Status code describing result of the operation. */
status_t fs_type_register(fs_type_t *type) {
	/* Check whether the structure is valid. */
	if(!type || !type->name || !type->description || !type->mount)
		return STATUS_INVALID_ARG;

	mutex_lock(&fs_types_lock);

	/* Check if this type already exists. */
	if(fs_type_lookup_unsafe(type->name)) {
		mutex_unlock(&fs_types_lock);
		return STATUS_ALREADY_EXISTS;
	}

	refcount_set(&type->count, 0);
	list_init(&type->header);
	list_append(&fs_types, &type->header);

	kprintf(LOG_NOTICE, "fs: registered filesystem type %s (%s)\n",
		type->name, type->description);
	mutex_unlock(&fs_types_lock);
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
	mutex_lock(&fs_types_lock);

	/* Check that the type is actually there. */
	if(fs_type_lookup_unsafe(type->name) != type) {
		mutex_unlock(&fs_types_lock);
		return STATUS_NOT_FOUND;
	} else if(refcount_get(&type->count) > 0) {
		mutex_unlock(&fs_types_lock);
		return STATUS_IN_USE;
	}

	list_remove(&type->header);
	mutex_unlock(&fs_types_lock);
	return STATUS_SUCCESS;
}

/**
 * Filesystem node functions.
 */

/**
 * Allocate a filesystem node structure.
 *
 * Allocates a new filesystem node structure. The node will have 1 reference
 * set on it. It will not be attached to the specified mount.
 *
 * @param mount		Mount that the node resides on.
 * @param id		ID of the node.
 * @param type		Type of the node.
 * @param ops		Pointer to operations structure for the node.
 * @param data		Implementation-specific data pointer.
 *
 * @return		Pointer to node structure allocated.
 */
fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, file_type_t type,
	fs_node_ops_t *ops, void *data)
{
	fs_node_t *node;

	assert(mount);
	assert(ops);

	node = slab_cache_alloc(fs_node_cache, MM_KERNEL);
	file_init(&node->file, &fs_file_ops, type);
	refcount_set(&node->count, 1);
	list_init(&node->mount_link);
	list_init(&node->unused_link);
	node->id = id;
	node->flags = 0;
	node->mounted = NULL;
	node->ops = ops;
	node->data = data;
	node->mount = mount;
	return node;
}

/** Flush changes to a node and free it.
 * @note		Never call this function unless it is necessary. Use
 *			fs_node_release().
 * @note		Mount lock must be held.
 * @param node		Node to free. Should be unused (zero reference count).
 * @return		Status code describing result of the operation. */
static status_t fs_node_free(fs_node_t *node) {
	status_t ret;

	assert(refcount_get(&node->count) == 0);
	assert(mutex_held(&node->mount->lock));

	/* Call the implementation to flush any changes and free up its data. */
	if(node->ops) {
		if(!FS_NODE_IS_READ_ONLY(node) && !(node->flags & FS_NODE_REMOVED)) {
			if(node->ops->flush) {
				ret = node->ops->flush(node);
				if(ret != STATUS_SUCCESS)
					return ret;
			}
		}

		if(node->ops->free)
			node->ops->free(node);
	}

	/* Detach it from the node tree/lists. */
	avl_tree_remove(&node->mount->nodes, &node->tree_link);
	list_remove(&node->mount_link);

	mutex_lock(&unused_nodes_lock);
	list_remove(&node->unused_link);
	unused_nodes_count--;
	mutex_unlock(&unused_nodes_lock);

	dprintf("fs: freed node %" PRIu16 ":%" PRIu64 " (%p)\n", node,
		node->mount->id, node->id);

	file_destroy(&node->file);
	slab_cache_free(fs_node_cache, node);
	return STATUS_SUCCESS;
}

#if 0
/** Reclaim space from the FS node cache.
 * @param level		Current resource level. */
static void fs_node_reclaim(int level) {
	fs_mount_t *mount;
	size_t count = 0;
	fs_node_t *node;
	status_t ret;

	mutex_lock(&unused_nodes_lock);

	/* Determine how many nodes to free based on the resource level. */
	switch(level) {
	case RESOURCE_LEVEL_ADVISORY:
		count = unused_nodes_count / 50;
		break;
	case RESOURCE_LEVEL_LOW:
		count = unused_nodes_count / 10;
		break;
	case RESOURCE_LEVEL_CRITICAL:
		count = unused_nodes_count;
		break;
	}

	/* Must do at least something. */
	if(!count) {
		count = 1;
	}

	/* Reclaim some nodes. */
	while(count-- && !list_empty(&unused_nodes_list)) {
		node = list_first(&unused_nodes_list, fs_node_t, unused_link);
		mutex_unlock(&unused_nodes_lock);

		/* Avoid a race condition: we must unlock the unused nodes list
		 * first to use the correct locking order, however this opens
		 * up the possibility that a node lookup gets hold of this node.
		 * Perform a reference count check to ensure this hasn't
		 * happened. */
		mount = node->mount;
		mutex_lock(&mount->lock);
		if(refcount_get(&node->count) > 0) {
			mutex_unlock(&mount->lock);
			count++;
			continue;
		}

		/* Free the node. If this fails, place the node back on the end
		 * of the list, but do not increment the count. This ensures
		 * that we do not get stuck in an infinite loop trying to free
		 * this node if it's going to continually fail. */
		ret = fs_node_free(node);
		mutex_lock(&unused_nodes_lock);
		if(ret != STATUS_SUCCESS) {
			kprintf(LOG_WARN, "fs: failed to flush node %" PRIu16 ":%" PRIu64 " (%d)\n",
			        node->mount->id, node->id);
			if(!list_empty(&node->unused_link)) {
				list_append(&unused_nodes_list, &node->unused_link);
			}
		}

		mutex_unlock(&mount->lock);
	}

	mutex_unlock(&unused_nodes_lock);
}
#endif

/** Look up a node in the filesystem.
 * @param path		Path string to look up.
 * @param node		Node to begin lookup at (referenced). Ignored if path
 *			is absolute.
 * @param follow	Whether to follow last path component if it is a
 *			symbolic link.
 * @param nest		Symbolic link nesting count.
 * @param nodep		Where to store pointer to node found (referenced).
 * @return		Status code describing result of the operation. */
static status_t fs_node_lookup_internal(char *path, fs_node_t *node, bool follow,
	unsigned nest, fs_node_t **nodep)
{
	fs_node_t *prev = NULL;
	fs_mount_t *mount;
	char *tok, *link;
	node_id_t id;
	status_t ret;

	/* Check whether the path is an absolute path. */
	if(path[0] == '/') {
		/* Drop the node we were provided, if any. */
		if(node != NULL)
			fs_node_release(node);

		/* Strip off all '/' characters at the start of the path. */
		while(path[0] == '/')
			path++;

		/* Get the root node of the current process. */
		assert(curr_proc->ioctx.root_dir);
		node = curr_proc->ioctx.root_dir;
		fs_node_retain(node);

		assert(node->file.type == FILE_TYPE_DIR);

		/* Return the root node if we've reached the end of the path. */
		if(!path[0]) {
			*nodep = node;
			return STATUS_SUCCESS;
		}
	} else {
		assert(node->file.type == FILE_TYPE_DIR);
	}

	/* Loop through each element of the path string. */
	while(true) {
		tok = strsep(&path, "/");

		/* If the node is a symlink and this is not the last element
		 * of the path, or the caller wishes to follow the link, follow
		 * it. */
		if(node->file.type == FILE_TYPE_SYMLINK && (tok || follow)) {
			/* The previous node should be the link's parent. */
			assert(prev);
			assert(prev->file.type == FILE_TYPE_DIR);

			/* Check whether the nesting count is too deep. */
			if(++nest > FS_NESTED_LINK_MAX) {
				fs_node_release(prev);
				fs_node_release(node);
				return STATUS_SYMLINK_LIMIT;
			}

			/* Obtain the link destination. */
			assert(node->ops->read_symlink);
			ret = node->ops->read_symlink(node, &link);
			if(ret != STATUS_SUCCESS) {
				fs_node_release(prev);
				fs_node_release(node);
				return ret;
			}

			dprintf("fs: following symbolic link %" PRIu16 ":%" PRIu64
				" to %s\n", node->mount->id, node->id, link);

			/* Move up to the parent node. The previous iteration
			 * of the loop left a reference on the previous node
			 * for us. */
			fs_node_release(node);
			node = prev;

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. TODO:
			 * perhaps we should avoid recursion here, it's a bit
			 * dodgy on our small kernel stack. */
			ret = fs_node_lookup_internal(link, node, true, nest, &node);
			if(ret != STATUS_SUCCESS) {
				kfree(link);
				return ret;
			}

			dprintf("fs: followed %s to %" PRIu16 ":%" PRIu64 "\n",
				link, node->mount->id, node->id);
			kfree(link);
		} else if(node->file.type == FILE_TYPE_SYMLINK) {
			/* The new node is a symbolic link but we do not want
			 * to follow it. We must release the previous node. */
			assert(prev != node);
			fs_node_release(prev);
		}

		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			*nodep = node;
			return STATUS_SUCCESS;
		} else if(node->file.type != FILE_TYPE_DIR) {
			/* The previous token was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			fs_node_release(node);
			return STATUS_NOT_DIR;
		} else if(!tok[0]) {
			/* Zero-length path component, do nothing. */
			continue;
		}

		/* We're trying to descend into the directory, check for
		 * execute permission. */
		if(!(object_rights(&node->file.obj, NULL) & FILE_RIGHT_EXECUTE)) {
			fs_node_release(node);
			return STATUS_ACCESS_DENIED;
		}

		/* Special handling for descending out of the directory. Other
		 * than these cases we rely on the filesystem implementation to
		 * handle it. */
		if(tok[0] == '.' && tok[1] == '.' && !tok[2]) {
			/* Do not allow the lookup to ascend past the process'
			 * root directory. */
			if(node == curr_proc->ioctx.root_dir)
				continue;

			assert(node != root_mount->root);
			if(node == node->mount->root) {
				assert(node->mount->mountpoint);
				assert(node->mount->mountpoint->file.type == FILE_TYPE_DIR);

				/* We're at the root of the mount, and the path
				 * wants to move to the parent. Using the '..'
				 * directory entry in the filesystem won't work
				 * in this case. Switch node to point to the
				 * mountpoint of the mount and then go through
				 * the normal lookup mechanism to get the '..'
				 * entry of the mountpoint. It is safe to use
				 * fs_node_retain() here - mountpoints will
				 * always have at least one reference. */
				prev = node;
				node = prev->mount->mountpoint;
				fs_node_retain(node);
				fs_node_release(prev);
			}
		}

		/* Look up this name within the directory. */
		ret = node->ops->lookup(node, tok, &id);
		if(ret != STATUS_SUCCESS) {
			fs_node_release(node);
			return ret;
		}

		/* If the ID is the same as the current node (e.g. the '.'
		 * entry), do nothing. */
		if(id == node->id)
			continue;

		/* Acquire the mount lock. */
		mount = node->mount;
		mutex_lock(&mount->lock);

		prev = node;

		dprintf("fs: resolved '%s' in %" PRIu16 ":%" PRIu64 " to %" PRIu64
			"\n", tok, mount->id, node->id, id);

		/* Check if the node is cached in the mount. */
		node = avl_tree_lookup(&mount->nodes, id, fs_node_t, tree_link);
		if(node) {
			assert(node->mount == mount);

			/* Check if the node has a mount on top of it. Only
			 * need to do this if the node was cached because nodes
			 * with mounts on will always be in the cache. Note
			 * that fs_unmount() takes the parent mount lock before
			 * changing node->mounted, therefore it is protected as
			 * we hold the mount lock. */
			if(node->mounted) {
				node = node->mounted->root;

				/* No need to check for a list move, it will
				 * have at least one reference because of the
				 * mount on it. */
				refcount_inc(&node->count);
				mutex_unlock(&mount->lock);
			} else {
				/* Reference the node and lock it, and move it
				 * to the used list if it was unused before. */
				if(refcount_inc(&node->count) == 1) {
					list_append(&mount->used_nodes, &node->mount_link);

					mutex_lock(&unused_nodes_lock);
					list_remove(&node->unused_link);
					unused_nodes_count--;
					mutex_unlock(&unused_nodes_lock);
				}

				mutex_unlock(&mount->lock);
			}
		} else {
			/* Node is not in the cache. We must pull it into the
			 * cache from the filesystem. */
			assert(mount->ops->read_node);
			ret = mount->ops->read_node(mount, id, &node);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&mount->lock);
				fs_node_release(prev);
				return ret;
			}

			/* Attach the node to the node tree and used list. */
			avl_tree_insert(&mount->nodes, id, &node->tree_link);
			list_append(&mount->used_nodes, &node->mount_link);
			mutex_unlock(&mount->lock);
		}

		/* Do not release the previous node if the new node is a
		 * symbolic link, as the symbolic link lookup requires it. */
		if(node->file.type != FILE_TYPE_SYMLINK)
			fs_node_release(prev);
	}
}

/**
 * Look up a node in the filesystem.
 *
 * Looks up a node in the filesystem. If the path is a relative path (one that
 * does not begin with a '/' character), then it will be looked up relative to
 * the current directory in the current process' I/O context. Otherwise, the
 * starting '/' character will be taken off and the path will be looked up
 * relative to the current I/O context's root.
 *
 * @param path		Path string to look up.
 * @param follow	If the last path component refers to a symbolic link,
 *			specified whether to follow the link or return the node
 *			of the link itself.
 * @param type		Required node type (negative will not check the type).
 * @param nodep		Where to store pointer to node found (referenced).
 *
 * @return		Status code describing result of the operation.
 */
static status_t fs_node_lookup(const char *path, bool follow, int type, fs_node_t **nodep) {
	fs_node_t *node = NULL;
	status_t ret;
	char *dup;

	assert(path);
	assert(nodep);

	if(!path[0])
		return STATUS_INVALID_ARG;

	/* Take the I/O context lock for reading across the entire lookup to
	 * prevent other threads from changing the root directory of the process
	 * while the lookup is being performed. */
	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Start from the current directory if the path is relative. */
	if(path[0] != '/') {
		assert(curr_proc->ioctx.curr_dir);
		node = curr_proc->ioctx.curr_dir;
		fs_node_retain(node);
	}

	/* Duplicate path so that fs_node_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_KERNEL);

	/* Look up the path string. */
	ret = fs_node_lookup_internal(dup, node, follow, 0, &node);
	if(ret == STATUS_SUCCESS) {
		if(type >= 0 && node->file.type != (file_type_t)type) {
			if(type == FILE_TYPE_REGULAR) {
				ret = STATUS_NOT_REGULAR;
			} else if(type == FILE_TYPE_DIR) {
				ret = STATUS_NOT_DIR;
			} else if(type == FILE_TYPE_SYMLINK) {
				ret = STATUS_NOT_SYMLINK;
			} else {
				ret = STATUS_NOT_SUPPORTED;
			}

			fs_node_release(node);
		} else {
			*nodep = node;
		}
	}

	rwlock_unlock(&curr_proc->ioctx.lock);
	kfree(dup);
	return ret;
}

/** Increase the reference count of a node.
 * @note		Should not be used on unused nodes.
 * @param node		Node to increase reference count of. */
void fs_node_retain(fs_node_t *node) {
	if(refcount_inc(&node->count) == 1) {
		fatal("Retaining unused FS node %" PRIu16 ":%" PRIu64 " (%p)",
			node->mount->id, node->id, node);
	}
}

/**
 * Decrease the reference count of a node.
 *
 * Decreases the reference count of a filesystem node. If this causes the
 * node's count to become zero, then the node will be moved on to the mount's
 * unused node list. This function should be called when a node obtained via
 * fs_node_lookup() or referenced via fs_node_retain() is no longer required;
 * each call to those functions should be matched with a call to this function.
 *
 * @param node		Node to decrease reference count of.
 */
void fs_node_release(fs_node_t *node) {
	fs_mount_t *mount;
	status_t ret;

	mount = node->mount;
	mutex_lock(&mount->lock);

	if(refcount_dec(&node->count) == 0) {
		assert(!node->mounted);

		/* Node has no references remaining, move it to its mount's
		 * unused list. If the node is not attached to anything or is
		 * removed, then destroy it immediately. */
		if(!(node->flags & FS_NODE_REMOVED) && !list_empty(&node->mount_link)) {
			list_append(&mount->unused_nodes, &node->mount_link);

			mutex_lock(&unused_nodes_lock);
			list_append(&unused_nodes_list, &node->unused_link);
			unused_nodes_count++;
			mutex_unlock(&unused_nodes_lock);

			dprintf("fs: transferred node %" PRIu64 " (%p) to unused "
				"list of mount %" PRIu16 " (%p)\n", node->id, node,
				node->mount->id, node->mount);
		} else {
			/* This shouldn't fail - the only thing that can fail
			 * in fs_node_free() is flushing data. Since this node
			 * has been removed, this should not fail. FIXME: But
			 * removal can actually fail? */
			ret = fs_node_free(node);
			if(ret != STATUS_SUCCESS) {
				fatal("Could not destroy removed node %p (%d)",
					node, ret);
			}
		}
	}

	mutex_unlock(&mount->lock);
}

/**
 * Mark a filesystem node as removed.
 *
 * Marks a filesystem node as removed. This is to be used by filesystem
 * implementations to mark a node as removed when its link count reaches 0,
 * to cause the node to be removed from memory as soon as it is released.
 *
 * @param node		Node to mark as removed.
 */
void fs_node_remove(fs_node_t *node) {
	node->flags &= FS_NODE_REMOVED;
}

/** Common node creation code.
 * @param path		Path to node to create.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @param nodep		Where to store pointer to created node (can be NULL).
 * @return		Status code describing result of the operation. */
static status_t fs_node_create(const char *path, file_type_t type, const char *target,
	fs_node_t **nodep)
{
	fs_node_t *parent = NULL, *node = NULL;
	char *dir, *name;
	node_id_t id;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_KERNEL);
	name = kbasename(path, MM_KERNEL);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = STATUS_ALREADY_EXISTS;
		goto out;
	}

	dprintf("fs: create '%s': dirname = '%s', basename = '%s'\n", path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		ret = STATUS_ALREADY_EXISTS;
		goto out;
	}

	/* Look up the parent node. */
	ret = fs_node_lookup(dir, true, FILE_TYPE_DIR, &parent);
	if(ret != STATUS_SUCCESS)
		goto out;

	mutex_lock(&parent->mount->lock);

	/* Check if the name we're creating already exists. */
	ret = parent->ops->lookup(parent, name, &id);
	if(ret != STATUS_NOT_FOUND) {
		if(ret == STATUS_SUCCESS) {
			ret = STATUS_ALREADY_EXISTS;
		}
		goto out;
	}

	/* Check that we are on a writable filesystem, that we have write
	 * permission to the directory, and that the FS supports node
	 * creation. */
	if(FS_NODE_IS_READ_ONLY(parent)) {
		ret = STATUS_READ_ONLY;
		goto out;
	} else if(!(object_rights(&parent->file.obj, NULL) & FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	} else if(!parent->ops->create) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	ret = parent->ops->create(parent, name, type, target, &node);
	if(ret != STATUS_SUCCESS)
		goto out;

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&parent->mount->nodes, node->id, &node->tree_link);
	list_append(&parent->mount->used_nodes, &node->mount_link);

	dprintf("fs: created '%s': node %" PRIu64 " (%p) in %" PRIu64 " (%p) on %"
		PRIu16 " (%p)\n", path, node->id, node, parent->id, parent,
		parent->mount->id);

	if(nodep) {
		*nodep = node;
		node = NULL;
	}
out:
	if(parent) {
		mutex_unlock(&parent->mount->lock);
		fs_node_release(parent);
	}

	if(node)
		fs_node_release(node);

	kfree(dir);
	kfree(name);
	return ret;
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

#if 0
/** Get the name of a node in its parent directory.
 * @param parent	Directory containing node.
 * @param id		ID of node to get name of.
 * @param namep		Where to store pointer to string containing node name.
 * @return		Status code describing result of the operation. */
static status_t fs_node_name(fs_node_t *parent, node_id_t id, char **namep) {
	dir_entry_t *entry;
	offset_t index = 0;
	status_t ret;

	if(!parent->ops->read_dir)
		return STATUS_NOT_SUPPORTED;

	while(true) {
		ret = parent->ops->read_dir(parent, index++, &entry);
		if(ret != STATUS_SUCCESS)
			return ret;

		if(entry->id == id) {
			*namep = kstrdup(entry->name, MM_KERNEL);
			kfree(entry);
			return STATUS_SUCCESS;
		}

		kfree(entry);
	}
}

/** Get the path of a filesystem node.
 * @todo		Implement this for files.
 * @param node		Node to get path to.
 * @param root		Node to take as the root of the FS tree.
 * @param pathp		Where to store pointer to kmalloc()'d path string.
 * @return		Status code describing result of the operation. */
static status_t fs_node_path(fs_node_t *node, fs_node_t *root, char **pathp) {
	char *buf = NULL, *tmp, *name, path[3];
	size_t len = 0;
	node_id_t id;
	status_t ret;

	fs_node_retain(node);

	/* Loop through until we reach the root. */
	while(node != root && node != root_mount->root) {
		/* Save the current node's ID. Use the mountpoint ID if this is
		 * the root of the mount. */
		id = (node == node->mount->root) ? node->mount->mountpoint->id : node->id;

		/* Get the parent of the node. */
		strcpy(path, "..");
		ret = fs_node_lookup_internal(path, node, false, 0, &node);
		if(ret != STATUS_SUCCESS) {
			node = NULL;
			goto fail;
		} else if(unlikely(node->type != FILE_TYPE_DIR)) {
			kprintf(LOG_WARN, "fs: node %" PRIu16 ":%" PRIu64 " (%p) "
				"should be a directory but it isn't!\n",
				node->mount->id, node->id, node);
			ret = STATUS_NOT_DIR;
			goto fail;
		}

		/* Look up the name of the child in this directory. */
		ret = fs_node_name(node, id, &name);
		if(ret != STATUS_SUCCESS)
			goto fail;

		/* Add the entry name on to the beginning of the path. */
		len += ((buf) ? strlen(name) + 1 : strlen(name));
		tmp = kmalloc(len + 1, MM_KERNEL);
		strcpy(tmp, name);
		kfree(name);
		if(buf) {
			strcat(tmp, "/");
			strcat(tmp, buf);
			kfree(buf);
		}
		buf = tmp;
	}

	fs_node_release(node);

	/* Prepend a '/'. */
	tmp = kmalloc((++len) + 1, MM_KERNEL);
	strcpy(tmp, "/");
	if(buf) {
		strcat(tmp, buf);
		kfree(buf);
	}
	buf = tmp;

	*pathp = buf;
	return STATUS_SUCCESS;
fail:
	if(node)
		fs_node_release(node);

	kfree(buf);
	return ret;
}
#endif

/**
 * File operations.
 */

/** Close a FS handle.
 * @param file		File being closed.
 * @param handle	File handle structure. */
static void fs_file_close(file_t *file, file_handle_t *handle) {
	fs_node_t *node = (fs_node_t *)file;

	if(node->ops->close)
		node->ops->close(node, handle);

	fs_node_release(node);
}

/** Signal that a file event is being waited for.
 * @param file		File being waited on.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t fs_file_wait(file_t *file, file_handle_t *handle, unsigned event, void *wait) {
	/* TODO. */
	return STATUS_NOT_IMPLEMENTED;
}

/** Stop waiting for a file.
 * @param file		File being waited on.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer. */
static void fs_file_unwait(file_t *file, file_handle_t *handle, unsigned event, void *wait) {
	/* TODO. */
}

/** Perform I/O on a file.
 * @param file		File to perform I/O on.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t fs_file_io(file_t *file, file_handle_t *handle, io_request_t *request) {
	fs_node_t *node = (fs_node_t *)file;

	if(!node->ops->io)
		return STATUS_NOT_SUPPORTED;

	return node->ops->io(node, handle, request);
}

/** Check if a file can be memory-mapped.
 * @param file		File being mapped.
 * @param handle	File handle structure.
 * @param protection	Protection flags (VM_PROT_*).
 * @param flags		Mapping flags (VM_MAP_*).
 * @return		STATUS_SUCCESS if can be mapped, status code explaining
 *			why if not. */
static status_t fs_file_mappable(file_t *file, file_handle_t *handle, uint32_t protection,
	uint32_t flags)
{
	fs_node_t *node = (fs_node_t *)file;

	return (node->ops->get_cache) ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

/** Get a page from the file.
 * @param file		File to get page from.
 * @param handle	File handle structure.
 * @param offset	Offset into file to get page from.
 * @param physp		Where to store physical address of page.
 * @return		Status code describing result of the operation. */
static status_t fs_file_get_page(file_t *file, file_handle_t *handle, offset_t offset,
	phys_ptr_t *physp)
{
	fs_node_t *node = (fs_node_t *)file;
	vm_cache_t *cache;

	cache = node->ops->get_cache(node, handle);
	return vm_cache_get_page(cache, offset, physp);
}

/** Release a page from the object.
 * @param file		File to release page in.
 * @param handle	File handle structure.
 * @param offset	Offset of page in file.
 * @param phys		Physical address of page that was unmapped. */
static void fs_file_release_page(file_t *file, file_handle_t *handle, offset_t offset,
	phys_ptr_t phys)
{
	fs_node_t *node = (fs_node_t *)file;
	vm_cache_t *cache;

	cache = node->ops->get_cache(node, handle);
	vm_cache_release_page(cache, offset, phys);
}

/** Read the next directory entry.
 * @param file		File to read from.
 * @param handle	File handle structure.
 * @param entryp	Where to store pointer to directory entry structure.
 * @return		Status code describing result of the operation. */
static status_t fs_file_read_dir(file_t *file, file_handle_t *handle, dir_entry_t **entryp) {
	fs_node_t *node, *child;
	dir_entry_t *entry;
	status_t ret;

	node = (fs_node_t *)file;

	if(!node->ops->read_dir)
		return STATUS_NOT_SUPPORTED;

	ret = node->ops->read_dir(node, handle, &entry);
	if(ret != STATUS_SUCCESS)
		return ret;

	mutex_lock(&node->mount->lock);

	/* Fix up the entry. */
	entry->mount = node->mount->id;
	if(node == node->mount->root && strcmp(entry->name, "..") == 0) {
		/* This is the '..' entry, and the node is the root of its
		 * mount. Change the node ID to be the ID of the mountpoint,
		 * if any. */
		if(node->mount->mountpoint) {
			ret = node->ops->lookup(node->mount->mountpoint, "..", &entry->id);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&node->mount->lock);
				kfree(entry);
				return ret;
			}

			entry->mount = node->mount->mountpoint->mount->id;
		}
	} else {
		/* Check if the entry refers to a mountpoint. In this case we
		 * need to change the node ID to be the node ID of the mount
		 * root, rather than the mountpoint. If the node the entry
		 * currently points to is not in the cache, then it won't be a
		 * mountpoint (mountpoints are always in the cache). */
		child = avl_tree_lookup(&node->mount->nodes, entry->id, fs_node_t, tree_link);
		if(child && child != node) {
			if(child->file.type == FILE_TYPE_DIR && child->mounted) {
				entry->id = child->mounted->root->id;
				entry->mount = child->mounted->id;
			}
		}
	}

	mutex_unlock(&node->mount->lock);

	*entryp = entry;
	return STATUS_SUCCESS;
}

/** Modify the size of a file.
 * @param file		File to resize.
 * @param handle	File handle structure.
 * @param size		New size of the file.
 * @return		Status code describing result of the operation. */
static status_t fs_file_resize(file_t *file, file_handle_t *handle, offset_t size) {
	fs_node_t *node = (fs_node_t *)file;

	if(!node->ops->resize)
		return STATUS_NOT_SUPPORTED;

	return node->ops->resize(node, size);
}

/** Get information about a file.
 * @param file		File to get information on.
 * @param handle	File handle structure.
 * @param info		Information structure to fill in. */
static void fs_file_info(file_t *file, file_handle_t *handle, file_info_t *info) {
	fs_node_t *node = (fs_node_t *)file;

	fs_node_info(node, info);
}

/** Flush changes to a file.
 * @param file		File to flush.
 * @param handle	File handle structure.
 * @return		Status code describing result of the operation. */
static status_t fs_file_sync(file_t *file, file_handle_t *handle) {
	fs_node_t *node = (fs_node_t *)file;

	if(!FS_NODE_IS_READ_ONLY(node) && node->ops->flush) {
		return node->ops->flush(node);
	} else {
		return STATUS_SUCCESS;
	}
}

/** FS file object operations. */
static file_ops_t fs_file_ops = {
	.close = fs_file_close,
	.wait = fs_file_wait,
	.unwait = fs_file_unwait,
	.io = fs_file_io,
	.mappable = fs_file_mappable,
	.get_page = fs_file_get_page,
	.release_page = fs_file_release_page,
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
status_t fs_open(const char *path, object_rights_t rights, uint32_t flags,
	unsigned create, object_handle_t **handlep)
{
	fs_node_t *node;
	void *data = NULL;
	status_t ret;

	assert(path);
	assert(handlep);

	if(create != FS_OPEN && create != FS_CREATE && create != FS_MUST_CREATE)
		return STATUS_INVALID_ARG;

	/* Look up the filesystem node. */
	ret = fs_node_lookup(path, true, -1, &node);
	if(ret != STATUS_SUCCESS) {
		/* If requested try to create the node. */
		if(ret == STATUS_NOT_FOUND && create > FS_OPEN) {
			ret = fs_node_create(path, FILE_TYPE_REGULAR, NULL, &node);
			if(ret != STATUS_SUCCESS)
				return ret;
		} else {
			return ret;
		}
	} else if(create == FS_MUST_CREATE) {
		fs_node_release(node);
		return STATUS_ALREADY_EXISTS;
	} else {
		/* FIXME: We should handle other types here too as well. Devices
		 * will eventually be redirected to the device layer, pipes
		 * should be openable and get directed into the pipe
		 * implementation. */
		if(node->file.type != FILE_TYPE_REGULAR && node->file.type != FILE_TYPE_DIR) {
			fs_node_release(node);
			return STATUS_NOT_SUPPORTED;
		}

		/* Check for correct access rights. We don't do this when we
		 * have first created the file: we allow the requested access
		 * regardless of the ACL upon first creation. TODO: The read-
		 * only FS check should be moved to the access() hook when ACLs
		 * are implemented. */
		if(rights && (object_rights(&node->file.obj, NULL) & rights) != rights) {
			fs_node_release(node);
			return STATUS_ACCESS_DENIED;
		} else if(rights & FILE_RIGHT_WRITE && FS_NODE_IS_READ_ONLY(node)) {
			fs_node_release(node);
			return STATUS_READ_ONLY;
		}
	}

	/* Call the FS' open hook, if any. */
	if(node->ops->open) {
		ret = node->ops->open(node, flags, &data);
		if(ret != STATUS_SUCCESS) {
			fs_node_release(node);
			return ret;
		}
	}

	*handlep = file_handle_create(&node->file, rights, flags, data);
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
	return fs_node_create(path, FILE_TYPE_DIR, NULL, NULL);
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
status_t fs_create_symlink(const char *path, const char *target) {
	return fs_node_create(path, FILE_TYPE_SYMLINK, target, NULL);
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
status_t fs_read_symlink(const char *path, char *buf, size_t size) {
	fs_node_t *node;
	status_t ret;
	char *dest;
	size_t len;

	assert(path);
	assert(buf);

	if(!size)
		return STATUS_TOO_SMALL;

	/* Find the link node. */
	ret = fs_node_lookup(path, false, FILE_TYPE_SYMLINK, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(!node->ops->read_symlink) {
		fs_node_release(node);
		return STATUS_NOT_SUPPORTED;
	}

	/* Read the link destination. */
	ret = node->ops->read_symlink(node, &dest);
	fs_node_release(node);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Check that the provided buffer is large enough. */
	len = strlen(dest);
	if(len + 1 > size) {
		kfree(dest);
		return STATUS_TOO_SMALL;
	}

	/* Copy the string across. */
	memcpy(buf, dest, len);
	buf[len] = 0;
	kfree(dest);
	return STATUS_SUCCESS;
}

/** Look up a mount by ID.
 * @note		Does not take the mount lock.
 * @param id		ID of mount to look up.
 * @return		Pointer to mount if found, NULL if not. */
static fs_mount_t *fs_mount_lookup(mount_id_t id) {
	fs_mount_t *mount;

	LIST_FOREACH(&mount_list, iter) {
		mount = list_entry(iter, fs_mount_t, header);
		if(mount->id == id) {
			return mount;
		}
	}

	return NULL;
}

/** Parse mount arguments.
 * @param str		Options string.
 * @param optsp		Where to store options structure array.
 * @param countp	Where to store number of arguments in array. */
static void parse_mount_opts(const char *str, fs_mount_option_t **optsp, size_t *countp) {
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

			opts = krealloc(opts, sizeof(*opts) * (count + 1), MM_KERNEL);
			opts[count].name = kstrdup(name, MM_KERNEL);
			opts[count].value = (value) ? kstrdup(value, MM_KERNEL) : NULL;
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

#if 0
/** Probe a device for filesystems.
 * @param device	Device to probe. */
void fs_probe(device_t *device) {
	kboot_tag_bootdev_t *bootdev;
	object_handle_t *handle;
	fs_type_t *type;
	status_t ret;
	char *path;

	if(device_get(device, DEVICE_RIGHT_READ, &handle) != STATUS_SUCCESS) {
		return;
	}

	/* Only probe for the boot FS at the moment. TODO: Notifications for
	 * filesystem detection. */
	if(!root_mount && !kboot_boolean_option("force_fsimage")) {
		bootdev = kboot_tag_iterate(KBOOT_TAG_BOOTDEV, NULL);
		if(bootdev && bootdev->type == KBOOT_BOOTDEV_DISK) {
			type = fs_type_probe(handle, (const char *)bootdev->disk.uuid);
			if(type) {
				path = device_path(device);
				ret = fs_mount(path, "/", type->name, NULL);
				if(ret != STATUS_SUCCESS) {
					fatal("Failed to mount boot filesystem (%d)", ret);
				}

				kprintf(LOG_NOTICE, "fs: mounted boot device %s:%s\n", type->name, path);
				refcount_dec(&type->count);
				kfree(path);
			}
		}
	}

	object_handle_release(handle);
}
#endif

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
	fs_mount_t *mount = NULL;
	fs_node_t *node = NULL;
	status_t ret;

	assert(path);
	assert(device || type);

	/* Parse the options string. */
	parse_mount_opts(opts, &opt_array, &opt_count);

	/* Lock the mount lock across the entire operation, so that only one
	 * mount can take place at a time. */
	mutex_lock(&mounts_lock);

	/* If the root filesystem is not yet mounted, the only place we can
	 * mount is '/'. */
	if(!root_mount) {
		assert(curr_proc == kernel_proc);
		if(strcmp(path, "/") != 0)
			fatal("Root filesystem is not yet mounted");
	} else {
		/* Look up the destination directory. */
		ret = fs_node_lookup(path, true, FILE_TYPE_DIR, &node);
		if(ret != STATUS_SUCCESS)
			goto fail;

		/* Check that it is not being used as a mount point already. */
		if(node->mount->root == node) {
			ret = STATUS_IN_USE;
			goto fail;
		}
	}

	/* Initialize the mount structure. */
	mount = kmalloc(sizeof(*mount), MM_KERNEL);
	mutex_init(&mount->lock, "fs_mount_lock", 0);
	avl_tree_init(&mount->nodes);
	list_init(&mount->used_nodes);
	list_init(&mount->unused_nodes);
	list_init(&mount->header);
	mount->flags = flags;
	mount->device = NULL;
	mount->root = NULL;
	mount->mountpoint = node;
	mount->type = NULL;

	/* If a type is specified, look it up. */
	if(type) {
		mount->type = fs_type_lookup(type);
		if(!mount->type) {
			ret = STATUS_NOT_FOUND;
			goto fail;
		}
	}

	/* Look up the device if the type needs one or we need to probe. */
	if(!type || mount->type->probe) {
		if(!device) {
			ret = STATUS_INVALID_ARG;
			goto fail;
		}

		#if 0
		/* Only request write access if not mounting read-only. */
		rights = DEVICE_RIGHT_READ;
		if(!(flags & FS_MOUNT_RDONLY)) {
			rights |= DEVICE_RIGHT_WRITE;
		}

		ret = device_open(device, rights, &mount->device);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		if(!type) {
			mount->type = fs_type_probe(mount->device, NULL);
			if(!mount->type) {
				ret = STATUS_UNKNOWN_FS;
				goto fail;
			}
		} else if(mount->type->probe) {
			/* Check if the device contains the type. */
			if(!mount->type->probe(mount->device, NULL)) {
				ret = STATUS_UNKNOWN_FS;
				goto fail;
			}
		}
		#endif

		fatal("TODO: Devices");
	}

	/* Allocate a mount ID. */
	if(next_mount_id == UINT16_MAX) {
		ret = STATUS_FS_FULL;
		goto fail;
	}
	mount->id = next_mount_id++;

	/* Call the filesystem's mount operation. */
	ret = mount->type->mount(mount, opt_array, opt_count);
	if(ret != STATUS_SUCCESS)
		goto fail;

	assert(mount->ops && mount->root);

	/* Put the root node into the node tree/used list. */
	avl_tree_insert(&mount->nodes, mount->root->id, &mount->root->tree_link);
	list_append(&mount->used_nodes, &mount->root->mount_link);

	/* Make the mountpoint point to the new mount. */
	if(mount->mountpoint)
		mount->mountpoint->mounted = mount;

	/* Store mount in mounts list and unlock the mount lock. */
	list_append(&mount_list, &mount->header);
	if(!root_mount) {
		root_mount = mount;

		/* Give the kernel process a correct current/root directory. */
		fs_node_retain(root_mount->root);
		curr_proc->ioctx.root_dir = root_mount->root;
		fs_node_retain(root_mount->root);
		curr_proc->ioctx.curr_dir = root_mount->root;
	}

	dprintf("fs: mounted %s:%s on %s (mount: %p, root: %p)\n", mount->type->name,
		(device) ? device : "<none>", path, mount, mount->root);

	mutex_unlock(&mounts_lock);
	free_mount_opts(opt_array, opt_count);
	return STATUS_SUCCESS;
fail:
	if(mount) {
		if(mount->device)
			object_handle_release(mount->device);

		if(mount->type)
			refcount_dec(&mount->type->count);

		kfree(mount);
	}

	if(node)
		fs_node_release(node);

	mutex_unlock(&mounts_lock);
	free_mount_opts(opt_array, opt_count);
	return ret;
}

/** Internal part of fs_unmount().
 * @param mount		Mount to unmount.
 * @param node		Node looked up for the unmount (can be NULL). Will be
 *			released when the function returns, even upon failure.
 * @return		Status code describing result of the operation. */
static status_t fs_unmount_internal(fs_mount_t *mount, fs_node_t *node) {
	fs_node_t *child;
	status_t ret;

	if(node) {
		if(node != mount->root) {
			fs_node_release(node);
			return STATUS_NOT_MOUNT;
		} else if(!mount->mountpoint && !shutdown_in_progress) {
			fs_node_release(node);
			return STATUS_IN_USE;
		}
	}

	/* Lock parent mount to ensure that the mount does not get looked up
	 * while we are unmounting. */
	if(mount->mountpoint)
		mutex_lock(&mount->mountpoint->mount->lock);

	mutex_lock(&mount->lock);

	/* If a lookup was performed, get rid of the reference it added. */
	if(node) {
		if(refcount_dec(&node->count) != 1) {
			assert(refcount_get(&node->count));
			ret = STATUS_IN_USE;
			goto fail;
		}
	}

	/* Check if any nodes are in use. */
	if(mount->root->mount_link.next != &mount->used_nodes
		|| mount->root->mount_link.prev != &mount->used_nodes)
	{
		ret = STATUS_IN_USE;
		goto fail;
	}

	/* Flush and free all nodes in the unused list. */
	LIST_FOREACH_SAFE(&mount->unused_nodes, iter) {
		child = list_entry(iter, fs_node_t, mount_link);

		ret = fs_node_free(child);
		if(ret != STATUS_SUCCESS)
			goto fail;
	}

	/* Free the root node itself. */
	refcount_dec(&mount->root->count);
	ret = fs_node_free(mount->root);
	if(ret != STATUS_SUCCESS) {
		refcount_inc(&mount->root->count);
		goto fail;
	}

	/* Detach from the mountpoint. */
	if(mount->mountpoint) {
		mount->mountpoint->mounted = NULL;
		mutex_unlock(&mount->mountpoint->mount->lock);
		fs_node_release(mount->mountpoint);
	}

	if(mount->ops->unmount)
		mount->ops->unmount(mount);

	if(mount->device)
		object_handle_release(mount->device);

	refcount_dec(&mount->type->count);

	list_remove(&mount->header);
	mutex_unlock(&mount->lock);
	kfree(mount);
	return STATUS_SUCCESS;
fail:
	mutex_unlock(&mount->lock);
	if(mount->mountpoint)
		mutex_unlock(&mount->mountpoint->mount->lock);

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
	fs_node_t *node;
	status_t ret;

	assert(path);

	mutex_lock(&mounts_lock);

	/* Look up the destination directory. */
	ret = fs_node_lookup(path, true, FILE_TYPE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mounts_lock);
		return ret;
	}

	ret = fs_unmount_internal(node->mount, node);
	mutex_unlock(&mounts_lock);
	return ret;
}

/** Get information about a filesystem entry.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t fs_info(const char *path, bool follow, file_info_t *info) {
	fs_node_t *node;
	status_t ret;

	assert(path);
	assert(info);

	ret = fs_node_lookup(path, follow, -1, &node);
	if(ret != STATUS_SUCCESS)
		return ret;

	fs_node_info(node, info);
	fs_node_release(node);
	return STATUS_SUCCESS;
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
	fs_node_t *parent = NULL, *node = NULL;
	char *dir, *name;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_KERNEL);
	name = kbasename(path, MM_KERNEL);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = STATUS_IN_USE;
		goto out;
	}

	dprintf("fs: unlink '%s': dirname = '%s', basename = '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	ret = fs_node_lookup(dir, true, FILE_TYPE_DIR, &parent);
	if(ret != STATUS_SUCCESS)
		goto out;
	ret = fs_node_lookup_internal(name, parent, false, 0, &node);
	if(ret != STATUS_SUCCESS)
		goto out;

	/* Check whether the node can be unlinked. */
	if(!(object_rights(&parent->file.obj, NULL) & FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	} else if(FS_NODE_IS_READ_ONLY(node)) {
		ret = STATUS_READ_ONLY;
		goto out;
	} else if(parent->mount != node->mount) {
		ret = STATUS_IN_USE;
		goto out;
	} else if(!node->ops->unlink) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	ret = node->ops->unlink(parent, name, node);
out:
	if(node)
		fs_node_release(node);

	if(parent)
		fs_node_release(parent);

	kfree(dir);
	kfree(name);
	return ret;
}

/** Display details of a mount.
 * @param mount		Mount to display. */
static void print_mount(fs_mount_t *mount) {
	kdb_printf("%-5" PRIu16 " %-5d %-10s %-18p %-18p %-18p %-18p\n",
		mount->id, mount->flags, (mount->type) ? mount->type->name : "none",
		mount->ops, mount->data, mount->root, mount->mountpoint);
}

/** Print a list of mounts.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_mount(int argc, char **argv, kdb_filter_t *filter) {
	fs_mount_t *mount;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<addr>]\n\n", argv[0]);

		kdb_printf("Prints out a list of all mounted filesystems.\n");
		return KDB_SUCCESS;
	} else if(argc != 1 && argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	kdb_printf("%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
		"ID", "Flags", "Type", "Ops", "Data", "Root", "Mountpoint");
	kdb_printf("%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
		"==", "=====", "====", "===", "====", "====", "==========");

	if(argc == 2) {
		if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;

		mount = (fs_mount_t *)((ptr_t)val);
		print_mount(mount);
	} else {
		LIST_FOREACH(&mount_list, iter) {
			mount = list_entry(iter, fs_mount_t, header);
			print_mount(mount);
		}
	}

	return KDB_SUCCESS;
}

/** Display details of a node.
 * @param node		Node to display. */
static void print_node(fs_node_t *node) {
	kdb_printf("%-8" PRIu64 " %-5d 0x%-3x %-4d %-18p %-18p %p\n", node->id,
		refcount_get(&node->count), node->flags, node->file.type,
		node->ops, node->data, node->mount);
}

/** Print information about a node.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_node(int argc, char **argv, kdb_filter_t *filter) {
	fs_node_t *node = NULL;
	list_t *list = NULL;
	fs_mount_t *mount = NULL;
	uint64_t val;
	size_t idx;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [--unused|--used] <mount ID>\n", argv[0]);
		kdb_printf("       %s <mount ID> <node ID>\n", argv[0]);
		kdb_printf("       %s <addr>\n\n", argv[0]);

		kdb_printf("Prints either a list of nodes on a mount, or details of a\n");
		kdb_printf("single filesystem node that's currently in memory.\n");
		return KDB_SUCCESS;
	} else if(argc != 2 && argc != 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Parse the arguments. */
	idx = (argc == 3 && argv[1][0] == '-') ? 2 : 1;
	if(kdb_parse_expression(argv[idx], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	if(val >= KERNEL_BASE) {
		node = (fs_node_t *)((ptr_t)val);
	} else {
		mount = fs_mount_lookup(val);
		if(!mount) {
			kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
			return KDB_FAILURE;
		}

		if(argc == 3 && argv[1][0] != '-') {
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
		kdb_printf("mount:   %p - locked: %d (%" PRId32 ")\n",
			node->mount, atomic_get(&node->mount->lock.value),
			(node->mount->lock.holder) ? node->mount->lock.holder->id : -1);
		kdb_printf("ops:     %p\n", node->ops);
		kdb_printf("data:    %p\n", node->data);
		kdb_printf("flags:   0x%x\n", node->flags);
		kdb_printf("type:    %d\n", node->file.type);
		if(node->mounted) {
			kdb_printf("mounted: %p (%" PRIu16 ")\n", node->mounted,
				node->mounted->id);
		}
	} else {
		if(argc == 3) {
			if(strcmp(argv[1], "--unused") == 0) {
				list = &mount->unused_nodes;
			} else if(strcmp(argv[1], "--used") == 0) {
				list = &mount->used_nodes;
			} else {
				kdb_printf("Unrecognized argument '%s'.\n", argv[1]);
				return KDB_FAILURE;
			}
		}

		kdb_printf("%-8s %-5s %-5s %-4s %-18s %-18s %s\n",
			"ID", "Count", "Flags", "Type", "Ops", "Data", "Mount");
		kdb_printf("%-8s %-5s %-5s %-4s %-18s %-18s %s\n",
			"==", "=====", "=====", "====", "===", "====", "=====");

		if(list) {
			LIST_FOREACH(list, iter) {
				node = list_entry(iter, fs_node_t, mount_link);
				print_node(node);
			}
		} else {
			AVL_TREE_FOREACH(&mount->nodes, iter) {
				node = avl_tree_entry(iter, fs_node_t, tree_link);
				print_node(node);
			}
		}
	}

	return KDB_SUCCESS;
}

/** Initialize the filesystem layer. */
__init_text void fs_init(void) {
	fs_node_cache = slab_cache_create("fs_node_cache", sizeof(fs_node_t),
		0, NULL, NULL, NULL, 0, MM_BOOT);

	/* Register the KDB commands. */
	kdb_register_command("mount", "Print a list of mounted filesystems.",
		kdb_cmd_mount);
	kdb_register_command("node", "Display information about a filesystem node.",
		kdb_cmd_node);
}

/** Shut down the filesystem layer. */
void fs_shutdown(void) {
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
status_t kern_fs_open(const char *path, object_rights_t rights, uint32_t flags,
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

status_t kern_fs_create_fifo(const char *path) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
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
	status_t ret;

	if(!path || !buf)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Allocate a buffer to read into. */
	kbuf = kmalloc(size, MM_USER);
	if(!kbuf) {
		kfree(kpath);
		return STATUS_NO_MEMORY;
	}

	ret = fs_read_symlink(kpath, kbuf, size);
	if(ret == STATUS_SUCCESS)
		ret = memcpy_to_user(buf, kbuf, size);

	kfree(kpath);
	kfree(kbuf);
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
#if 0
	mount_info_t *info = NULL;
	size_t i = 0, count = 0;
	fs_mount_t *mount;
	status_t ret;
	char *path;

	if(infos) {
		ret = memcpy_from_user(&count, countp, sizeof(count));
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(!count) {
			return STATUS_SUCCESS;
		}

		info = kmalloc(sizeof(*info), MM_KERNEL);
	}

	mutex_lock(&mounts_lock);

	LIST_FOREACH(&mount_list, iter) {
		if(infos) {
			mount = list_entry(iter, fs_mount_t, header);
			info->id = mount->id;
			strncpy(info->type, mount->type->name, ARRAY_SIZE(info->type));
			info->type[ARRAY_SIZE(info->type) - 1] = 0;

			/* Get the path of the mount. */
			ret = fs_node_path(mount->root, root_mount->root, &path);
			if(ret != STATUS_SUCCESS) {
				kfree(info);
				mutex_unlock(&mounts_lock);
				return ret;
			}

			strncpy(info->path, path, ARRAY_SIZE(info->path));
			info->path[ARRAY_SIZE(info->path) - 1] = 0;
			kfree(path);

			/* Get the device path. */
			if(mount->device) {
				path = device_path((device_t *)mount->device->object);
				strncpy(info->device, path, ARRAY_SIZE(info->device));
				info->device[ARRAY_SIZE(info->device) - 1] = 0;
				kfree(path);
			} else {
				info->device[0] = 0;
			}

			ret = memcpy_to_user(&infos[i], info, sizeof(*info));
			if(ret != STATUS_SUCCESS) {
				kfree(info);
				mutex_unlock(&mounts_lock);
				return ret;
			}

			if(++i >= count)
				break;
		} else {
			i++;
		}
	}

	mutex_unlock(&mounts_lock);

	if(infos)
		kfree(info);

	return memcpy_to_user(countp, &i, sizeof(i));
#endif
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

status_t kern_fs_path(const char *path, handle_t from, char *buf, size_t size) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to store in.
 * @param size		Size of buffer.
 * @return		Status code describing result of the operation. */
status_t kern_fs_curr_dir(char *buf, size_t size) {
#if 0
	status_t ret;
	size_t len;
	char *path;

	if(!buf)
		return STATUS_INVALID_ARG;

	rwlock_read_lock(&curr_proc->ioctx.lock);

	ret = fs_node_path(curr_proc->ioctx.curr_dir, curr_proc->ioctx.root_dir, &path);
	if(ret != STATUS_SUCCESS) {
		rwlock_unlock(&curr_proc->ioctx.lock);
		return ret;
	}

	rwlock_unlock(&curr_proc->ioctx.lock);

	len = strlen(path);
	if(len < size) {
		ret = memcpy_to_user(buf, path, len + 1);
	} else {
		ret = STATUS_TOO_SMALL;
	}

	kfree(path);
	return ret;
#endif
	return STATUS_NOT_IMPLEMENTED;
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		Status code describing result of the operation. */
status_t kern_fs_set_curr_dir(const char *path) {
	fs_node_t *node;
	status_t ret;
	char *kpath;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_node_lookup(kpath, true, FILE_TYPE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->file.obj, NULL) & FILE_RIGHT_EXECUTE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_ACCESS_DENIED;
	}

	/* Release after setting, it is retained by io_context_set_curr_dir(). */
	io_context_set_curr_dir(&curr_proc->ioctx, node);
	fs_node_release(node);
	kfree(kpath);
	return STATUS_SUCCESS;
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
	fs_node_t *node;
	status_t ret;
	char *kpath;

	if(!path)
		return STATUS_INVALID_ARG;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = fs_node_lookup(kpath, true, FILE_TYPE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->file.obj, NULL) & FILE_RIGHT_EXECUTE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_ACCESS_DENIED;
	}

	/* Release after setting, it is retained by io_context_set_root_dir(). */
	io_context_set_root_dir(&curr_proc->ioctx, node);
	fs_node_release(node);
	kfree(kpath);
	return STATUS_SUCCESS;
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

status_t kern_fs_link(const char *source, const char *dest) {
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

status_t kern_fs_rename(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_fs_sync(void) {
	return STATUS_NOT_IMPLEMENTED;
}
