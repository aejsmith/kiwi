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
 * @brief		Filesystem layer.
 *
 * @note		Mount locks should be taken before node locks. If a
 *			node lock is held and it is desired to lock its mount,
 *			you should unlock the node, lock the mount, then relock
 *			the node. If the node lock is taken first, a deadlock
 *			can occur (lock node, attempt to lock mount which
 *			blocks because node is being searched for, search
 *			attempts to lock node, deadlock).
 * @note		Handle locks should be taken before node locks.
 *
 * @todo		Could probably use an rwlock on nodes.
 * @todo		Node cache reclaim.
 */

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <kargs.h>
#include <kdbg.h>

#if CONFIG_FS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Data for a filesystem handle (both handle types need the same data). */
typedef struct fs_handle {
	rwlock_t lock;			/**< Lock to protect offset. */
	offset_t offset;		/**< Current file offset. */
	int flags;			/**< Flags the file was opened with. */
} fs_handle_t;

extern void fs_node_get(fs_node_t *node);
static int fs_dir_lookup(fs_node_t *node, const char *name, node_id_t *idp);
static int fs_symlink_cache(fs_node_t *node);
static object_type_t file_object_type;
static object_type_t dir_object_type;

/** List of registered FS types. */
static LIST_DECLARE(fs_types);
static MUTEX_DECLARE(fs_types_lock, 0);

/** List of all mounts. */
static mount_id_t next_mount_id = 1;
static LIST_DECLARE(mount_list);
static MUTEX_DECLARE(mounts_lock, 0);

/** Cache of filesystem node structures. */
static slab_cache_t *fs_node_cache;

/** Mount at the root of the filesystem. */
fs_mount_t *root_mount;

/** Look up a filesystem type with lock already held.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static fs_type_t *fs_type_lookup_internal(const char *name) {
	fs_type_t *type;

	LIST_FOREACH(&fs_types, iter) {
		type = list_entry(iter, fs_type_t, header);

		if(strcmp(type->name, name) == 0) {
			return type;
		}
	}

	return NULL;
}

/** Look up a filesystem type and reference it.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static fs_type_t *fs_type_lookup(const char *name) {
	fs_type_t *type;

	mutex_lock(&fs_types_lock);

	if((type = fs_type_lookup_internal(name))) {
		refcount_inc(&type->count);
	}

	mutex_unlock(&fs_types_lock);
	return type;
}

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

/** Register a new filesystem type.
 * @param type		Pointer to type structure to register.
 * @return		0 on success, negative error code on failure. */
int fs_type_register(fs_type_t *type) {
	/* Check whether the structure is valid. */
	if(!type || !type->name || !type->description || !type->mount) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&fs_types_lock);

	/* Check if this type already exists. */
	if(fs_type_lookup_internal(type->name) != NULL) {
		mutex_unlock(&fs_types_lock);
		return -ERR_ALREADY_EXISTS;
	}

	refcount_set(&type->count, 0);
	list_init(&type->header);
	list_append(&fs_types, &type->header);

	kprintf(LOG_NORMAL, "fs: registered filesystem type %s (%s)\n",
	        type->name, type->description);
	mutex_unlock(&fs_types_lock);
	return 0;
}

/** Remove a filesystem type.
 *
 * Removes a previously registered filesystem type. Will not succeed if the
 * filesystem type is in use by any mounts.
 *
 * @param type		Type to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_type_unregister(fs_type_t *type) {
	mutex_lock(&fs_types_lock);

	/* Check that the type is actually there. */
	if(fs_type_lookup_internal(type->name) != type) {
		mutex_unlock(&fs_types_lock);
		return -ERR_NOT_FOUND;
	} else if(refcount_get(&type->count) > 0) {
		mutex_unlock(&fs_types_lock);
		return -ERR_IN_USE;
	}

	list_remove(&type->header);
	mutex_unlock(&fs_types_lock);
	return 0;
}

/** FS node object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int fs_node_ctor(void *obj, void *data, int kmflag) {
	fs_node_t *node = obj;

	mutex_init(&node->lock, "fs_node_lock", 0);
	list_init(&node->mount_link);
	return 0;
}

/** Allocate a filesystem node structure.
 * @note		Does not attach to the mount.
 * @note		One reference will be set on the node.
 * @param mount		Mount that the node resides on.
 * @param id		ID of the node.
 * @param type		Type of the node.
 * @param ops		Pointer to operations structure for the node.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to node structure allocated. */
fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, fs_node_type_t type,
                         fs_node_ops_t *ops, void *data) {
	fs_node_t *node;

	node = slab_cache_alloc(fs_node_cache, MM_SLEEP);
	refcount_set(&node->count, 1);
	node->id = 0;
	node->type = type;
	node->flags = 0;
	node->mounted = NULL;
	node->ops = ops;
	node->data = data;
	node->mount = mount;
	node->data_cache = NULL;
	node->entry_cache = NULL;
	node->link_cache = NULL;

	/* Initialise the node's object header. */
	switch(type) {
	case FS_NODE_FILE:
		object_init(&node->obj, &file_object_type);
		break;
	case FS_NODE_DIR:
		object_init(&node->obj, &dir_object_type);
		break;
	default:
		object_init(&node->obj, NULL);
		break;
	}

	return node;
}

/** Flush all changes to a node.
 * @param node		Node to flush. Both the node and its mount should be
 *			locked.
 * @return		0 on success, negative error code on failure. If a
 *			failure occurs while flushing file data, the function
 *			carries on attempting to flush, but still returns an
 *			error. If multiple errors occur, it is the most recent
 *			that is returned. */
static int fs_node_flush(fs_node_t *node) {
	int ret = 0, err;

	assert(!node->mount || mutex_held(&node->mount->lock));
	assert(mutex_held(&node->lock));

	if(!FS_NODE_IS_RDONLY(node)) {
		if(node->type == FS_NODE_FILE && node->data_cache) {
			vm_cache_flush(node->data_cache);
		}
		if(node->ops->flush) {
			if((err = node->ops->flush(node)) != 0) {
				ret = err;
			}
		}
	}

	return ret;
}

/** Flush changes to a node and free it.
 * @note		Never call this function unless it is necessary. Use
 *			fs_node_release().
 * @note		Mount lock (if there is a mount) and node lock must be
 *			held. Mount lock will still be locked when the function
 *			returns (or both on failure).
 * @param node		Node to free. Should be unused (zero reference count).
 * @return		0 on success, negative error code on failure (this can
 *			happen if an error occurs flushing the node data). */
static int fs_node_free(fs_node_t *node) {
	int ret;

	assert(refcount_get(&node->count) == 0);
	assert(!node->mount || mutex_held(&node->mount->lock));
	assert(mutex_held(&node->lock));

	/* Flush and destroy the node's caches, if any. */
	switch(node->type) {
	case FS_NODE_FILE:
		if(node->data_cache) {
			if((ret = vm_cache_destroy(node->data_cache, node->flags & FS_NODE_REMOVED)) != 0) {
				return ret;
			}
			node->data_cache = NULL;
		}
		break;
	case FS_NODE_DIR:
		if(node->entry_cache) {
			radix_tree_clear(&node->entry_cache->entries, kfree);
			kfree(node->entry_cache);
			node->entry_cache = NULL;
		}
		break;
	case FS_NODE_SYMLINK:
		if(node->link_cache) {
			kfree(node->link_cache);
			node->link_cache = NULL;
		}
		break;
	default:
		break;
	}

	/* Call the implementation to flush any changes and free up its data. */
	if(!FS_NODE_IS_RDONLY(node) && !(node->flags & FS_NODE_REMOVED) && node->ops->flush) {
		if((ret = node->ops->flush(node)) != 0) {
			return ret;
		}
	}
	if(node->ops->free) {
		node->ops->free(node);
	}

	/* If the node has a mount, detach it from the node tree/lists. */
	if(node->mount) {
		avl_tree_remove(&node->mount->nodes, (key_t)node->id);
		list_remove(&node->mount_link);
	}

	object_destroy(&node->obj);
	dprintf("fs: freed node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
	        (node->mount) ? node->mount->id : 0, node->id);
	mutex_unlock(&node->lock);
	slab_cache_free(fs_node_cache, node);
	return 0;
}

/** Look up a node in the filesystem.
 * @param path		Path string to look up.
 * @param node		Node to begin lookup at (locked and referenced).
 *			Ignored if path is absolute.
 * @param follow	Whether to follow last path component if it is a
 *			symbolic link.
 * @param nest		Symbolic link nesting count.
 * @param nodep		Where to store pointer to node found (referenced and
 *			locked).
 * @return		0 on success, negative error code on failure. */
