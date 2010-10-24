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
 */

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>
#include <mm/vm_cache.h>

#include <proc/process.h>

#include <security/cap.h>

#include <assert.h>
#include <console.h>
#include <kargs.h>
#include <kdbg.h>
#include <lrm.h>
#include <status.h>

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
static status_t fs_dir_lookup(fs_node_t *node, const char *name, node_id_t *idp);
static object_type_t file_object_type;
static object_type_t dir_object_type;

/** Pointer to the boot FS UUID string. */
static const char *boot_fs_uuid = NULL;

/** Whether to force FS image usage. */
static bool force_fsimage = false;

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

	type = fs_type_lookup_internal(name);
	if(type) {
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
 * @return		Status code describing result of the operation. */
status_t fs_type_register(fs_type_t *type) {
	/* Check whether the structure is valid. */
	if(!type || !type->name || !type->description || !type->mount) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&fs_types_lock);

	/* Check if this type already exists. */
	if(fs_type_lookup_internal(type->name) != NULL) {
		mutex_unlock(&fs_types_lock);
		return STATUS_ALREADY_EXISTS;
	}

	refcount_set(&type->count, 0);
	list_init(&type->header);
	list_append(&fs_types, &type->header);

	kprintf(LOG_NORMAL, "fs: registered filesystem type %s (%s)\n",
	        type->name, type->description);
	mutex_unlock(&fs_types_lock);
	return STATUS_SUCCESS;
}

/** Remove a filesystem type.
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
	if(fs_type_lookup_internal(type->name) != type) {
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
	list_init(&node->mount_link);
	list_init(&node->unused_link);
	node->id = id;
	node->type = type;
	node->removed = false;
	node->mounted = NULL;
	node->ops = ops;
	node->data = data;
	node->mount = mount;

	// FIXME
	object_acl_t acl;
	object_security_t security = { 0, 0, &acl };
	object_acl_init(&acl);
	object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0,
	                     OBJECT_SET_ACL | OBJECT_SET_OWNER | FS_READ | FS_WRITE | FS_EXECUTE);

	/* Initialise the node's object header. */
	switch(type) {
	case FS_NODE_FILE:
		object_init(&node->obj, &file_object_type, &security, NULL);
		break;
	case FS_NODE_DIR:
		object_init(&node->obj, &dir_object_type, &security, NULL);
		break;
	default:
		object_init(&node->obj, NULL, &security, NULL);
		break;
	}

	return node;
}

/** Flush changes to a node and free it.
 * @note		Never call this function unless it is necessary. Use
 *			fs_node_release().
 * @note		Mount lock (if there is a mount) must be held.
 * @param node		Node to free. Should be unused (zero reference count).
 * @return		Status code describing result of the operation. */
static status_t fs_node_free(fs_node_t *node) {
	status_t ret;

	assert(refcount_get(&node->count) == 0);
	assert(!node->mount || mutex_held(&node->mount->lock));

	/* Call the implementation to flush any changes and free up its data. */
	if(node->ops) {
		if(!FS_NODE_IS_RDONLY(node) && !node->removed && node->ops->flush) {
			ret = node->ops->flush(node);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		}
		if(node->ops->free) {
			node->ops->free(node);
		}
	}

	/* If the node has a mount, detach it from the node tree/lists. */
	if(node->mount) {
		avl_tree_remove(&node->mount->nodes, (key_t)node->id);
		list_remove(&node->mount_link);
	}

	mutex_lock(&unused_nodes_lock);
	list_remove(&node->unused_link);
	unused_nodes_count--;
	mutex_unlock(&unused_nodes_lock);

	object_destroy(&node->obj);
	dprintf("fs: freed node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
	        (node->mount) ? node->mount->id : 0, node->id);
	slab_cache_free(fs_node_cache, node);
	return STATUS_SUCCESS;
}

/** Low resource handler for the FS node cache.
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
		node = list_entry(unused_nodes_list.next, fs_node_t, unused_link);
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

/** FS low resource handler. */
static lrm_handler_t fs_lrm_handler = {
	.types = RESOURCE_TYPE_MEMORY | RESOURCE_TYPE_KASPACE,
	.priority = LRM_FS_PRIORITY,
	.func = fs_node_reclaim,
};

/** Look up a node in the filesystem.
 * @param path		Path string to look up.
 * @param node		Node to begin lookup at (referenced). Ignored if path
 *			is absolute.
 * @param follow	Whether to follow last path component if it is a
 *			symbolic link.
 * @param nest		Symbolic link nesting count.
 * @param nodep		Where to store pointer to node found (referenced).
 * @return		Status code describing result of the operation. */