static int fs_node_lookup_internal(char *path, fs_node_t *node, bool follow, int nest,
                                   fs_node_t **nodep) {
	fs_node_t *prev = NULL, *tmp;
	fs_mount_t *mount;
	char *tok, *link;
	node_id_t id;
	int ret;

	/* Handle absolute paths here rather than in fs_node_lookup() because
	 * the symbolic link resolution code below calls this function directly
	 * rather than fs_node_lookup(). */
	if(path[0] == '/') {
		/* Drop the node we were provided, if any. */
		if(node != NULL) {
			mutex_unlock(&node->lock);
			fs_node_release(node);
		}

		/* Strip off all '/' characters at the start of the path. */
		while(path[0] == '/') {
                        path++;
                }

		/* Get the root node of the current process. */
		assert(curr_proc->ioctx.root_dir);
		node = curr_proc->ioctx.root_dir;
		mutex_lock(&node->lock);
		fs_node_get(node);

		assert(node->type == FS_NODE_DIR);

		/* Return the root node if the end of the path has been reached. */
		if(!path[0]) {
			*nodep = node;
			return 0;
		}
	} else {
		assert(node->type == FS_NODE_DIR);
	}

	/* Loop through each element of the path string. */
	while(true) {
		tok = strsep(&path, "/");

		/* If the node is a symlink and this is not the last element
		 * of the path, or the caller wishes to follow the link, follow
		 * it. */
		if(node->type == FS_NODE_SYMLINK && (tok || follow)) {
			/* The previous node should be the link's parent. */
			assert(prev);
			assert(prev->type == FS_NODE_DIR);

			/* Check whether the nesting count is too deep. */
			if(++nest > SYMLOOP_MAX) {
				mutex_unlock(&node->lock);
				fs_node_release(prev);
				fs_node_release(node);
				return -ERR_LINK_LIMIT;
			}

			/* Ensure that the link destination is cached. */
			if((ret = fs_symlink_cache(node)) != 0) {
				mutex_unlock(&node->lock);
				fs_node_release(prev);
				fs_node_release(node);
				return ret;
			}

			dprintf("fs: following symbolic link %" PRIu16 ":%" PRIu64 " to %s\n",
			        node->mount->id, node->id, node->link_cache);

			/* Duplicate the link destination as the lookup needs
			 * to modify it. */
			link = kstrdup(node->link_cache, MM_SLEEP);

			/* Move up to the parent node. The previous iteration
			 * of the loop left a reference on the previous node
			 * for us. */
			tmp = node; node = prev; prev = tmp;
			mutex_unlock(&prev->lock);
			mutex_lock(&node->lock);

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. */
			if((ret = fs_node_lookup_internal(link, node, true, nest, &node)) != 0) {
				fs_node_release(prev);
				kfree(link);
				return ret;
			}

			dprintf("fs: followed %s to %" PRIu16 ":%" PRIu64 "\n",
			        prev->link_cache, node->mount->id, node->id);
			kfree(link);
		}

		/* Previous node no longer needed. */
		if(prev) {
			mutex_unlock(&node->lock);
			fs_node_release(prev);
			mutex_lock(&node->lock);
		}

		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			*nodep = node;
			return 0;
		} else if(node->type != FS_NODE_DIR) {
			/* The previous token was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			mutex_unlock(&node->lock);
			fs_node_release(node);
			return -ERR_TYPE_INVAL;
		} else if(!tok[0]) {
			/* Zero-length path component, do nothing. */
			continue;
		} else if(tok[0] == '.' && tok[1] == '.' && !tok[2]) {
			if(node == curr_proc->ioctx.root_dir) {
				/* Do not allow the lookup to ascend past the
				 * process' root directory. */
				continue;
			}

			assert(node != root_mount->root);

			if(node == node->mount->root) {
				assert(node->mount->mountpoint);
				assert(node->mount->mountpoint->type == FS_NODE_DIR);

				/* We're at the root of the mount, and the path
				 * wants to move to the parent. Using the '..'
				 * directory entry in the filesystem won't work
				 * in this case. Switch node to point to the
				 * mountpoint of the mount and then go through
				 * the normal lookup mechanism to get the '..'
				 * entry of the mountpoint. It is safe to use
				 * fs_node_get() here - mountpoints will
				 * always have at least one reference. */
				prev = node;
				node = prev->mount->mountpoint;
				fs_node_get(node);
				mutex_unlock(&prev->lock);
				fs_node_release(prev);
				mutex_lock(&node->lock);
			}
		}

		/* Look up this name within the directory entry cache. */
		if((ret = fs_dir_lookup(node, tok, &id)) != 0) {
			mutex_unlock(&node->lock);
			fs_node_release(node);
			return ret;
		}

		/* If the ID is the same as the current node (e.g. the '.'
		 * entry), do nothing. */
		if(id == node->id) {
			continue;
		}

		/* Acquire the mount lock. See note in file header about
		 * locking order. */
		mount = node->mount;
		mutex_unlock(&node->lock);
		mutex_lock(&mount->lock);

		prev = node;

		dprintf("fs: looking for node %" PRIu64 " in cache for mount %" PRIu16 " (%s)\n",
		        id, mount->id, tok);

		/* Check if the node is cached in the mount. */
		node = avl_tree_lookup(&mount->nodes, (key_t)id);
		if(node) {
			assert(node->mount == mount);

			/* Check if the node has a mount on top of it. Only
			 * need to do this if the node was cached because nodes
			 * with mounts on will always be in the cache. */
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
				}

				mutex_unlock(&mount->lock);
			}
		} else {
			/* Node is not in the cache. We must pull it into the
			 * cache from the filesystem. */
			if(!mount->ops->read_node) {
				mutex_unlock(&mount->lock);
				fs_node_release(prev);
				return -ERR_NOT_SUPPORTED;
			} else if((ret = mount->ops->read_node(mount, id, &node)) != 0) {
				mutex_unlock(&mount->lock);
				fs_node_release(prev);
				return ret;
			}

			assert(node->ops);

			/* Attach the node to the node tree and used list. */
			avl_tree_insert(&mount->nodes, (key_t)id, node, NULL);
			list_append(&mount->used_nodes, &node->mount_link);
			mutex_unlock(&mount->lock);
		}

		/* Lock the new node. We do not release the previous node yet
		 * as it may be needed by the symbolic link code. */
		mutex_lock(&node->lock);
	}
}

/** Look up a node in the filesystem.
 *
 * Looks up a node in the filesystem. If the path is a relative path (one that
 * does not begin with a '/' character), then it will be looked up relative to
 * the current directory in the current process' I/O context. Otherwise, the
 * starting '/' character will be taken off and the path will be looked up
 * relative to the current I/O context's root.
 *
 * @note		This function holds the I/O context lock for reading
 *			across the entire lookup to prevent other threads from
 *			changing the root directory of the process while the
 *			lookup is being performed.
 *
 * @param path		Path string to look up.
 * @param follow	If the last path component refers to a symbolic link,
 *			specified whether to follow the link or return the node
 *			of the link itself.
 * @param type		Required node type (negative will not check the type).
 * @param nodep		Where to store pointer to node found (referenced,
 *			unlocked).
 *
 * @return		0 on success, negative error code on failure.
 */
static int fs_node_lookup(const char *path, bool follow, int type, fs_node_t **nodep) {
	fs_node_t *node = NULL;
	char *dup;
	int ret;

	assert(path);
	assert(nodep);

	if(!path[0]) {
		return -ERR_PARAM_INVAL;
	}

	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Start from the current directory if the path is relative. */
	if(path[0] != '/') {
		assert(curr_proc->ioctx.curr_dir);
		node = curr_proc->ioctx.curr_dir;
		mutex_lock(&node->lock);
		fs_node_get(node);
	}

	/* Duplicate path so that fs_node_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_SLEEP);

	/* Look up the path string. */
	if((ret = fs_node_lookup_internal(dup, node, follow, 0, &node)) == 0) {
		if(type >= 0 && node->type != (fs_node_type_t)type) {
			ret = -ERR_TYPE_INVAL;
			mutex_unlock(&node->lock);
			fs_node_release(node);
		} else {
			*nodep = node;
			mutex_unlock(&node->lock);
		}
	}

	rwlock_unlock(&curr_proc->ioctx.lock);
	kfree(dup);
	return ret;
}

/** Increase the reference count of a node.
 * @note		Should not be used on unused nodes.
 * @param node		Node to increase reference count of. */
void fs_node_get(fs_node_t *node) {
	if(refcount_inc(&node->count) == 1) {
		fatal("Getting unused FS node %" PRIu16 ":%" PRIu64,
		      (node->mount) ? node->mount->id : 0, node->id);
	}
}

/** Decrease the reference count of a node.
 *
 * Decreases the reference count of a filesystem node. If this causes the
 * node's count to become zero, then the node will be moved on to the mount's
 * unused node list. This function should be called when a node obtained via
 * fs_node_lookup() or referenced via fs_node_get() is no longer required;
 * each call to those functions should be matched with a call to this function.
 *
 * @param node		Node to decrease reference count of.
 */
void fs_node_release(fs_node_t *node) {
	fs_mount_t *mount = NULL;
	int ret;

	if(node->mount) {
		mutex_lock(&node->mount->lock);
		mount = node->mount;
	}
	mutex_lock(&node->lock);

	if(refcount_dec(&node->count) == 0) {
		assert(!node->mounted);

		/* Node has no references remaining, move it to its mount's
		 * unused list if it has a mount. If the node is not attached
		 * to anything, then destroy it immediately. */
		if(mount && !(node->flags & FS_NODE_REMOVED) && !list_empty(&node->mount_link)) {
			list_append(&node->mount->unused_nodes, &node->mount_link);
			dprintf("fs: transferred node %p to unused list (mount: %p)\n", node, node->mount);
			mutex_unlock(&node->lock);
			mutex_unlock(&mount->lock);
		} else {
			/* This shouldn't fail - the only thing that can fail
			 * in fs_node_free() is flushing data. Since this node
			 * has no source to flush to, or has been removed, this
			 * should not fail. */
			if((ret = fs_node_free(node)) != 0) {
				fatal("Could not destroy %s (%d)",
				      (mount) ? "removed node" : "node with no mount",
				      ret);
			}
			if(mount) {
				mutex_unlock(&mount->lock);
			}
		}
	} else {
		mutex_unlock(&node->lock);
		if(mount) {
			mutex_unlock(&mount->lock);
		}
	}
}

/** Mark a filesystem node as removed.
 *
 * Marks a filesystem node as removed. This is to be used by filesystem
 * implementations to mark a node as removed when its link count reaches 0,
 * to cause the node to be removed from memory as soon as it is released.
 *
 * @param node		Node to mark as removed.
 */
void fs_node_remove(fs_node_t *node) {
	node->flags |= FS_NODE_REMOVED;
}

/** Common node creation code.
 * @param path		Path to node to create.
 * @param node		Node structure describing the node to be created.
 * @return		0 on success, negative error code on failure. */
static int fs_node_create(const char *path, fs_node_t *node) {
	fs_node_t *parent = NULL;
	char *dir, *name;
	node_id_t id;
	int ret;

	assert(!node->mount);

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = -ERR_ALREADY_EXISTS;
		goto out;
	}

	dprintf("fs: create(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                ret = -ERR_ALREADY_EXISTS;
		goto out;
        }

	/* Look up the parent node. */
	if((ret = fs_node_lookup(dir, true, FS_NODE_DIR, &parent)) != 0) {
		goto out;
	}

	mutex_lock(&parent->mount->lock);
	mutex_lock(&parent->lock);

	/* Ensure that we are on a writable filesystem, and that the FS
	 * supports node creation. */
	if(FS_NODE_IS_RDONLY(parent)) {
		ret = -ERR_READ_ONLY;
		goto out;
	} else if(!parent->ops->create) {
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	}

	/* Check if the name we're creating already exists. This will populate
	 * the entry cache so it will be OK to add the node to it. */
	if((ret = fs_dir_lookup(parent, name, &id)) != -ERR_NOT_FOUND) {
		if(ret == 0) {
			ret = -ERR_ALREADY_EXISTS;
		}
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	node->ops = parent->ops;
	node->mount = parent->mount;
	if((ret = node->ops->create(parent, name, node)) != 0) {
		goto out;
	}

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&node->mount->nodes, (key_t)node->id, node, NULL);
	list_append(&node->mount->used_nodes, &node->mount_link);

	/* Insert the node into the parent's entry cache. */
	fs_dir_insert(parent, name, node->id);

	dprintf("fs: created %s (node: %" PRIu16 ":%" PRIu64 ", parent: %" PRIu16 ":%" PRIu64 ")\n",
	        path, node->mount->id, node->id, parent->mount->id, parent->id);
	ret = 0;
out:
	if(parent) {
		mutex_unlock(&parent->lock);
		mutex_unlock(&parent->mount->lock);
		fs_node_release(parent);
	}
	kfree(dir);
	kfree(name);

	/* Reset mount pointer to NULL in node so that the caller can free it
	 * properly. */
	if(ret != 0) {
		node->mount = NULL;
	}
	return ret;
}

/** Get information about a node.
 * @param node		Node to get information for. Lock must be held.
 * @param info		Structure to store information in. */
static void fs_node_info(fs_node_t *node, fs_info_t *info) {
	assert(mutex_held(&node->lock));

	memset(info, 0, sizeof(fs_info_t));
	info->id = node->id;
	info->mount = (node->mount) ? node->mount->id : 0;
	info->type = node->type;
	info->blksize = PAGE_SIZE;
	if(node->ops->info) {
		node->ops->info(node, info);
	} else {
		info->links = 1;
		if(node->type == FS_NODE_FILE && node->data_cache) {
			info->size = node->data_cache->size;
		} else {
			info->size = 0;
		}
	}
}

/** Create a handle to a node.
 * @param node		Node to create handle to. Will have an extra reference
 *			added to it.
 * @param flags		Flags for the handle.
 * @return		Pointer to handle structure. */
static object_handle_t *fs_handle_create(fs_node_t *node, int flags) {
	object_handle_t *handle;
	fs_handle_t *data;

	/* Allocate the per-handle data structure. */
	data = kmalloc(sizeof(fs_handle_t), MM_SLEEP);
	rwlock_init(&data->lock, "fs_handle_lock");
	data->offset = 0;
	data->flags = flags;

	/* Create the handle. */
	fs_node_get(node);
	handle = object_handle_create(&node->obj, data);
	dprintf("fs: opened handle %p to node %p (data: %p)\n", handle, node, data);
	return handle;
}

/** Read a page of data from a file.
 * @param cache		Cache being read from.
 * @param buf		Buffer to read into.
 * @param offset	Offset to read from.
 * @param nonblock	Whether the operation is required to not block.
 * @return		0 on success, negative error code on failure. */
static int file_cache_read_page(vm_cache_t *cache, void *buf, offset_t offset, bool nonblock) {
	fs_node_t *node = cache->data;

	if(node->ops->read_page) {
		return node->ops->read_page(node, buf, offset, nonblock);
	} else {
		memset(buf, 0, PAGE_SIZE);
		return 0;
	}
}

/** Write a page of data to a file.
 * @param cache		Cache to write to.
 * @param buf		Buffer containing data to write.
 * @param offset	Offset to write from.
 * @param nonblock	Whether the operation is required to not block.
 * @return		0 on success, negative error code on failure. */
static int file_cache_write_page(vm_cache_t *cache, const void *buf, offset_t offset, bool nonblock) {
	fs_node_t *node = cache->data;

	assert(!FS_NODE_IS_RDONLY(node));

	if(node->ops->write_page) {
		return node->ops->write_page(node, buf, offset, nonblock);
	} else {
		/* Unless the cache is being destroyed, return an error, which
		 * will cause the page to remain in the modified queue, so that
		 * the page daemon won't ever try to evict the page. */
		return (cache->deleted) ? 0 : -ERR_NOT_SUPPORTED;
	}
}

/** Determine whether a file page can be evicted.
 * @param cache		Cache that the page belongs to.
 * @param page	 	Page to check.
 * @return		Always returns true. */
static bool file_cache_evict_page(vm_cache_t *cache, vm_page_t *page) {
	fs_node_t *node = cache->data;

	/* If the file's pages should remain in memory, they will always remain
	 * in the modified queue and will never end up in the cached or
	 * pageable queues. */
	assert(node->ops->write_page);

	return true;
}

/** File data cache operations. */
static vm_cache_ops_t file_cache_ops = {
	.read_page = file_cache_read_page,
	.write_page = file_cache_write_page,
	.evict_page = file_cache_evict_page,
};

/** Get the data cache for a file.
 * @param node		Node to get data cache for.
 * @return		Pointer to node's data cache. */
static vm_cache_t *fs_file_get_cache(fs_node_t *node) {
	fs_info_t info;

	assert(mutex_held(&node->lock));
	assert(node->type == FS_NODE_FILE);

	if(!node->data_cache) {
		/* Create a new data cache. We first need to get the size. */
		fs_node_info(node, &info);
		node->data_cache = vm_cache_create(info.size, &file_cache_ops, node);
	}

	return node->data_cache;
}

/** Close a handle to a file.
 * @param handle	Handle to close. */
static void file_object_close(object_handle_t *handle) {
	fs_node_release((fs_node_t *)handle->object);
	kfree(handle->data);
}

/** Check if a file can be memory-mapped.
 * @param handle	Handle to file.
 * @param flags		Mapping flags (VM_MAP_*).
 * @return		0 if can be mapped, negative error code if not. */
static int file_object_mappable(object_handle_t *handle, int flags) {
	fs_handle_t *data = handle->data;

	/* If shared write access is required, ensure that the handle flags
	 * allow this. */
	if(!(flags & VM_MAP_PRIVATE) && flags & VM_MAP_WRITE && !(data->flags & FS_FILE_WRITE)) {
		return -ERR_PERM_DENIED;
	} else {
		return 0;
	}
}

/** Get a page from a file object.
 * @param handle	Handle to file.
 * @param offset	Offset of page to get.
 * @param physp		Where to store physical address of page.
 * @return		0 on success, negative error code on failure. */
static int file_object_get_page(object_handle_t *handle, offset_t offset, phys_ptr_t *physp) {
	fs_node_t *node = (fs_node_t *)handle->object;
	vm_cache_t *cache;

	mutex_lock(&node->lock);
	cache = fs_file_get_cache(node);
	mutex_unlock(&node->lock);
	return vm_cache_get_page(cache, offset, physp);
}

/** Release a page from a file object.
 * @param handle	Handle to file.
 * @param offset	Offset of page to release.
 * @param phys		Physical address of page that was unmapped. */
static void file_object_release_page(object_handle_t *handle, offset_t offset, phys_ptr_t phys) {
	fs_node_t *node = (fs_node_t *)handle->object;
	vm_cache_t *cache;

	mutex_lock(&node->lock);
	cache = fs_file_get_cache(node);
	mutex_unlock(&node->lock);
	vm_cache_release_page(cache, offset, phys);
}

/** File object operations. */
static object_type_t file_object_type = {
	.id = OBJECT_TYPE_FILE,
	.close = file_object_close,
	.mappable = file_object_mappable,
	.get_page = file_object_get_page,
	.release_page = file_object_release_page,
};

/** Create a regular file in the file system.
 * @param path		Path to file to create.
 * @return		0 on success, negative error code on failure. */
int fs_file_create(const char *path) {
	fs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = fs_node_alloc(NULL, 0, FS_NODE_FILE, NULL, NULL);

	/* Call the common creation code. */
	ret = fs_node_create(path, node);
	fs_node_release(node);
	return ret;
}

/** Free an in-memory node.
 * @param node		Node to free. */
static void memory_node_free(fs_node_t *node) {
	kfree(node->data);
}

/** Modify the size of a file.
 * @param node		Node being resized.
 * @param size		New size of the node.
 * @return		0 on success, negative error code on failure. */
static int memory_node_resize(fs_node_t *node, offset_t size) {
	*(offset_t *)node->data = size;
	return 0;
}

/** Get information about an in-memory node.
 * @param node		Node to get information on.
 * @param info		Information structure to fill in. */
static void memory_node_info(fs_node_t *node, fs_info_t *info) {
	info->size = *(offset_t *)node->data;
}

/** Operations for an in-memory node. */
static fs_node_ops_t memory_node_ops = {
	.free = memory_node_free,
	.resize = memory_node_resize,
	.info = memory_node_info,
};

/** Create a special file backed by a chunk of memory.
 *
 * Creates a special file that is backed by the specified chunk of memory.
 * This is useful to pass data stored in memory to code that expects to be
 * operating on filesystem entries, such as the module loader.
 *
 * When the file is created, the data in the given memory area is duplicated
 * into its data cache, so updates to the memory area after this function has
 * been called will not show on reads from the file. Similarly, writes to the
 * file will not be written back to the memory area.
 *
 * The file is not attached anywhere in the filesystem, and therefore when the
 * handle is closed, it will be immediately destroyed.
 *
 * @param buf		Pointer to memory area to use.
 * @param size		Size of memory area.
 * @param flags		Flags for the handle.
 * @param handlep	Where to store handle to file.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_from_memory(const void *buf, size_t size, int flags, object_handle_t **handlep) {
	object_handle_t *handle;
	fs_node_t *node;
	offset_t *data;
	int ret;

	if(!buf || !size || !flags || !handlep) {
		return -ERR_PARAM_INVAL;
	}

	/* Create a node to store the data. */
	data = kmalloc(sizeof(offset_t), MM_SLEEP);
	*data = (offset_t)size;
	node = fs_node_alloc(NULL, 0, FS_NODE_FILE, &memory_node_ops, data);

	/* Create a temporary handle to the file with write permission and
	 * write the data to the file. */
	handle = fs_handle_create(node, FS_FILE_WRITE);
	if((ret = fs_file_write(handle, buf, size, NULL)) == 0) {
		*handlep = fs_handle_create(node, flags);
	}

	object_handle_release(handle);
	fs_node_release(node);
	return ret;
}

/** Open a handle to a file.
 * @param path		Path to file to open.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		0 on success, negative error code on failure. */