static status_t fs_node_lookup_internal(char *path, fs_node_t *node, bool follow, int nest,
                                        fs_node_t **nodep) {
	fs_node_t *prev = NULL;
	fs_mount_t *mount;
	char *tok, *link;
	node_id_t id;
	status_t ret;

	/* Check whether the path is an absolute path. */
	if(path[0] == '/') {
		/* Drop the node we were provided, if any. */
		if(node != NULL) {
			fs_node_release(node);
		}

		/* Strip off all '/' characters at the start of the path. */
		while(path[0] == '/') {
                        path++;
                }

		/* Get the root node of the current process. */
		assert(curr_proc->ioctx.root_dir);
		node = curr_proc->ioctx.root_dir;
		fs_node_get(node);

		assert(node->type == FS_NODE_DIR);

		/* Return the root node if the end of the path has been reached. */
		if(!path[0]) {
			*nodep = node;
			return STATUS_SUCCESS;
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
			if(++nest > FS_NESTED_LINK_MAX) {
				fs_node_release(prev);
				fs_node_release(node);
				return STATUS_SYMLINK_LIMIT;
			}

			/* Obtain the link destination. */
			assert(node->ops->read_link);
			ret = node->ops->read_link(node, &link);
			if(ret != STATUS_SUCCESS) {
				fs_node_release(prev);
				fs_node_release(node);
				return ret;
			}

			dprintf("fs: following symbolic link %" PRIu16 ":%" PRIu64 " to %s\n",
			        node->mount->id, node->id, link);

			/* Move up to the parent node. The previous iteration
			 * of the loop left a reference on the previous node
			 * for us. */
			fs_node_release(node);
			node = prev;

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. */
			ret = fs_node_lookup_internal(link, node, true, nest, &node);
			if(ret != STATUS_SUCCESS) {
				kfree(link);
				return ret;
			}

			dprintf("fs: followed %s to %" PRIu16 ":%" PRIu64 "\n",
			        link, node->mount->id, node->id);
			kfree(link);
		} else if(node->type == FS_NODE_SYMLINK) {
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
		} else if(node->type != FS_NODE_DIR) {
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
		if(!(object_rights(&node->obj, NULL) & FS_EXECUTE)) {
			fs_node_release(node);
			return STATUS_PERM_DENIED;
		}

		/* Special handling for descending out of the directory. */
		if(tok[0] == '.' && tok[1] == '.' && !tok[2]) {
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
				fs_node_release(prev);
			}
		}

		/* Look up this name within the directory. */
		ret = fs_dir_lookup(node, tok, &id);
		if(ret != STATUS_SUCCESS) {
			fs_node_release(node);
			return ret;
		}

		/* If the ID is the same as the current node (e.g. the '.'
		 * entry), do nothing. */
		if(id == node->id) {
			continue;
		}

		/* Acquire the mount lock. */
		mount = node->mount;
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
			if(!mount->ops->read_node) {
				mutex_unlock(&mount->lock);
				fs_node_release(prev);
				return STATUS_NOT_SUPPORTED;
			}

			ret = mount->ops->read_node(mount, id, &node);
			if(ret != STATUS_SUCCESS) {
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

		/* Do not release the previous node if the new node is a
		 * symbolic link, as the symbolic link lookup requires it. */
		if(node->type != FS_NODE_SYMLINK) {
			fs_node_release(prev);
		}
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

	if(!path[0]) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Start from the current directory if the path is relative. */
	if(path[0] != '/') {
		assert(curr_proc->ioctx.curr_dir);
		node = curr_proc->ioctx.curr_dir;
		fs_node_get(node);
	}

	/* Duplicate path so that fs_node_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_SLEEP);

	/* Look up the path string. */
	ret = fs_node_lookup_internal(dup, node, follow, 0, &node);
	if(ret == STATUS_SUCCESS) {
		if(type >= 0 && node->type != (fs_node_type_t)type) {
			if(type == FS_NODE_FILE) {
				ret = STATUS_NOT_FILE;
			} else if(type == FS_NODE_DIR) {
				ret = STATUS_NOT_DIR;
			} else if(type == FS_NODE_SYMLINK) {
				ret = STATUS_NOT_SYMLINK;
			} else {
				/* FIXME. */
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
	status_t ret;

	if(node->mount) {
		mutex_lock(&node->mount->lock);
		mount = node->mount;
	}

	if(refcount_dec(&node->count) == 0) {
		assert(!node->mounted);

		/* Node has no references remaining, move it to its mount's
		 * unused list if it has a mount. If the node is not attached
		 * to anything or is removed, then destroy it immediately. */
		if(mount && !node->removed && !list_empty(&node->mount_link)) {
			list_append(&node->mount->unused_nodes, &node->mount_link);

			mutex_lock(&unused_nodes_lock);
			list_append(&unused_nodes_list, &node->unused_link);
			unused_nodes_count++;
			mutex_unlock(&unused_nodes_lock);

			dprintf("fs: transferred node %p to unused list (mount: %p)\n", node, node->mount);
			mutex_unlock(&mount->lock);
		} else {
			/* This shouldn't fail - the only thing that can fail
			 * in fs_node_free() is flushing data. Since this node
			 * has no source to flush to, or has been removed, this
			 * should not fail. */
			ret = fs_node_free(node);
			if(ret != STATUS_SUCCESS) {
				fatal("Could not destroy %s (%d)",
				      (mount) ? "removed node" : "node with no mount",
				      ret);
			}
			if(mount) {
				mutex_unlock(&mount->lock);
			}
		}
	} else {
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
	node->removed = true;
}

/** Common node creation code.
 * @param path		Path to node to create.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @return		Status code describing result of the operation. */
static status_t fs_node_create(const char *path, fs_node_type_t type, const char *target) {
	fs_node_t *parent = NULL, *node = NULL;
	char *dir, *name;
	node_id_t id;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	/* It is possible for kbasename() to return a string with a '/'
	 * character if the path refers to the root of the FS. */
	if(strchr(name, '/')) {
		ret = STATUS_ALREADY_EXISTS;
		goto out;
	}

	dprintf("fs: create(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                ret = STATUS_ALREADY_EXISTS;
		goto out;
        }

	/* Look up the parent node. */
	ret = fs_node_lookup(dir, true, FS_NODE_DIR, &parent);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	mutex_lock(&parent->mount->lock);

	/* Check that we are on a writable filesystem, that we have write
	 * permission to the directory, and that the FS supports node
	 * creation. */
	if(FS_NODE_IS_RDONLY(parent)) {
		ret = STATUS_READ_ONLY;
		goto out;
	} else if(!(object_rights(&parent->obj, NULL) & FS_WRITE)) {
		ret = STATUS_PERM_DENIED;
		goto out;
	} else if(!parent->ops->create) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	/* Check if the name we're creating already exists. This will populate
	 * the entry cache so it will be OK to add the node to it. */
	ret = fs_dir_lookup(parent, name, &id);
	if(ret != STATUS_NOT_FOUND) {
		if(ret == STATUS_SUCCESS) {
			ret = STATUS_ALREADY_EXISTS;
		}
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	ret = parent->ops->create(parent, name, type, target, &node);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&node->mount->nodes, (key_t)node->id, node, NULL);
	list_append(&node->mount->used_nodes, &node->mount_link);

	dprintf("fs: created %s (node: %" PRIu16 ":%" PRIu64 ", parent: %" PRIu16 ":%" PRIu64 ")\n",
	        path, node->mount->id, node->id, parent->mount->id, parent->id);
out:
	if(parent) {
		mutex_unlock(&parent->mount->lock);
		fs_node_release(parent);
	}
	if(node) {
		fs_node_release(node);
	}
	kfree(dir);
	kfree(name);
	return ret;
}

/** Get information about a node.
 * @param node		Node to get information for.
 * @param info		Structure to store information in. */
static void fs_node_info(fs_node_t *node, fs_info_t *info) {
	memset(info, 0, sizeof(fs_info_t));
	info->id = node->id;
	info->mount = (node->mount) ? node->mount->id : 0;
	info->type = node->type;
	if(node->ops->info) {
		node->ops->info(node, info);
	} else {
		info->links = 1;
		info->size = 0;
		info->blksize = PAGE_SIZE;
	}
}

/** Get the name of a node in its parent directory.
 * @param parent	Directory containing node.
 * @param id		ID of node to get name of.
 * @param namep		Where to store pointer to string containing node name.
 * @return		Status code describing result of the operation. */
static status_t fs_node_name(fs_node_t *parent, node_id_t id, char **namep) {
	fs_dir_entry_t *entry;
	offset_t index = 0;
	status_t ret;

	if(!parent->ops->read_entry) {
		return STATUS_NOT_SUPPORTED;
	}

	while(true) {
		ret = parent->ops->read_entry(parent, index++, &entry);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		if(entry->id == id) {
			*namep = kstrdup(entry->name, MM_SLEEP);
			kfree(entry);
			return STATUS_SUCCESS;
		}

		kfree(entry);
	}
}

/** Create a handle to a node.
 * @param node		Node to create handle to (will be referenced).
 * @param rights	Requested access rights for the handle.
 * @param flags		Flags for the handle.
 * @param handlep	Where to store pointer to handle.
 * @return		Status code describing result of the operation. */
static status_t fs_handle_create(fs_node_t *node, object_rights_t rights, int flags,
                                 object_handle_t **handlep) {
	fs_handle_t *data;
	status_t ret;

	/* Prevent opening for writing on a read-only filesystem. */
	if(rights & (OBJECT_SET_ACL | OBJECT_SET_OWNER | FS_WRITE) && FS_NODE_IS_RDONLY(node)) {
		return STATUS_READ_ONLY;
	}

	/* Allocate the per-handle data structure. */
	data = kmalloc(sizeof(fs_handle_t), MM_SLEEP);
	rwlock_init(&data->lock, "fs_handle_lock");
	data->offset = 0;
	data->flags = flags;

	fs_node_get(node);

	/* Create the handle. */
	ret = object_handle_create(&node->obj, data, rights, NULL, 0, handlep, NULL, NULL);
	if(ret != STATUS_SUCCESS) {
		fs_node_release(node);
		kfree(data);
	}

	return ret;
}

/** Validate a filesystem security attributes change.
 * @param object	Object to check.
 * @param security	New security attributes.
 * @return		STATUS_SUCCESS if change should be allowed, other
 *			status code if not. */
static status_t fs_object_set_security(object_t *object, object_security_t *security) {
	fs_node_t *node = (fs_node_t *)object;

	if(FS_NODE_IS_RDONLY(node)) {
		return STATUS_READ_ONLY;
	}

	return STATUS_SUCCESS;
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
 * @return		STATUS_SUCCESS if mappable, other status code if not. */
static status_t file_object_mappable(object_handle_t *handle, int flags) {
	fs_node_t *node = (fs_node_t *)handle->object;

	/* Check whether the filesystem supports memory-mapping. */
	if(!node->ops->get_cache) {
		return STATUS_NOT_SUPPORTED;
	}

	/* If mapping for reading, check if allowed. */
	if(flags & VM_MAP_READ && !object_handle_rights(handle, FS_READ)) {
		return STATUS_PERM_DENIED;
	}

	/* If creating a shared mapping for writing, check for write access.
	 * It is not necessary to check for a read-only filesystem here: a
	 * handle cannot be opened with FS_WRITE on a read-only FS. */
	if((flags & (VM_MAP_WRITE | VM_MAP_PRIVATE)) == VM_MAP_WRITE && !object_handle_rights(handle, FS_WRITE)) {
		return STATUS_PERM_DENIED;
	}

	/* If mapping for execution, check for execute access. */
	if(flags & VM_MAP_EXEC && !object_handle_rights(handle, FS_EXECUTE)) {
		return STATUS_PERM_DENIED;
	}

	return STATUS_SUCCESS;
}

/** Get a page from a file object.
 * @param handle	Handle to file.
 * @param offset	Offset of page to get.
 * @param physp		Where to store physical address of page.
 * @return		Status code describing result of the operation. */
static status_t file_object_get_page(object_handle_t *handle, offset_t offset, phys_ptr_t *physp) {
	fs_node_t *node = (fs_node_t *)handle->object;
	vm_cache_t *cache;

	assert(node->ops->get_cache);

	cache = node->ops->get_cache(node);
	return vm_cache_get_page(cache, offset, physp);
}

/** Release a page from a file object.
 * @param handle	Handle to file.
 * @param offset	Offset of page to release.
 * @param phys		Physical address of page that was unmapped. */
static void file_object_release_page(object_handle_t *handle, offset_t offset, phys_ptr_t phys) {
	fs_node_t *node = (fs_node_t *)handle->object;
	vm_cache_t *cache;

	assert(node->ops->get_cache);

	cache = node->ops->get_cache(node);
	vm_cache_release_page(cache, offset, phys);
}

/** File object operations. */
static object_type_t file_object_type = {
	.id = OBJECT_TYPE_FILE,
	.set_security = fs_object_set_security,
	.close = file_object_close,
	.mappable = file_object_mappable,
	.get_page = file_object_get_page,
	.release_page = file_object_release_page,
};

/** Create a regular file in the file system.
 * @param path		Path to file to create.
 * @return		Status code describing result of the operation. */
status_t fs_file_create(const char *path) {
	return fs_node_create(path, FS_NODE_FILE, NULL);
}

/** Structure containing details of a memory file. */
typedef struct memory_file {
	const char *data;		/**< Data for the file. */
	size_t size;			/**< Size of the file. */
} memory_file_t;

/** Free a memory file.
 * @param node		Node to free. */
static void memory_file_free(fs_node_t *node) {
	kfree(node->data);
}

/** Read from a memory file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into file to read from.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t memory_file_read(fs_node_t *node, void *buf, size_t count, offset_t offset,
                                 bool nonblock, size_t *bytesp) {
	memory_file_t *file = node->data;

	if(offset >= file->size) {
		*bytesp = 0;
		return STATUS_SUCCESS;
	} else if((offset + count) > file->size) {
		count = file->size - offset;
	}

	memcpy(buf, file->data + offset, count);
	*bytesp = count;
	return STATUS_SUCCESS;
}

/** Operations for an in-memory file. */
static fs_node_ops_t memory_file_ops = {
	.free = memory_file_free,
	.read = memory_file_read,
};

/** Create a read-only file backed by a chunk of memory.
 *
 * Creates a special read-only file that is backed by the specified chunk of
 * memory. This is useful to pass data stored in memory to code that expects
 * to be operating on filesystem entries, such as the module loader.
 *
 * The given memory area will not be duplicated, and therefore it must remain
 * in memory for the lifetime of the node.
 *
 * The file is not attached anywhere in the filesystem, and therefore when the
 * handle is closed, it will be immediately destroyed.
 *
 * @note		Files created with this function do not support being
 *			memory-mapped.
 *
 * @param buf		Pointer to memory area to use.
 * @param size		Size of memory area.
 *
 * @return		Pointer to handle to file (has FS_READ access right).
 */
object_handle_t *fs_file_from_memory(const void *buf, size_t size) {
	object_handle_t *handle;
	memory_file_t *file;
	fs_node_t *node;

	// FIXME: Will need to create an ACL here.
	file = kmalloc(sizeof(memory_file_t), MM_SLEEP);
	file->data = buf;
	file->size = size;
	node = fs_node_alloc(NULL, 0, FS_NODE_FILE, &memory_file_ops, file);

	if(fs_handle_create(node, FS_READ, 0, &handle) != STATUS_SUCCESS) {
		fatal("Should not fail to create memory file");
	}

	fs_node_release(node);
	return handle;
}

/** Open a handle to a file.
 * @param path		Path to file to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		Status code describing result of the operation. */
status_t fs_file_open(const char *path, object_rights_t rights, int flags, object_handle_t **handlep) {
	fs_node_t *node;
	status_t ret;

	/* Look up the filesystem node. */
	ret = fs_node_lookup(path, true, FS_NODE_FILE, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_handle_create(node, rights, flags, handlep);
	fs_node_release(node);
	return ret;
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
 * @return		Status code describing result of the operation. */
static status_t fs_file_read_internal(object_handle_t *handle, void *buf, size_t count,
                                      offset_t offset, bool usehnd, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	fs_handle_t *data;
	size_t total = 0;
	fs_node_t *node;

	if(!handle || !buf) {
		ret = STATUS_INVALID_ARG;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = STATUS_INVALID_HANDLE;
		goto out;
	} else if(!object_handle_rights(handle, FS_READ)) {
		ret = STATUS_PERM_DENIED;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_FILE);

	if(!node->ops->read) {
		ret = STATUS_NOT_SUPPORTED;
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

	ret = node->ops->read(node, buf, count, offset, data->flags & FS_NONBLOCK, &total);
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
 * @param handle	Handle to file to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_file_read(object_handle_t *handle, void *buf, size_t count, size_t *bytesp) {
	return fs_file_read_internal(handle, buf, count, 0, true, bytesp);
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_file_pread(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
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
 * @return		Status code describing result of the operation. */
static status_t fs_file_write_internal(object_handle_t *handle, const void *buf, size_t count,
                                       offset_t offset, bool usehnd, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	fs_handle_t *data;
	size_t total = 0;
	fs_node_t *node;
	fs_info_t info;

	if(!handle || !buf) {
		ret = STATUS_INVALID_ARG;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = STATUS_INVALID_HANDLE;
		goto out;
	} else if(!object_handle_rights(handle, FS_WRITE)) {
		ret = STATUS_PERM_DENIED;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_FILE);

	if(!node->ops->write) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Pull the offset out of the handle structure, and handle the
	 * FS_FILE_APPEND flag. */
	if(usehnd) {
		rwlock_write_lock(&data->lock);
		if(data->flags & FS_FILE_APPEND) {
			fs_node_info(node, &info);
			data->offset = info.size;
		}
		offset = data->offset;
		rwlock_unlock(&data->lock);
	}

	ret = node->ops->write(node, buf, count, offset, data->flags & FS_NONBLOCK, &total);
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
 * @param handle	Handle to file to write to. Must have the FS_WRITE
 *			access right.
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
 * @return		Status code describing result of the operation.
 */
status_t fs_file_write(object_handle_t *handle, const void *buf, size_t count, size_t *bytesp) {
	return fs_file_write_internal(handle, buf, count, 0, true, bytesp);
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to write to. Must have the FS_WRITE
 *			access right.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments.
 * @param offset	Offset into file to write at.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_file_pwrite(object_handle_t *handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	return fs_file_write_internal(handle, buf, count, offset, false, bytesp);
}

/** Modify the size of a file.
 *
 * Modifies the size of a file in the file system. If the new size is smaller
 * than the previous size of the file, then the extra data is discarded. If
 * it is larger than the previous size, then the extended space will be filled
 * with zero bytes.
 *
 * @param handle	Handle to file to resize. Must have the FS_WRITE access
 *			right.
 * @param size		New size of the file.
 *
 * @return		Status code describing result of the operation.
 */
status_t fs_file_resize(object_handle_t *handle, offset_t size) {
	fs_handle_t *data;
	fs_node_t *node;

	if(!handle) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	} else if(!object_handle_rights(handle, FS_WRITE)) {
		return STATUS_PERM_DENIED;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_FILE);

	if(!node->ops->resize) {
		return STATUS_NOT_SUPPORTED;
	}

	return node->ops->resize(node, size);
}

/** Look up an entry in a directory.
 * @param node		Node to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry maps to.
 * @return		Status code describing result of the operation. */
static status_t fs_dir_lookup(fs_node_t *node, const char *name, node_id_t *idp) {
	if(!node->ops->lookup_entry) {
		return STATUS_NOT_SUPPORTED;
	}
	return node->ops->lookup_entry(node, name, idp);
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
	.set_security = fs_object_set_security,
	.close = dir_object_close,
};

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @return		Status code describing result of the operation. */
status_t fs_dir_create(const char *path) {
	return fs_node_create(path, FS_NODE_DIR, NULL);
}

/** Open a handle to a directory.
 * @param path		Path to directory to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		Status code describing result of the operation. */
status_t fs_dir_open(const char *path, object_rights_t rights, int flags, object_handle_t **handlep) {
	fs_node_t *node;
	status_t ret;

	/* Look up the filesystem node. */
	ret = fs_node_lookup(path, true, FS_NODE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_handle_create(node, rights, flags, handlep);
	fs_node_release(node);
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
 * @param handle	Handle to directory to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, the function will
 *			return STATUS_TOO_SMALL).
 *
 * @return		Status code describing result of the operation. If the
 *			handle's offset is past the end of the directory,
 *			STATUS_NOT_FOUND will be returned.
 */
status_t fs_dir_read(object_handle_t *handle, fs_dir_entry_t *buf, size_t size) {
	fs_node_t *child, *node;
	fs_dir_entry_t *entry;
	fs_handle_t *data;
	offset_t index;
	status_t ret;

	if(!handle || !buf) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_DIR) {
		return STATUS_INVALID_HANDLE;
	} else if(!object_handle_rights(handle, FS_READ)) {
		return STATUS_PERM_DENIED;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == FS_NODE_DIR);
	if(!node->ops->read_entry) {
		return STATUS_NOT_SUPPORTED;
	}

	/* Pull the offset out of the handle structure. */
	rwlock_read_lock(&data->lock);
	index = data->offset;
	rwlock_unlock(&data->lock);

	/* Ask the filesystem to read the entry. */
	ret = node->ops->read_entry(node, index, &entry);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Copy the entry across. */
	if(entry->length > size) {
		kfree(entry);
		return STATUS_TOO_SMALL;
	}
	memcpy(buf, entry, entry->length);
	kfree(entry);

	mutex_lock(&node->mount->lock);

	/* Fix up the entry. */
	buf->mount = node->mount->id;
	if(node == node->mount->root && strcmp(buf->name, "..") == 0) {
		/* This is the '..' entry, and the node is the root of its
		 * mount. Change the node ID to be the ID of the mountpoint,
		 * if any. */
		if(node->mount->mountpoint) {
			ret = fs_dir_lookup(node->mount->mountpoint, "..", &buf->id);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&node->mount->lock);
				return ret;
			}
			buf->mount = node->mount->mountpoint->mount->id;
		}
	} else {
		/* Check if the entry refers to a mountpoint. In this case we
		 * need to change the node ID to be the node ID of the mount
		 * root, rather than the mountpoint. If the node the entry
		 * currently points to is not in the cache, then it won't be a
		 * mountpoint (mountpoints are always in the cache). */
		child = avl_tree_lookup(&node->mount->nodes, (key_t)buf->id);
		if(child) {
			if(child != node) {
				/* Mounted pointer is protected by mount lock. */
				if(child->type == FS_NODE_DIR && child->mounted) {
					buf->id = child->mounted->root->id;
					buf->mount = child->mounted->id;
				}
			}
		}
	}

	mutex_unlock(&node->mount->lock);

	/* Update offset in the handle. */
	rwlock_read_lock(&data->lock);
	data->offset++;
	rwlock_unlock(&data->lock);
	return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation.
 */
status_t fs_handle_seek(object_handle_t *handle, int action, rel_offset_t offset, offset_t *newp) {
	fs_handle_t *data;
	fs_node_t *node;
	fs_info_t info;

	if(!handle || (action != FS_SEEK_SET && action != FS_SEEK_ADD && action != FS_SEEK_END)) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	rwlock_write_lock(&data->lock);

	/* Perform the action. */
	switch(action) {
	case FS_SEEK_SET:
		if(offset < 0) {
			rwlock_unlock(&data->lock);
			return STATUS_INVALID_ARG;
		}
		data->offset = (offset_t)offset;
		break;
	case FS_SEEK_ADD:
		if(((rel_offset_t)data->offset + offset) < 0) {
			rwlock_unlock(&data->lock);
			return STATUS_INVALID_ARG;
		}
		data->offset += offset;
		break;
	case FS_SEEK_END:
		if(node->type == FS_NODE_DIR) {
			/* FIXME. */
			rwlock_unlock(&data->lock);
			return STATUS_NOT_IMPLEMENTED;
		} else {
			fs_node_info(node, &info);
			data->offset = info.size + offset;
		}
		break;
	}

	/* Save the new offset if necessary. */
	if(newp) {
		*newp = data->offset;
	}
	rwlock_unlock(&data->lock);
	return STATUS_SUCCESS;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information on.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t fs_handle_info(object_handle_t *handle, fs_info_t *info) {
	fs_node_t *node;

	if(!handle || !info) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	fs_node_info(node, info);
	return STATUS_SUCCESS;
}

/** Flush changes to a filesystem node to the FS.
 * @param handle	Handle to node to flush.
 * @return		Status code describing result of the operation. */
status_t fs_handle_sync(object_handle_t *handle) {
	fs_node_t *node;

	if(!handle) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	if(!FS_NODE_IS_RDONLY(node) && node->ops->flush) {
		return node->ops->flush(node);
	} else {
		return STATUS_SUCCESS;
	}
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
status_t fs_symlink_create(const char *path, const char *target) {
	return fs_node_create(path, FS_NODE_SYMLINK, target);
}

/** Get the destination of a symbolic link.
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
status_t fs_symlink_read(const char *path, char *buf, size_t size) {
	fs_node_t *node;
	status_t ret;
	char *dest;
	size_t len;

	if(!path || !buf || !size) {
		return STATUS_INVALID_ARG;
	}

	/* Find the link node. */
	ret = fs_node_lookup(path, false, FS_NODE_SYMLINK, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(!node->ops->read_link) {
		fs_node_release(node);
		return STATUS_NOT_SUPPORTED;
	}

	/* Read the link destination. */
	ret = node->ops->read_link(node, &dest);
	fs_node_release(node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Check that the provided buffer is large enough. */
	len = strlen(dest);
	if((len + 1) > size) {
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
			if(strlen(name) == 0) {
				continue;
			} else if(value && strlen(value) == 0) {
				value = NULL;
			}

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

/** Probe a device for filesystems.
 * @param device	Device to probe. */
void fs_probe(device_t *device) {
	object_handle_t *handle;
	fs_type_t *type;
	status_t ret;
	char *path;

	if(device_get(device, DEVICE_READ, &handle) != STATUS_SUCCESS) {
		return;
	}

	/* Only probe for the boot FS at the moment. TODO: Notifications for
	 * filesystem detection. */
	if(!root_mount && !force_fsimage) {
		type = fs_type_probe(handle, boot_fs_uuid);
		if(type) {
			path = device_path(device);
			ret = fs_mount(path, "/", type->name, "ro");
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to mount boot filesystem (%d)", ret);
			}

			kprintf(LOG_NORMAL, "fs: mounted boot device %s:%s\n", type->name, path);
			refcount_dec(&type->count);
			kfree(path);
		}
	}

	object_handle_release(handle);
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * The opts parameter allows a string containing a list of comma-seperated
 * mount options to be passed. Some options are recognised by this function:
 *  - ro - Mount the filesystem read-only.
 * All other options are passed through to the filesystem implementation.
 * Mounting multiple filesystems on one directory at a time is not allowed.
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
status_t fs_mount(const char *device, const char *path, const char *type, const char *opts) {
	fs_mount_option_t *optarr;
	fs_mount_t *mount = NULL;
	fs_node_t *node = NULL;
	status_t ret;
	size_t count;
	int flags;

	if(!path || (!device && !type)) {
		return STATUS_INVALID_ARG;
	}

	if(!cap_check(NULL, CAP_FS_MOUNT)) {
		return STATUS_PERM_DENIED;
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
			fatal("Non-root mount before root filesystem mounted");
		}
	} else {
		/* Look up the destination directory. */
		ret = fs_node_lookup(path, true, FS_NODE_DIR, &node);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		/* Check that it is not being used as a mount point already. */
		if(node->mount->root == node) {
			ret = STATUS_IN_USE;
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

		ret = device_open(device, DEVICE_READ | DEVICE_WRITE, &mount->device);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Probe for the filesystem type if needed. */
	if(!type) {
		mount->type = fs_type_probe(mount->device, NULL);
		if(!mount->type) {
			ret = STATUS_UNKNOWN_FS;
			goto fail;
		}
	} else {
		/* Check if the device contains the type. */
		if(mount->type->probe && !mount->type->probe(mount->device, NULL)) {
			ret = STATUS_UNKNOWN_FS;
			goto fail;
		}
	}

	/* Allocate a mount ID. */
	if(next_mount_id == UINT16_MAX) {
		ret = STATUS_FS_FULL;
		goto fail;
	}
	mount->id = next_mount_id++;

	/* Call the filesystem's mount operation. */
	ret = mount->type->mount(mount, optarr, count);
	if(ret != STATUS_SUCCESS) {
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
	        (device) ? device : "<none>", path, mount, mount->root);
	mutex_unlock(&mounts_lock);
	free_mount_options(optarr, count);
	return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation.
 */
status_t fs_unmount(const char *path) {
	fs_node_t *node = NULL, *child;
	fs_mount_t *mount = NULL;
	status_t ret;

	if(!path) {
		return STATUS_INVALID_ARG;
	}

	if(!cap_check(NULL, CAP_FS_MOUNT)) {
		return STATUS_PERM_DENIED;
	}

	/* Serialise mount/unmount operations. */
	mutex_lock(&mounts_lock);

	/* Look up the destination directory and check if it can be unmounted. */
	ret = fs_node_lookup(path, true, FS_NODE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	} else if(node != node->mount->root) {
		ret = STATUS_NOT_MOUNT;
		goto fail;
	} else if(!node->mount->mountpoint) {
		ret = STATUS_IN_USE;
		goto fail;
	}

	/* Lock parent mount to ensure that the mount does not get looked up
	 * while we are unmounting. */
	mount = node->mount;
	mutex_lock(&mount->mountpoint->mount->lock);
	mutex_lock(&mount->lock);

	/* Get rid of the reference the lookup added, and check if any nodes
	 * on the mount are in use. */
	if(refcount_dec(&node->count) != 1) {
		assert(refcount_get(&node->count));
		ret = STATUS_IN_USE;
		goto fail;
	} else if(node->mount_link.next != &mount->used_nodes || node->mount_link.prev != &mount->used_nodes) {
		ret = STATUS_IN_USE;
		goto fail;
	}

	/* Flush and free all nodes in the unused list. */
	LIST_FOREACH_SAFE(&mount->unused_nodes, iter) {
		child = list_entry(iter, fs_node_t, mount_link);

		ret = fs_node_free(child);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Free the root node itself. */
	refcount_dec(&node->count);
	ret = fs_node_free(node);
	if(ret != STATUS_SUCCESS) {
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
	return STATUS_SUCCESS;
fail:
	if(node) {
		if(mount) {
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
 * @return		Status code describing result of the operation. */
status_t fs_info(const char *path, bool follow, fs_info_t *info) {
	fs_node_t *node;
	status_t ret;

	if(!path || !info) {
		return STATUS_INVALID_ARG;
	}

	ret = fs_node_lookup(path, follow, -1, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	fs_node_info(node, info);
	fs_node_release(node);
	return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation.
 */
status_t fs_unlink(const char *path) {
	fs_node_t *parent = NULL, *node = NULL;
	char *dir, *name;
	status_t ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	dprintf("fs: unlink(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	ret = fs_node_lookup(dir, true, FS_NODE_DIR, &parent);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}
	ret = fs_node_lookup(path, false, -1, &node);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Check whether the node can be unlinked. */
	if(parent->mount != node->mount) {
		ret = STATUS_IN_USE;
		goto out;
	} else if(!(object_rights(&parent->obj, NULL) & FS_WRITE)) {
		ret = STATUS_PERM_DENIED;
		goto out;
	} else if(FS_NODE_IS_RDONLY(node)) {
		ret = STATUS_READ_ONLY;
		goto out;
	} else if(!node->ops->unlink) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	ret = node->ops->unlink(parent, name, node);
out:
	if(node) {
		fs_node_release(node);
	}
	if(parent) {
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

		kprintf(LOG_NONE, "Count:   %d\n", refcount_get(&node->count));
		if(node->mount) {
			kprintf(LOG_NONE, "Mount:   %p (Locked: %d (%" PRId32 "))\n", node->mount,
			        atomic_get(&node->mount->lock.locked),
			        (node->mount->lock.holder) ? node->mount->lock.holder->id : -1);
		} else {
			kprintf(LOG_NONE, "Mount:   %p\n", node->mount);
		}
		kprintf(LOG_NONE, "Ops:     %p\n", node->ops);
		kprintf(LOG_NONE, "Data:    %p\n", node->data);
		kprintf(LOG_NONE, "Removed: %d\n", node->removed);
		kprintf(LOG_NONE, "Type:    %d\n", node->type);
		if(node->mounted) {
			kprintf(LOG_NONE, "Mounted: %p(%" PRIu16 ")\n", node->mounted,
			        node->mounted->id);
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

		kprintf(LOG_NONE, "ID       Count Removed Type Ops                Data               Mount\n");
		kprintf(LOG_NONE, "==       ===== ======= ==== ===                ====               =====\n");

		if(list) {
			LIST_FOREACH(list, iter) {
				node = list_entry(iter, fs_node_t, mount_link);
				kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-7d %-4d %-18p %-18p %p\n",
				        node->id, refcount_get(&node->count), node->removed,
				        node->type, node->ops, node->data, node->mount);
			}
		} else {
			AVL_TREE_FOREACH(&mount->nodes, iter) {
				node = avl_tree_entry(iter, fs_node_t);
				kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-7d %-4d %-18p %-18p %p\n",
				        node->id, refcount_get(&node->count), node->removed,
				        node->type, node->ops, node->data, node->mount);
			}
		}
	}

	return KDBG_OK;
}

/** Initialise the filesystem layer.
 * @param args		Kernel arguments structure. */
void __init_text fs_init(kernel_args_t *args) {
	fs_node_cache = slab_cache_create("fs_node_cache", sizeof(fs_node_t), 0,
	                                  NULL, NULL, NULL, NULL, 0, MM_FATAL);

	/* Register the low resource handler. */
	lrm_handler_register(&fs_lrm_handler);

	/* Store the boot FS UUID for use by fs_probe(). */
	boot_fs_uuid = args->boot_fs_uuid;
	force_fsimage = args->force_fsimage;
}

/** Create a regular file in the file system.
 * @param path		Path to file to create.
 * @return		Status code describing result of the operation. */
status_t sys_fs_file_create(const char *path) {
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_file_create(kpath);
	kfree(kpath);
	return ret;
}

/** Open a handle to a file.
 * @param path		Path to file to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store handle to file.
 * @return		Status code describing result of the operation. */
status_t sys_fs_file_open(const char *path, object_rights_t rights, int flags, handle_t *handlep) {
	object_handle_t *handle;
	char *kpath = NULL;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_file_open(kpath, rights, flags, &handle);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(handle, NULL, 0, NULL, handlep);
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
 * @param handle	Handle to file to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	size_t bytes = 0;
	void *kbuf;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to read. */
	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = fs_file_read(khandle, kbuf, count, &bytes);
	if(bytes) {
		err = memcpy_to_user(buf, kbuf, bytes);
		if(err != STATUS_SUCCESS) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
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
 * @param handle	Handle to file to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset into file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_file_pread(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	size_t bytes = 0;
	void *kbuf;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to read. */
	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = fs_file_pread(khandle, kbuf, count, offset, &bytes);
	if(bytes) {
		err = memcpy_to_user(buf, kbuf, bytes);
		if(err != STATUS_SUCCESS) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
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
 * @param handle	Handle to file to write to. Must have the FS_WRITE
 *			access right.
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
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	void *kbuf = NULL;
	size_t bytes = 0;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to write. */
	if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}
	ret = memcpy_from_user(kbuf, buf, count);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Perform the actual write. */
	ret = fs_file_write(khandle, kbuf, count, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
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
 * @param handle	Handle to file to write to. Must have the FS_WRITE
 *			access right.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments.
 * @param offset	Offset into file to write at.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_file_pwrite(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	void *kbuf = NULL;
	size_t bytes = 0;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to write. */
	if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}
	ret = memcpy_from_user(kbuf, buf, count);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Perform the actual write. */
	ret = fs_file_pwrite(khandle, kbuf, count, offset, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
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
 * @param handle	Handle to file to resize. Must have the FS_WRITE access
 *			right.
 * @param size		New size of the file.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_file_resize(handle_t handle, offset_t size) {
	object_handle_t *khandle;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_file_resize(khandle, size);
	object_handle_release(khandle);
	return ret;
}

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @return		Status code describing result of the operation. */
status_t sys_fs_dir_create(const char *path) {
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_dir_create(kpath);
	kfree(kpath);
	return ret;
}

/** Open a handle to a directory.
 * @param path		Path to file to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store handle to directory.
 * @return		Status code describing result of the operation. */
status_t sys_fs_dir_open(const char *path, object_rights_t rights, int flags, handle_t *handlep) {
	object_handle_t *handle;
	char *kpath = NULL;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_dir_open(kpath, rights, flags, &handle);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(handle, NULL, 0, NULL, handlep);
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
 * @param handle	Handle to directory to read from. Must have the FS_READ
 *			access right.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, the function will
 *			return STATUS_TOO_SMALL).
 *
 * @return		Status code describing result of the operation. If the
 *			handle's offset is past the end of the directory,
 *			STATUS_NOT_FOUND will be returned.
 */
status_t sys_fs_dir_read(handle_t handle, fs_dir_entry_t *buf, size_t size) {
	object_handle_t *khandle;
	fs_dir_entry_t *kbuf;
	status_t ret;

	if(!size) {
		return STATUS_TOO_SMALL;
	}

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_DIR, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(size, 0);
	if(!kbuf) {
		object_handle_release(khandle);
		return STATUS_NO_MEMORY;
	}

	/* Perform the actual read. */
	ret = fs_dir_read(khandle, kbuf, size);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(buf, kbuf, kbuf->length);
	}

	kfree(kbuf);
	object_handle_release(khandle);
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
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_handle_seek(handle_t handle, int action, rel_offset_t offset, offset_t *newp) {
	object_handle_t *khandle;
	status_t ret;
	offset_t new;

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_handle_seek(khandle, action, offset, &new);
	if(ret == STATUS_SUCCESS && newp) {
		ret = memcpy_to_user(newp, &new, sizeof(offset_t));
	}
	object_handle_release(khandle);
	return ret;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information on.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t sys_fs_handle_info(handle_t handle, fs_info_t *info) {
	object_handle_t *khandle;
	fs_info_t kinfo;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_handle_info(khandle, &kinfo);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(info, &kinfo, sizeof(fs_info_t));
	}
	object_handle_release(khandle);
	return ret;
}

/** Flush changes to a filesystem node to the FS.
 * @param handle	Handle to node to flush.
 * @return		Status code describing result of the operation. */
status_t sys_fs_handle_sync(handle_t handle) {
	object_handle_t *khandle;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_handle_sync(khandle);
	object_handle_release(khandle);
	return ret;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
status_t sys_fs_symlink_create(const char *path, const char *target) {
	char *kpath, *ktarget;
	status_t ret;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = strndup_from_user(target, FS_PATH_MAX, &ktarget);
	if(ret != STATUS_SUCCESS) {
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
 * always be placed at the end of the string.
 *
 * @param path		Path to the symbolic link to read.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer. If this is too small, the function will
 *			return STATUS_TOO_SMALL.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_symlink_read(const char *path, char *buf, size_t size) {
	char *kpath, *kbuf;
	status_t ret;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Allocate a buffer to read into. See comment in sys_fs_file_read()
	 * about not using MM_SLEEP. */
	kbuf = kmalloc(size, 0);
	if(!kbuf) {
		kfree(kpath);
		return STATUS_NO_MEMORY;
	}

	ret = fs_symlink_read(kpath, kbuf, size);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(buf, kbuf, size);
	}

	kfree(kpath);
	kfree(kbuf);
	return ret;
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * The opts parameter allows a string containing a list of comma-seperated
 * mount options to be passed. Some options are recognised by this function:
 *  - ro - Mount the filesystem read-only.
 * All other options are passed through to the filesystem implementation.
 * Mounting multiple filesystems on one directory at a time is not allowed.
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
status_t sys_fs_mount(const char *dev, const char *path, const char *type, const char *opts) {
	char *kdevice = NULL, *kpath = NULL, *ktype = NULL, *kopts = NULL;
	status_t ret;

	/* Copy string arguments across from userspace. */
	if(dev) {
		ret = strndup_from_user(dev, FS_PATH_MAX, &kdevice);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}
	if(type) {
		ret = strndup_from_user(type, FS_PATH_MAX, &ktype);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
	if(opts) {
		ret = strndup_from_user(opts, FS_PATH_MAX, &kopts);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	ret = fs_mount(kdevice, kpath, ktype, kopts);
out:
	if(kdevice) { kfree(kdevice); }
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
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_unmount(const char *path) {
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_unmount(kpath);
	kfree(kpath);
	return ret;
}

/** Flush all cached filesystem changes. */
status_t sys_fs_sync(void) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to store in.
 * @param size		Size of buffer.
 * @return		Status code describing result of the operation. */
status_t sys_fs_getcwd(char *buf, size_t size) {
	char *kbuf = NULL, *tmp, *name, path[3];
	fs_node_t *node;
	size_t len = 0;
	node_id_t id;
	status_t ret;

	if(!buf || !size) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&curr_proc->ioctx.lock);

	/* Get the working directory. */
	node = curr_proc->ioctx.curr_dir;
	fs_node_get(node);

	/* Loop through until we reach the root. */
	while(node != curr_proc->ioctx.root_dir) {
		/* Save the current node's ID. Use the mountpoint ID if this is
		 * the root of the mount. */
		id = (node == node->mount->root) ? node->mount->mountpoint->id : node->id;

		/* Get the parent of the node. */
		strcpy(path, "..");
		ret = fs_node_lookup_internal(path, node, false, 0, &node);
		if(ret != STATUS_SUCCESS) {
			node = NULL;
			goto fail;
		} else if(node->type != FS_NODE_DIR) {
			kprintf(LOG_WARN, "fs: node %" PRIu16 ":%" PRIu64 " should be a directory but it isn't!\n",
			        node->mount->id, node->id);
			ret = STATUS_NOT_DIR;
			goto fail;
		}

		/* Look up the name of the child in this directory. */
		ret = fs_node_name(node, id, &name);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		/* Add the entry name on to the beginning of the path. */
		len += ((kbuf) ? strlen(name) + 1 : strlen(name));
		tmp = kmalloc(len + 1, MM_SLEEP);
		strcpy(tmp, name);
		kfree(name);
		if(kbuf) {
			strcat(tmp, "/");
			strcat(tmp, kbuf);
			kfree(kbuf);
		}
		kbuf = tmp;
	}

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
		ret = STATUS_TOO_SMALL;
	} else {
		ret = memcpy_to_user(buf, kbuf, len + 1);
	}
	kfree(kbuf);
	return ret;
fail:
	if(node) {
		fs_node_release(node);
	}
	rwlock_unlock(&curr_proc->ioctx.lock);
	kfree(kbuf);
	return ret;
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		Status code describing result of the operation. */
status_t sys_fs_setcwd(const char *path) {
	fs_node_t *node;
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_node_lookup(kpath, true, FS_NODE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->obj, NULL) & FS_WRITE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_PERM_DENIED;
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
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_setroot(const char *path) {
	fs_node_t *node;
	status_t ret;
	char *kpath;

	if(!cap_check(NULL, CAP_FS_SETROOT)) {
		return STATUS_PERM_DENIED;
	}

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_node_lookup(kpath, true, FS_NODE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->obj, NULL) & FS_WRITE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_PERM_DENIED;
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
 * @return		Status code describing result of the operation. */
status_t sys_fs_info(const char *path, bool follow, fs_info_t *info) {
	fs_info_t kinfo;
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_info(kpath, follow, &kinfo);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(info, &kinfo, sizeof(fs_info_t));
	}
	kfree(kpath);
	return ret;
}

status_t sys_fs_link(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
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
 * @return		Status code describing result of the operation.
 */
status_t sys_fs_unlink(const char *path) {
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_unlink(kpath);
	kfree(kpath);
	return ret;
}

status_t sys_fs_rename(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
}