int fs_file_open(const char *path, int flags, object_handle_t **handlep) {
	fs_node_t *node;
	int ret;

	/* Look up the filesystem node and check if it is suitable. */
	if((ret = fs_node_lookup(path, true, FS_NODE_FILE, &node)) != 0) {
		return ret;
	} else if(flags & FS_FILE_WRITE && FS_NODE_IS_RDONLY(node)) {
		fs_node_release(node);
		return -ERR_READ_ONLY;
	}

	*handlep = fs_handle_create(node, flags);
	fs_node_release(node);
	return 0;
}

/** Read from a file.
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to read from (if usehnd is true).
 * @param usehnd	Whether to use the handle's offset.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 * @return		0 on success, negative error code on failure.
 */
static int fs_file_read_internal(object_handle_t *handle, void *buf, size_t count,
                                 offset_t offset, bool usehnd, size_t *bytesp) {
	fs_handle_t *data;
	vm_cache_t *cache;
	size_t total = 0;
	fs_node_t *node;
	int ret = 0;

	if(!handle || !buf) {
		ret = -ERR_PARAM_INVAL;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_FILE);

	if(!(data->flags & FS_FILE_READ)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Pull the offset out of the handle structure if required. */
	if(usehnd) {
		rwlock_read_lock(&data->lock);
		offset = data->offset;
		rwlock_unlock(&data->lock);
	}

	/* Get the node's data cache. */
	mutex_lock(&node->lock);
	cache = fs_file_get_cache(node);
	mutex_unlock(&node->lock);

	/* Read the data from the cache. */
	ret = vm_cache_read(cache, buf, count, offset, data->flags & FS_NONBLOCK, &total);
out:
	if(total) {
		dprintf("fs: read %zu bytes from offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
		        total, offset, node, (node->mount) ? node->mount->id : 0, node->id);
		if(usehnd) {
			rwlock_write_lock(&data->lock);
			data->offset += total;
			rwlock_unlock(&data->lock);
		}
	}

	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur from the file
 * handle's current offset, and before returning the offset will be incremented
 * by the number of bytes read.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_read(object_handle_t *handle, void *buf, size_t count, size_t *bytesp) {
	return fs_file_read_internal(handle, buf, count, 0, true, bytesp);
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_pread(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	return fs_file_read_internal(handle, buf, count, offset, false, bytesp);
}

/** Write to a file.
 * @param handle	Handle to file to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to write to (if usehnd is true).
 * @param usehnd	Whether to use the handle's offset.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 * @return		0 on success, negative error code on failure.
 */
static int fs_file_write_internal(object_handle_t *handle, const void *buf, size_t count,
                                  offset_t offset, bool usehnd, size_t *bytesp) {
	fs_handle_t *data;
	vm_cache_t *cache;
	size_t total = 0;
	fs_node_t *node;
	int ret = 0;

	if(!handle || !buf) {
		ret = -ERR_PARAM_INVAL;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_FILE);

	if(!(data->flags & FS_FILE_WRITE)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Locking order - handle lock before node lock. */
	rwlock_write_lock(&data->lock);
	mutex_lock(&node->lock);

	/* Get the node's data cache. */
	cache = fs_file_get_cache(node);

	/* Pull the offset out of the handle structure, and handle the
	 * FS_FILE_APPEND flag. */
	if(data->flags & FS_FILE_APPEND) {
		data->offset = cache->size;
	}
	offset = data->offset;
	rwlock_unlock(&data->lock);

	/* Attempt to resize the node if necessary. */
	if((offset + count) >= cache->size) {
		/* If the resize operation is not provided, we can only write
		 * within the space that we have. */
		if(!node->ops->resize) {
			if(offset >= cache->size) {
				ret = 0;
				mutex_unlock(&node->lock);
				goto out;
			} else {
				count = (size_t)(cache->size - offset);
			}
		} else {
			if((ret = node->ops->resize(node, offset + count)) != 0) {
				mutex_unlock(&node->lock);
				goto out;
			}

			vm_cache_resize(cache, offset + count);
		}
	}

	/* Ensure that space is allocated in the filesystem. */
	if(node->ops->allocate) {
		/* FIXME: On failure, should this restore the original file
		 * size? */
		if((ret = node->ops->allocate(node, offset, count)) != 0) {
			mutex_unlock(&node->lock);
			goto out;
		}
	}

	mutex_unlock(&node->lock);

	/* Write the data to the cache. */
	ret = vm_cache_write(cache, buf, count, offset, data->flags & FS_NONBLOCK, &total);
out:
	if(total) {
		dprintf("fs: wrote %zu bytes to offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
		        total, offset, node, (node->mount) ? node->mount->id : 0, node->id);
		if(usehnd) {
			rwlock_write_lock(&data->lock);
			data->offset += total;
			rwlock_unlock(&data->lock);
		}
	}

	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the file
 * handle's current offset (if the FS_FILE_APPEND flag is set, the offset will
 * be set to the end of the file and the write will take place there), and
 * before returning the handle's offset will be incremented by the number of
 * bytes written.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FS_FILE_APPEND is
 *			set).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_write(object_handle_t *handle, const void *buf, size_t count, size_t *bytesp) {
	return fs_file_write_internal(handle, buf, count, 0, true, bytesp);
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments.
 * @param offset	Offset into file to write at.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_pwrite(object_handle_t *handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	return fs_file_write_internal(handle, buf, count, offset, false, bytesp);
}

/** Modify the size of a file.
 *
 * Modifies the size of a file in the file system. If the new size is smaller
 * than the previous size of the file, then the extra data is discarded. If
 * it is larger than the previous size, then the extended space will be filled
 * with zero bytes.
 *
 * @param handle	Handle to file to resize.
 * @param size		New size of the file.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_file_resize(object_handle_t *handle, offset_t size) {
	fs_handle_t *data;
	fs_node_t *node;
	int ret;

	if(!handle) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return -ERR_TYPE_INVAL;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	mutex_lock(&node->lock);
	assert(node->type == FS_NODE_FILE);

	/* Check if resizing is allowed. */
	if(!(data->flags & FS_FILE_WRITE)) {
		mutex_unlock(&node->lock);
		return -ERR_PERM_DENIED;
	} else if(!node->ops->resize) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_SUPPORTED;
	}

	if((ret = node->ops->resize(node, size)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	}

	/* If the node has a data cache, resize it. */
	if(node->data_cache) {
		vm_cache_resize(node->data_cache, size);
	}
	mutex_unlock(&node->lock);
	return 0;
}

/** Close a handle to a directory.
 * @param handle	Handle to close. */
static void dir_object_close(object_handle_t *handle) {
	fs_node_release((fs_node_t *)handle->object);
	kfree(handle->data);
}

/** Directory object operations. */
static object_type_t dir_object_type = {
	.id = OBJECT_TYPE_DIR,
	.close = dir_object_close,
};

/** Ensure a directory's entry cache is allocated.
 * @param node		Node to allocate for. */
static void fs_dir_alloc_cache(fs_node_t *node) {
	if(!node->entry_cache) {
		node->entry_cache = kmalloc(sizeof(fs_dir_cache_t), MM_SLEEP);
		radix_tree_init(&node->entry_cache->entries);
		node->entry_cache->count = 0;
	}
}

/** Get a directory's entry cache.
 * @param node		Node of directory.
 * @param cachep	Where to store pointer to cache.
 * @return		0 on success, negative error code on failure. */
static int fs_dir_get_cache(fs_node_t *node, fs_dir_cache_t **cachep) {
	int ret;

	assert(node->type == FS_NODE_DIR);
	assert(node->ops);

	/* Create a new cache if there isn't one yet. */
	fs_dir_alloc_cache(node);
	if(!node->entry_cache->count) {
		if(likely(node->ops->cache_children)) {
			if((ret = node->ops->cache_children(node)) != 0) {
				radix_tree_clear(&node->entry_cache->entries, kfree);
				kfree(node->entry_cache);
				node->entry_cache = NULL;
				return ret;
			}
		} else {
			kprintf(LOG_WARN, "fs: entry cache empty, but operations lack cache_children (type: %s)\n",
			        (node->mount) ? node->mount->type->name : "none");
			return -ERR_NOT_FOUND;
		}
	}

	*cachep = node->entry_cache;
	return 0;
}

/** Look up a directory entry.
 * @param node		Node of directory (should be locked).
 * @param name		Name of entry to get.
 * @param idp		Where to store ID of node.
 * @return		0 on success, negative error code on failure. */
static int fs_dir_lookup(fs_node_t *node, const char *name, node_id_t *idp) {
	fs_dir_cache_t *cache;
	fs_dir_entry_t *entry;
	int ret;

	/* Get the populated entry cache. */
	if((ret = fs_dir_get_cache(node, &cache)) != 0) {
		return ret;
	}

	/* Look up the entry. */
	if((entry = radix_tree_lookup(&cache->entries, name))) {
		*idp = entry->id;
		return 0;
	} else {
		return -ERR_NOT_FOUND;
	}
}

/** Add an entry to a directory's entry cache.
 *
 * Adds an entry to a directory node's entry cache. This function should only
 * be used by filesystem type operations.
 *
 * @param node		Node to add entry to.
 * @param name		Name of entry.
 * @param id		ID of node entry points to.
 */
void fs_dir_insert(fs_node_t *node, const char *name, node_id_t id) {
	fs_dir_entry_t *entry;
	size_t len;

	assert(node->type == FS_NODE_DIR);

	/* Ensure the cache is created. */
	fs_dir_alloc_cache(node);

	/* Work out the length we need. */
	len = sizeof(fs_dir_entry_t) + strlen(name) + 1;

	/* Allocate the buffer for it and fill it in. */
	entry = kmalloc(len, MM_SLEEP);
	entry->length = len;
	entry->id = id;
	strcpy(entry->name, name);

	/* Insert into the cache. */
	radix_tree_insert(&node->entry_cache->entries, name, entry);
	node->entry_cache->count++;
}

/** Remove an entry from a directory's entry cache.
 * @param node		Node to remove entry from.
 * @param name		Name of entry to remove. */
static void fs_dir_remove(fs_node_t *node, const char *name) {
	if(node->entry_cache) {
		radix_tree_remove(&node->entry_cache->entries, name, kfree);
		node->entry_cache->count--;
	}
}

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @return		0 on success, negative error code on failure. */
int fs_dir_create(const char *path) {
	fs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = fs_node_alloc(NULL, 0, FS_NODE_DIR, NULL, NULL);

	/* Call the common creation code. */
	ret = fs_node_create(path, node);
	fs_node_release(node);
	return ret;
}

/** Open a handle to a directory.
 * @param path		Path to directory to open.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		0 on success, negative error code on failure. */
int fs_dir_open(const char *path, int flags, object_handle_t **handlep) {
	fs_node_t *node;
	int ret;

	/* Look up the filesystem node. */
	if((ret = fs_node_lookup(path, true, FS_NODE_DIR, &node)) != 0) {
		return ret;
	}

	*handlep = fs_handle_create(node, flags);
	fs_node_release(node);
	return 0;
}

/** Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed. The number of the entry read
 * will be the handle's current offset, and upon success the handle's offset
 * will be incremented by 1.
 *
 * @param handle	Handle to directory to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_BUF_TOO_SMALL
 *			will be returned).
 *
 * @return		0 on success, negative error code on failure. If the
 *			handle's offset is past the end of the directory,
 *			-ERR_NOT_FOUND will be returned.
 */
int fs_dir_read(object_handle_t *handle, fs_dir_entry_t *buf, size_t size) {
	fs_dir_entry_t *entry = NULL;
	fs_node_t *child, *node;
	fs_dir_cache_t *cache;
	offset_t i = 0, index;
	fs_handle_t *data;
	int ret;

	if(!handle || !buf) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_DIR);

	/* Pull the offset out of the handle structure. */
	rwlock_read_lock(&data->lock);
	index = data->offset;
	rwlock_unlock(&data->lock);

	mutex_lock(&node->lock);

	/* Obtain the entry cache for the directory, and ensure the index is
	 * within the directory. */
	if((ret = fs_dir_get_cache(node, &cache)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	} else if(index >= (offset_t)cache->count) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_FOUND;
	}

	/* Iterate through the tree to find the entry. */
	RADIX_TREE_FOREACH(&cache->entries, iter) {
		if(i++ == index) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);
			break;
		}
	}

	/* We should have it because we checked against the entry count. */
	if(!entry) {
		fatal("Entry %" PRIu64 " within size but not found (%p)", index, node);
	}

	/* Check that the buffer is large enough. */
	if(size < entry->length) {
		mutex_unlock(&node->lock);
		return -ERR_BUF_TOO_SMALL;
	}

	/* Copy it to the buffer. */
	memcpy(buf, entry, entry->length);

	mutex_unlock(&node->lock);
	mutex_lock(&node->mount->lock);
	mutex_lock(&node->lock);

	/* Fix up the entry. */
	if(node == node->mount->root && strcmp(entry->name, "..") == 0) {
		/* This is the '..' entry, and the node is the root of its
		 * mount. Change the node ID to be the ID of the mountpoint,
		 * if any. */
		if(node->mount->mountpoint) {
			mutex_lock(&node->mount->mountpoint->lock);
			if((ret = fs_dir_lookup(node->mount->mountpoint, "..", &buf->id)) != 0) {
				mutex_unlock(&node->mount->mountpoint->lock);
				mutex_unlock(&node->mount->lock);
				mutex_unlock(&node->lock);
				return ret;
			}
			mutex_unlock(&node->mount->mountpoint->lock);
		}
	} else {
		/* Check if the entry refers to a mountpoint. In this case we
		 * need to change the node ID to be the node ID of the mount
		 * root, rather than the mountpoint. If the node the entry
		 * currently points to is not in the cache, then it won't be a
		 * mountpoint (mountpoints are always in the cache). */
		if((child = avl_tree_lookup(&node->mount->nodes, (key_t)buf->id))) {
			if(child != node) {
				mutex_lock(&child->lock);
				if(child->type == FS_NODE_DIR && child->mounted) {
					buf->id = child->mounted->root->id;
				}
				mutex_unlock(&child->lock);
			}
		}
	}

	mutex_unlock(&node->mount->lock);
	mutex_unlock(&node->lock);

	/* Update offset in the handle. */
	rwlock_read_lock(&data->lock);
	data->offset++;
	rwlock_unlock(&data->lock);
	return 0;
}

/** Set the offset of a file/directory handle.
 *
 * Modifies the offset of a file or directory handle according to the specified
 * action, and returns the new offset. For directories, the offset is the
 * index of the next directory entry that will be read.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FS_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_handle_seek(object_handle_t *handle, int action, rel_offset_t offset, offset_t *newp) {
	fs_dir_cache_t *dcache;
	fs_handle_t *data;
	vm_cache_t *cache;
	fs_node_t *node;
	int ret = 0;

	if(!handle || (action != FS_SEEK_SET && action != FS_SEEK_ADD && action != FS_SEEK_END)) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	rwlock_write_lock(&data->lock);

	/* Perform the action. */
	switch(action) {
	case FS_SEEK_SET:
		data->offset = (offset_t)offset;
		break;
	case FS_SEEK_ADD:
		data->offset += offset;
		break;
	case FS_SEEK_END:
		mutex_lock(&node->lock);

		if(node->type == FS_NODE_DIR) {
			/* We must cache the entries to know the entry count. */
			if((ret = fs_dir_get_cache(node, &dcache)) != 0) {
				mutex_unlock(&node->lock);
				rwlock_unlock(&data->lock);
				return ret;
			}
			data->offset = (offset_t)dcache->count + offset;
		} else {
			/* Get the data cache so we can get the size. */
			cache = fs_file_get_cache(node);
			data->offset = cache->size + offset;
		}

		mutex_unlock(&node->lock);
		break;
	}

	/* Save the new offset if necessary. */
	if(newp) {
		*newp = data->offset;
	}
	rwlock_unlock(&data->lock);
	return 0;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information on.
 * @param info		Information structure to fill in.
 * @return		0 on success, negative error code on failure. */
int fs_handle_info(object_handle_t *handle, fs_info_t *info) {
	fs_node_t *node;

	if(!handle || !info) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (fs_node_t *)handle->object;
	mutex_lock(&node->lock);
	fs_node_info(node, info);
	mutex_unlock(&node->lock);
	return 0;
}

/** Flush changes to a filesystem node to the FS.
 * @param handle	Handle to node to flush.
 * @return		0 on success, negative error code on failure. */
int fs_handle_sync(object_handle_t *handle) {
	fs_node_t *node;
	int ret;

	if(!handle) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (fs_node_t *)handle->object;
	mutex_lock(&node->mount->lock);
	mutex_lock(&node->lock);

	ret = fs_node_flush(node);

	mutex_unlock(&node->lock);
	mutex_unlock(&node->mount->lock);
	return ret;
}

/** Ensure that a symbolic link's destination is cached.
 * @param node		Node of link (should be locked).
 * @return		0 on success, negative error code on failure. */
static int fs_symlink_cache(fs_node_t *node) {
	int ret;

	assert(node->type == FS_NODE_SYMLINK);

	if(!node->link_cache) {
		if(!node->ops->cache_dest) {
			kprintf(LOG_WARN, "fs: cache_dest should be implemented on FS with symlinks (type: %s)\n",
			        (node->mount) ? node->mount->type->name : "none");
			return -ERR_NOT_SUPPORTED;
		} else if((ret = node->ops->cache_dest(node)) != 0) {
			return ret;
		}

		if(!node->link_cache) {
			kprintf(LOG_WARN, "fs: cache_dest did not set link_cache (type: %s)\n",
			        (node->mount) ? node->mount->type->name : "none");
			return -ERR_NOT_SUPPORTED;
		}
	}

	return 0;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		0 on success, negative error code on failure. */
int fs_symlink_create(const char *path, const char *target) {
	fs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = fs_node_alloc(NULL, 0, FS_NODE_SYMLINK, NULL, NULL);
	node->link_cache = kstrdup(target, MM_SLEEP);

	/* Call the common creation code. */
	ret = fs_node_create(path, node);
	fs_node_release(node);
	return ret;
}

/** Get the destination of a symbolic link.
 *
 * Reads the destination of a symbolic link into a buffer. A NULL byte will
 * be placed at the end of the buffer, unless the buffer is too small.
 *
 * @param path		Path to the symbolic link to read.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer (destination will be truncated if buffer
 *			is too small).
 *
 * @return		Number of bytes read on success, negative error code
 *			on failure.
 */
int fs_symlink_read(const char *path, char *buf, size_t size) {
	fs_node_t *node;
	size_t len;
	int ret;

	if(!path || !buf || !size) {
		return -ERR_PARAM_INVAL;
	} else if((ret = fs_node_lookup(path, false, FS_NODE_SYMLINK, &node)) != 0) {
		return ret;
	}

	mutex_lock(&node->lock);

	/* Ensure destination is cached. */
	if((ret = fs_symlink_cache(node)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	}

	len = ((len = strlen(node->link_cache) + 1) > size) ? size : len;
	memcpy(buf, node->link_cache, len);
	mutex_unlock(&node->lock);
	fs_node_release(node);
	return (int)len;
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
 * @param countp	Where to store number of arguments in array.
 * @param flagsp	Where to store mount flags. */
static void parse_mount_options(const char *str, fs_mount_option_t **optsp, size_t *countp, int *flagsp) {
	fs_mount_option_t *opts = NULL;
	char *dup, *name, *value;
	size_t count = 0;
	int flags = 0;

	if(str) {
		/* Duplicate the string to allow modification with strsep(). */
		dup = kstrdup(str, MM_SLEEP);

		while((value = strsep(&dup, ","))) {
			name = strsep(&value, "=");

			/* Handle arguments recognised by us. */
			if(strcmp(name, "ro") == 0) {
				flags |= FS_MOUNT_RDONLY;
			} else {
				opts = krealloc(opts, sizeof(fs_mount_option_t) * (count + 1), MM_SLEEP);
				opts[count].name = kstrdup(name, MM_SLEEP);
				opts[count].value = (value) ? kstrdup(value, MM_SLEEP) : NULL;
				count++;
			}
		}

		kfree(dup);
	}

	*optsp = opts;
	*countp = count;
	*flagsp = flags;
}

/** Free a mount options array.
 * @param opts		Array of options.
 * @param count		Number of options. */
static void free_mount_options(fs_mount_option_t *opts, size_t count) {
	size_t i;

	if(count) {
		for(i = 0; i < count; i++) {
			kfree((char *)opts[i].name);
			if(opts[i].value) {
				kfree((char *)opts[i].value);
			}
		}
		kfree(opts);
	}
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * The opts parameter allows a string containing a list of comma-seperated
 * mount options to be passed. Some options are recognised by this file system:
 *  - ro - Mount the filesystem read-only.
 * All other options are passed through to the filesystem implementation.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 *
 * @param dev		Device tree path to device filesystem resides on (can
 *			be NULL if the filesystem does not require a device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type (if not specified, device will
 *			be probed to determine the correct type - in this case,
 *			a device must be specified).
 * @param opts		Options string.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_mount(const char *dev, const char *path, const char *type, const char *opts) {
	fs_mount_option_t *optarr;
	fs_mount_t *mount = NULL;
	fs_node_t *node = NULL;
	device_t *device;
	int ret, flags;
	size_t count;

	if(!path || (!dev && !type)) {
		return -ERR_PARAM_INVAL;
	}

	/* Parse the options string. */
	parse_mount_options(opts, &optarr, &count, &flags);

	/* Lock the mount lock across the entire operation, so that only one
	 * mount can take place at a time. */
	mutex_lock(&mounts_lock);

	/* If the root filesystem is not yet mounted, the only place we can
	 * mount is '/'. */
	if(!root_mount) {
		assert(curr_proc == kernel_proc);
		if(strcmp(path, "/") != 0) {
			ret = -ERR_NOT_FOUND;
			goto fail;
		}
	} else {
		/* Look up the destination directory. */
		if((ret = fs_node_lookup(path, true, FS_NODE_DIR, &node)) != 0) {
			goto fail;
		}

		mutex_lock(&node->lock);

		/* Check that it is not being used as a mount point already. */
		if(node->mount->root == node) {
			ret = -ERR_IN_USE;
			goto fail;
		}
	}

	/* Initialise the mount structure. */
	mount = kmalloc(sizeof(fs_mount_t), MM_SLEEP);
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

	/* Look up the device, if any. */
	if(dev) {
		if((ret = device_lookup(dev, &device)) != 0) {
			goto fail;
		}

		ret = device_open(device, &mount->device);
		device_release(device);
		if(ret != 0) {
			goto fail;
		}
	}

	/* Look up the filesystem type. If there is not a type specified, probe
	 * for one. */
	if(type) {
		if(!(mount->type = fs_type_lookup(type))) {
			ret = -ERR_NOT_FOUND;
			goto fail;
		}

		/* Release the device if it is not needed, and check if the
		 * device contains the FS type. */
		if(!mount->type->probe) {
			if(mount->device) {
				object_handle_release(mount->device);
				mount->device = NULL;
			}
		} else if(!mount->device) {
			ret = -ERR_PARAM_INVAL;
			goto fail;
		} else if(!mount->type->probe(mount->device, NULL)) {
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}
	} else {
		if(!(mount->type = fs_type_probe(mount->device, NULL))) {
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}
	}

	/* Allocate a mount ID. */
	if(next_mount_id == UINT16_MAX) {
		ret = -ERR_RESOURCE_UNAVAIL;
		goto fail;
	}
	mount->id = next_mount_id++;

	/* Call the filesystem's mount operation. */
	if((ret = mount->type->mount(mount, optarr, count)) != 0) {
		goto fail;
	} else if(!mount->ops || !mount->root) {
		fatal("Mount (%s) did not set ops/root", mount->type->name);
	}

	/* Put the root node into the node tree/used list. */
	avl_tree_insert(&mount->nodes, (key_t)mount->root->id, mount->root, NULL);
	list_append(&mount->used_nodes, &mount->root->mount_link);

	/* Make the mountpoint point to the new mount. */
	if(mount->mountpoint) {
		mount->mountpoint->mounted = mount;
		mutex_unlock(&mount->mountpoint->lock);
	}

	/* Store mount in mounts list and unlock the mount lock. */
	list_append(&mount_list, &mount->header);
	if(!root_mount) {
		root_mount = mount;

		/* Give the kernel process a correct current/root directory. */
		fs_node_get(root_mount->root);
		curr_proc->ioctx.root_dir = root_mount->root;
		fs_node_get(root_mount->root);
		curr_proc->ioctx.curr_dir = root_mount->root;
	}

	dprintf("fs: mounted %s:%s on %s (mount: %p, root: %p)\n", mount->type->name,
	        (dev) ? dev : "<none>", path, mount, mount->root);
	mutex_unlock(&mounts_lock);
	free_mount_options(optarr, count);
	return 0;
fail:
	if(mount) {
		if(mount->device) {
			object_handle_release(mount->device);
		}
		if(mount->type) {
			refcount_dec(&mount->type->count);
		}
		kfree(mount);
	}
	if(node) {
		mutex_unlock(&node->lock);
		fs_node_release(node);
	}
	mutex_unlock(&mounts_lock);
	free_mount_options(optarr, count);
	return ret;
}

/** Unmounts a filesystem.
 *
 * Flushes all modifications to a filesystem if it is not read-only and
 * unmounts it. If any nodes in the filesystem are busy, then the operation
 * will fail.
 *
 * @param path		Path to mount point of filesystem.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_unmount(const char *path) {
	fs_node_t *node = NULL, *child;
	fs_mount_t *mount = NULL;
	int ret;

	if(!path) {
		return -ERR_PARAM_INVAL;
	}

	/* Serialise mount/unmount operations. */
	mutex_lock(&mounts_lock);

	/* Look up the destination directory. */
	if((ret = fs_node_lookup(path, true, FS_NODE_DIR, &node)) != 0) {
		goto fail;
	} else if(!node->mount->mountpoint) {
		ret = -ERR_IN_USE;
		goto fail;
	} else if(node != node->mount->root) {
		ret = -ERR_PARAM_INVAL;
		goto fail;
	}

	/* Lock parent mount to ensure that the mount does not get looked up
	 * while we are unmounting. */
	mount = node->mount;
	mutex_lock(&mount->mountpoint->mount->lock);
	mutex_lock(&mount->lock);
	mutex_lock(&node->lock);

	/* Get rid of the reference the lookup added, and check if any nodes
	 * on the mount are in use. */
	if(refcount_dec(&node->count) != 1) {
		ret = -ERR_IN_USE;
		goto fail;
	} else if(node->mount_link.next != &mount->used_nodes || node->mount_link.prev != &mount->used_nodes) {
		ret = -ERR_IN_USE;
		goto fail;
	}

	/* Flush all child nodes. */
	LIST_FOREACH_SAFE(&mount->unused_nodes, iter) {
		child = list_entry(iter, fs_node_t, mount_link);

		/* On success, the child is unlocked by fs_node_free(). */
		mutex_lock(&child->lock);
		if((ret = fs_node_free(child)) != 0) {
			mutex_unlock(&child->lock);
			goto fail;
		}
	}

	/* Free the root node itself. */
	refcount_dec(&node->count);
	if((ret = fs_node_free(node)) != 0) {
		refcount_inc(&node->count);
		goto fail;
	}

	/* Detach from the mountpoint. */
	mount->mountpoint->mounted = NULL;
	mutex_unlock(&mount->mountpoint->mount->lock);
	fs_node_release(mount->mountpoint);

	/* Call unmount operation and release device/type. */
	if(mount->ops->unmount) {
		mount->ops->unmount(mount);
	}
	if(mount->device) {
		object_handle_release(mount->device);
	}
	refcount_dec(&mount->type->count);

	list_remove(&mount->header);
	mutex_unlock(&mounts_lock);
	mutex_unlock(&mount->lock);
	kfree(mount);
	return 0;
fail:
	if(node) {
		if(mount) {
			mutex_unlock(&node->lock);
			mutex_unlock(&mount->lock);
			mutex_unlock(&mount->mountpoint->mount->lock);
		} else {
			fs_node_release(node);
		}
	}
	mutex_unlock(&mounts_lock);
	return ret;
}

/** Get information about a filesystem entry.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		0 on success, negative error code on failure. */
int fs_info(const char *path, bool follow, fs_info_t *info) {
	fs_node_t *node;
	int ret;

	if(!path || !info) {
		return -ERR_PARAM_INVAL;
	} else if((ret = fs_node_lookup(path, follow, -1, &node)) != 0) {
		return ret;
	}

	mutex_lock(&node->lock);
	fs_node_info(node, info);
	mutex_unlock(&node->lock);
	fs_node_release(node);
	return 0;
}

/** Decrease the link count of a filesystem node.
 *
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path		Path to node to decrease link count of.
 *
 * @return		0 on success, negative error code on failure.
 */
int fs_unlink(const char *path) {
	fs_node_t *parent = NULL, *node = NULL;
	fs_dir_cache_t *cache;
	fs_dir_entry_t *entry;
	char *dir, *name;
	int ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	dprintf("fs: unlink(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	if((ret = fs_node_lookup(dir, true, FS_NODE_DIR, &parent)) != 0) {
		goto out;
	} else if((ret = fs_node_lookup(path, false, -1, &node)) != 0) {
		goto out;
	}

	mutex_lock(&parent->lock);
	mutex_lock(&node->lock);

	if(parent->mount != node->mount) {
		ret = -ERR_IN_USE;
		goto out;
	} else if(FS_NODE_IS_RDONLY(node)) {
		ret = -ERR_READ_ONLY;
		goto out;
	} else if(!node->ops->unlink) {
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	}

	/* If it is a directory, ensure that it is empty. */
	if(node->type == FS_NODE_DIR) {
		if((ret = fs_dir_get_cache(node, &cache)) != 0) {
			goto out;
		}

		RADIX_TREE_FOREACH(&cache->entries, iter) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);

			if(strcmp(entry->name, "..") && strcmp(entry->name, ".")) {
				ret = -ERR_IN_USE;
				goto out;
			}
		}
	}

	/* Call the filesystem's unlink operation. */
	if((ret = node->ops->unlink(parent, name, node)) == 0) {
		/* Update the directory entry cache. */
		fs_dir_remove(parent, name);
	}
out:
	if(node) {
		mutex_unlock(&node->lock);
		mutex_unlock(&parent->lock);
		fs_node_release(node);
		fs_node_release(parent);
	} else if(parent) {
		fs_node_release(parent);
	}
	kfree(dir);
	kfree(name);
	return ret;
}

/** Print a list of mounts.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Always returns KDBG_OK. */
int kdbg_cmd_mount(int argc, char **argv) {
	fs_mount_t *mount;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out a list of all mounted filesystems.");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
	        "ID", "Flags", "Type", "Ops", "Data", "Root", "Mountpoint");
	kprintf(LOG_NONE, "%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
	        "==", "=====", "====", "===", "====", "====", "==========");

	LIST_FOREACH(&mount_list, iter) {
		mount = list_entry(iter, fs_mount_t, header);
		kprintf(LOG_NONE, "%-5" PRIu16 " %-5d %-10s %-18p %-18p %-18p %-18p\n",
		        mount->id, mount->flags, (mount->type) ? mount->type->name : "invalid",
		        mount->ops, mount->data, mount->root, mount->mountpoint);
	}

	return KDBG_OK;
}

/** Print information about a node.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_node(int argc, char **argv) {
	fs_node_t *node = NULL;
	fs_dir_entry_t *entry;
	list_t *list = NULL;
	fs_mount_t *mount;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [--unused|--used] <mount ID>\n", argv[0]);
		kprintf(LOG_NONE, "       %s <mount ID> <node ID>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints either a list of nodes on a mount, or details of a\n");
		kprintf(LOG_NONE, "single filesystem node that's currently in memory.\n");
		return KDBG_OK;
	} else if(argc != 2 && argc != 3) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Parse the arguments. */
	if(argc == 3) {
		if(argv[1][0] == '-' && argv[1][1] == '-') {
			if(kdbg_parse_expression(argv[2], &val, NULL) != KDBG_OK) {
				return KDBG_FAIL;
			} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
				kprintf(LOG_NONE, "Unknown mount ID %" PRIun ".\n", val);
				return KDBG_FAIL;
			}
		} else {
			if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
				return KDBG_FAIL;
			} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
				kprintf(LOG_NONE, "Unknown mount ID %" PRIun ".\n", val);
				return KDBG_FAIL;
			} else if(kdbg_parse_expression(argv[2], &val, NULL) != KDBG_OK) {
				return KDBG_FAIL;
			} else if(!(node = avl_tree_lookup(&mount->nodes, (key_t)val))) {
				kprintf(LOG_NONE, "Unknown node ID %" PRIun ".\n", val);
				return KDBG_FAIL;
			}
		}
	} else {
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
			kprintf(LOG_NONE, "Unknown mount ID %" PRIun ".\n", val);
			return KDBG_FAIL;
		}
	}

	if(node) {
		/* Print out basic node information. */
		kprintf(LOG_NONE, "Node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
		        (node->mount) ? node->mount->id : 0, node->id);
		kprintf(LOG_NONE, "=================================================\n");

		kprintf(LOG_NONE, "Count:        %d\n", refcount_get(&node->count));
		kprintf(LOG_NONE, "Locked:       %d (%" PRId32 ")\n", atomic_get(&node->lock.locked),
		        (node->lock.holder) ? node->lock.holder->id : -1);
		if(node->mount) {
			kprintf(LOG_NONE, "Mount:        %p (Locked: %d (%" PRId32 "))\n", node->mount,
			        atomic_get(&node->mount->lock.locked),
			        (node->mount->lock.holder) ? node->mount->lock.holder->id : -1);
		} else {
			kprintf(LOG_NONE, "Mount:        %p\n", node->mount);
		}
		kprintf(LOG_NONE, "Ops:          %p\n", node->ops);
		kprintf(LOG_NONE, "Data:         %p\n", node->data);
		kprintf(LOG_NONE, "Flags:        %d\n", node->flags);
		kprintf(LOG_NONE, "Type:         %d\n", node->type);
		if(node->mounted) {
			kprintf(LOG_NONE, "Mounted:      %p(%" PRIu16 ")\n", node->mounted,
			        node->mounted->id);
		}
		if(node->type == FS_NODE_DIR) {
			if(node->entry_cache) {
				kprintf(LOG_NONE, "\nCached directory entries:\n");
				RADIX_TREE_FOREACH(&node->entry_cache->entries, iter) {
					entry = radix_tree_entry(iter, fs_dir_entry_t);
					kprintf(LOG_NONE, "  Entry %p - %" PRIu64 "(%s)\n",
					        entry, entry->id, entry->name);
				}
			}
		} else if(node->type == FS_NODE_SYMLINK) {
			kprintf(LOG_NONE, "Destination:  %p(%s)\n", node->link_cache,
			        (node->link_cache) ? node->link_cache : "<not cached>");
		} else if(node->type == FS_NODE_FILE) {
			kprintf(LOG_NONE, "Cache:        %p\n", node->data_cache);
		}
	} else {
		if(argc == 3) {
			if(strcmp(argv[1], "--unused") == 0) {
				list = &mount->unused_nodes;
			} else if(strcmp(argv[1], "--used") == 0) {
				list = &mount->used_nodes;
			} else {
				kprintf(LOG_NONE, "Unrecognized argument '%s'.\n", argv[1]);
				return KDBG_FAIL;
			}
		}

		kprintf(LOG_NONE, "ID       Flags Count Locked Type Ops                Data               Mount\n");
		kprintf(LOG_NONE, "==       ===== ===== ====== ==== ===                ====               =====\n");

		if(list) {
			LIST_FOREACH(list, iter) {
				node = list_entry(iter, fs_node_t, mount_link);

				kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-5d %-6d %-4d %-18p %-18p %p\n",
				        node->id, node->flags, refcount_get(&node->count),
				        atomic_get(&node->lock.locked), node->type, node->ops,
				        node->data, node->mount);
			}
		} else {
			AVL_TREE_FOREACH(&mount->nodes, iter) {
				node = avl_tree_entry(iter, fs_node_t);

				kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-5d %-6d %-4d %-18p %-18p %p\n",
				        node->id, node->flags, refcount_get(&node->count),
				        atomic_get(&node->lock.locked), node->type, node->ops,
				        node->data, node->mount);
			}
		}
	}

	return KDBG_OK;
}

/** Mount the root filesystem.
 * @param args		Kernel arguments structure. */
void __init_text fs_mount_root(kernel_args_t *args) {
	if(!root_mount) {
		fatal("Root filesystem probe not implemented");
	}
}

/** Create the filesystem node cache. */
void __init_text fs_init(void) {
	fs_node_cache = slab_cache_create("fs_node_cache", sizeof(fs_node_t), 0,
	                                  fs_node_ctor, NULL, NULL,
	                                  NULL, 1, NULL, 0, MM_FATAL);
}

/** Create a regular file in the file system.
 * @param path		Path to file to create.
 * @return		0 on success, negative error code on failure. */
int sys_fs_file_create(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = fs_file_create(kpath);
	kfree(kpath);
	return ret;
}

/** Open a handle to a file.
 * @param path		Path to file to open.
 * @param flags		Behaviour flags for the handle.
 * @return		Handle ID on success, negative error code on failure. */
handle_t sys_fs_file_open(const char *path, int flags) {
	object_handle_t *handle;
	char *kpath = NULL;
	handle_t ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = fs_file_open(kpath, flags, &handle)) != 0) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(curr_proc, handle);
	object_handle_release(handle);
	kfree(kpath);
	return ret;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur from the file
 * handle's current offset, and before returning the offset will be incremented
 * by the number of bytes read.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp) {
	object_handle_t *obj = NULL;
	size_t bytes = 0;
	int ret, err;
	void *kbuf;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = fs_file_read(obj, kbuf, count, &bytes);
	if(bytes) {
		if((err = memcpy_to_user(buf, kbuf, bytes)) != 0) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(obj) {
		object_handle_release(obj);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_pread(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *obj = NULL;
	size_t bytes = 0;
	int ret, err;
	void *kbuf;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = fs_file_pread(obj, kbuf, count, offset, &bytes);
	if(bytes) {
		if((err = memcpy_to_user(buf, kbuf, bytes)) != 0) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(obj) {
		object_handle_release(obj);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the file
 * handle's current offset (if the FS_FILE_APPEND flag is set, the offset will
 * be set to the end of the file and the write will take place there), and
 * before returning the handle's offset will be incremented by the number of
 * bytes written.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FS_FILE_APPEND is
 *			set).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp) {
	object_handle_t *obj = NULL;
	void *kbuf = NULL;
	size_t bytes = 0;
	int ret, err;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	} else if((ret = memcpy_from_user(kbuf, buf, count)) != 0) {
		goto out;
	}

	/* Perform the actual write. */
	ret = fs_file_write(obj, kbuf, count, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(obj) {
		object_handle_release(obj);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments.
 * @param offset	Offset into file to write at.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_pwrite(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *obj = NULL;
	void *kbuf = NULL;
	size_t bytes = 0;
	int ret, err;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	} else if((ret = memcpy_from_user(kbuf, buf, count)) != 0) {
		goto out;
	}

	/* Perform the actual write. */
	ret = fs_file_pwrite(obj, kbuf, count, offset, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(obj) {
		object_handle_release(obj);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Modify the size of a file.
 *
 * Modifies the size of a file in the file system. If the new size is smaller
 * than the previous size of the file, then the extra data is discarded. If
 * it is larger than the previous size, then the extended space will be filled
 * with zero bytes.
 *
 * @param handle	Handle to file to resize.
 * @param size		New size of the file.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_resize(handle_t handle, offset_t size) {
	object_handle_t *obj;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		return ret;
	}

	ret = fs_file_resize(obj, size);
	object_handle_release(obj);
	return ret;
}

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @return		0 on success, negative error code on failure. */
int sys_fs_dir_create(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = fs_dir_create(kpath);
	kfree(kpath);
	return ret;
}

/** Open a handle to a directory.
 * @param path		Path to directory to open.
 * @param flags		Behaviour flags for the handle.
 * @return		Handle ID on success, negative error code on failure. */
handle_t sys_fs_dir_open(const char *path, int flags) {
	object_handle_t *handle;
	char *kpath = NULL;
	handle_t ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = fs_dir_open(kpath, flags, &handle)) != 0) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(curr_proc, handle);
	object_handle_release(handle);
	kfree(kpath);
	return ret;
}

/** Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed. The number of the entry read
 * will be the handle's current offset, and upon success the handle's offset
 * will be incremented by 1.
 *
 * @param handle	Handle to directory to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_BUF_TOO_SMALL
 *			will be returned).
 *
 * @return		0 on success, negative error code on failure. If the
 *			handle's offset is past the end of the directory,
 *			-ERR_NOT_FOUND will be returned.
 */
int sys_fs_dir_read(handle_t handle, fs_dir_entry_t *buf, size_t size) {
	fs_dir_entry_t *kbuf;
	object_handle_t *obj;
	int ret;

	if(!size) {
		return -ERR_BUF_TOO_SMALL;
	} else if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_DIR, &obj)) != 0) {
		return ret;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(size, 0))) {
		object_handle_release(obj);
		return -ERR_NO_MEMORY;
	}

	/* Perform the actual read. */
	if((ret = fs_dir_read(obj, kbuf, size)) == 0) {
		ret = memcpy_to_user(buf, kbuf, kbuf->length);
	}

	kfree(kbuf);
	object_handle_release(obj);
	return ret;
}

/** Set the offset of a file/directory handle.
 *
 * Modifies the offset of a file or directory handle according to the specified
 * action, and returns the new offset. For directories, the offset is the
 * index of the next directory entry that will be read.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FS_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_handle_seek(handle_t handle, int action, rel_offset_t offset, offset_t *newp) {
	object_handle_t *obj;
	offset_t new;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) != 0) {
		return ret;
	}

	if((ret = fs_handle_seek(obj, action, offset, &new)) == 0 && newp) {
		ret = memcpy_to_user(newp, &new, sizeof(offset_t));
	}
	object_handle_release(obj);
	return ret;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information on.
 * @param info		Information structure to fill in.
 * @return		0 on success, negative error code on failure. */
int sys_fs_handle_info(handle_t handle, fs_info_t *info) {
	object_handle_t *obj;
	fs_info_t kinfo;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) != 0) {
		return ret;
	}

	if((ret = fs_handle_info(obj, &kinfo)) == 0) {
		ret = memcpy_to_user(info, &kinfo, sizeof(fs_info_t));
	}
	object_handle_release(obj);
	return ret;
}

/** Flush changes to a filesystem node to the FS.
 * @param handle	Handle to node to flush.
 * @return		0 on success, negative error code on failure. */
int sys_fs_handle_sync(handle_t handle) {
	object_handle_t *obj;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) != 0) {
		return ret;
	}

	ret = fs_handle_sync(obj);
	object_handle_release(obj);
	return ret;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		0 on success, negative error code on failure. */
int sys_fs_symlink_create(const char *path, const char *target) {
	char *kpath, *ktarget;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = strndup_from_user(target, PATH_MAX, MM_SLEEP, &ktarget)) != 0) {
		kfree(kpath);
		return ret;
	}

	ret = fs_symlink_create(kpath, ktarget);
	kfree(ktarget);
	kfree(kpath);
	return ret;
}

/** Get the destination of a symbolic link.
 *
 * Reads the destination of a symbolic link into a buffer. A NULL byte will
 * be placed at the end of the buffer, unless the buffer is too small.
 *
 * @param path		Path to symbolic link.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer (destination will be truncated if buffer
 *			is too small).
 *
 * @return		Number of bytes read on success, negative error code
 *			on failure.
 */
int sys_fs_symlink_read(const char *path, char *buf, size_t size) {
	char *kpath, *kbuf;
	int ret, err;

	/* Copy the path across. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	/* Allocate a buffer to read into. See comment in sys_fs_file_read()
	 * about not using MM_SLEEP. */
	if(!(kbuf = kmalloc(size, 0))) {
		kfree(kpath);
		return -ERR_NO_MEMORY;
	}

	ret = fs_symlink_read(kpath, kbuf, size);
	if(ret > 0) {
		if((err = memcpy_to_user(buf, kbuf, size)) != 0) {
			ret = err;
		}
	}

	kfree(kpath);
	kfree(kbuf);
	return ret;
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * The opts parameter allows a string containing a list of comma-seperated
 * mount options to be passed. Some options are recognised by this file system:
 *  - ro - Mount the filesystem read-only.
 * All other options are passed through to the filesystem implementation.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 *
 * @param dev		Device tree path to device filesystem resides on (can
 *			be NULL if the filesystem does not require a device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type (if not specified, device will
 *			be probed to determine the correct type - in this case,
 *			a device must be specified).
 * @param opts		Options string.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_mount(const char *dev, const char *path, const char *type, const char *opts) {
	char *kdev = NULL, *kpath = NULL, *ktype = NULL, *kopts = NULL;
	int ret;

	/* Copy string arguments across from userspace. */
	if(dev) {
		if((ret = strndup_from_user(dev, PATH_MAX, MM_SLEEP, &kdev)) != 0) {
			goto out;
		}
	}
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		goto out;
	}
	if(type) {
		if((ret = strndup_from_user(type, PATH_MAX, MM_SLEEP, &ktype)) != 0) {
			goto out;
		}
	}
	if(opts) {
		if((ret = strndup_from_user(opts, PATH_MAX, MM_SLEEP, &kopts)) != 0) {
			goto out;
		}
	}

	ret = fs_mount(kdev, kpath, ktype, kopts);
out:
	if(kdev) { kfree(kdev); }
	if(kpath) { kfree(kpath); }
	if(ktype) { kfree(ktype); }
	if(kopts) { kfree(kopts); }
	return ret;
}

/** Unmounts a filesystem.
 *
 * Flushes all modifications to a filesystem if it is not read-only and
 * unmounts it. If any nodes in the filesystem are busy, then the operation
 * will fail.
 *
 * @param path		Path to mount point of filesystem.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_unmount(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = fs_unmount(kpath);
	kfree(kpath);
	return ret;
}

/** Flush all cached filesystem changes. */
int sys_fs_sync(void) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to store in.
 * @param size		Size of buffer.
 * @return		0 on success, negative error code on failure. */
int sys_fs_getcwd(char *buf, size_t size) {
	char *kbuf = NULL, *tmp, path[3];
	fs_dir_cache_t *cache;
	fs_dir_entry_t *entry;
	fs_node_t *node;
	size_t len = 0;
	node_id_t id;
	int ret;

	if(!buf || !size) {
		return -ERR_PARAM_INVAL;
	}

	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Get the working directory. */
	node = curr_proc->ioctx.curr_dir;
	mutex_lock(&node->lock);
	fs_node_get(node);

	/* Loop through until we reach the root. */
	while(node != curr_proc->ioctx.root_dir) {
		/* Save the current node's ID. Use the mountpoint ID if this is
		 * the root of the mount. */
		id = (node == node->mount->root) ? node->mount->mountpoint->id : node->id;

		/* Get the parent of the node. */
		strcpy(path, "..");
		if((ret = fs_node_lookup_internal(path, node, false, 0, &node)) != 0) {
			node = NULL;
			goto fail;
		} else if(node->type != FS_NODE_DIR) {
			dprintf("fs: node %p(%" PRIu64 ") should be a directory but it isn't!\n",
			        node, node->id);
			ret = -ERR_TYPE_INVAL;
			goto fail;
		}

		/* Now try to find the old node in this directory. */
		if((ret = fs_dir_get_cache(node, &cache)) != 0) {
			goto fail;
		}
		entry = NULL;
		RADIX_TREE_FOREACH(&cache->entries, iter) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);
			if(entry->id == id) {
				break;
			} else {
				entry = NULL;
			}
		}
		if(!entry) {
			/* Directory has probably been unlinked. */
			ret = -ERR_NOT_FOUND;
			goto fail;
		}

		/* Add the entry name on to the beginning of the path. */
		len += ((kbuf) ? strlen(entry->name) + 1 : strlen(entry->name));
		tmp = kmalloc(len + 1, MM_SLEEP);
		strcpy(tmp, entry->name);
		if(kbuf) {
			strcat(tmp, "/");
			strcat(tmp, kbuf);
			kfree(kbuf);
		}
		kbuf = tmp;
	}

	mutex_unlock(&node->lock);
	fs_node_release(node);
	rwlock_unlock(&curr_proc->ioctx.lock);

	/* Prepend a '/'. */
	tmp = kmalloc((++len) + 1, MM_SLEEP);
	strcpy(tmp, "/");
	if(kbuf) {
		strcat(tmp, kbuf);
		kfree(kbuf);
	}
	kbuf = tmp;

	if(len >= size) {
		ret = -ERR_BUF_TOO_SMALL;
	} else {
		ret = memcpy_to_user(buf, kbuf, len + 1);
	}
	kfree(kbuf);
	return ret;
fail:
	if(node) {
		mutex_unlock(&node->lock);
		fs_node_release(node);
	}
	rwlock_unlock(&curr_proc->ioctx.lock);
	kfree(kbuf);
	return ret;
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		0 on success, negative error code on failure. */
int sys_fs_setcwd(const char *path) {
	fs_node_t *node;
	char *kpath;
	int ret;

	/* Get the path and look it up. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = fs_node_lookup(kpath, true, FS_NODE_DIR, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. Release the node no matter what, as upon success it
	 * is referenced by io_context_setcwd(). */
	ret = io_context_setcwd(&curr_proc->ioctx, node);
	fs_node_release(node);
	kfree(kpath);
	return ret;
}

/** Set the root directory.
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
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_setroot(const char *path) {
	fs_node_t *node;
	char *kpath;
	int ret;

	/* Get the path and look it up. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = fs_node_lookup(kpath, true, FS_NODE_DIR, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. Release the node no matter what, as upon success it
	 * is referenced by io_context_setroot(). */
	ret = io_context_setroot(&curr_proc->ioctx, node);
	fs_node_release(node);
	kfree(kpath);
	return ret;
}

/** Get information about a node.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		0 on success, negative error code on failure. */
int sys_fs_info(const char *path, bool follow, fs_info_t *info) {
	fs_info_t kinfo;
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	if((ret = fs_info(kpath, follow, &kinfo)) == 0) {
		ret = memcpy_to_user(info, &kinfo, sizeof(fs_info_t));
	}
	kfree(kpath);
	return ret;
}

int sys_fs_link(const char *source, const char *dest) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Decrease the link count of a filesystem node.
 *
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path		Path to node to decrease link count of.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_unlink(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = fs_unlink(kpath);
	kfree(kpath);
	return ret;
}

int sys_fs_rename(const char *source, const char *dest) {
	return -ERR_NOT_IMPLEMENTED;
}
