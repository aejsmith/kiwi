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
 * @brief		Virtual file system (VFS).
 *
 * @note		Mount locks should be taken before node locks. If a
 *			node lock is held and it is desired to lock its mount,
 *			you should unlock the node, lock the mount, then relock
 *			the node. If the node lock is taken first, a deadlock
 *			can occur (lock node, attempt to lock mount which
 *			blocks because node is being searched for, search
 *			attempts to lock node, deadlock).
 *
 * @todo		This needs a major cleanup, and should be split up into
 *			multiple files.
 * @todo		Implement FS_HANDLE_NONBLOCK.
 * @todo		Could probably use an rwlock on nodes.
 */

#include <io/context.h>
#include <io/device.h>
#include <io/vfs.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <sync/rwlock.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kargs.h>
#include <kdbg.h>

#if CONFIG_VFS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Data for a VFS handle (both handle types need the same data). */
typedef struct vfs_handle {
	rwlock_t lock;			/**< Lock to protect offset. */
	offset_t offset;		/**< Current file offset. */
	int flags;			/**< Flags the file was opened with. */
} vfs_handle_t;

extern vfs_type_t ramfs_fs_type;

/** Pointer to mount at root of the filesystem. */
vfs_mount_t *vfs_root_mount = NULL;

/** List of all mounts. */
static mount_id_t vfs_next_mount_id = 0;
static LIST_DECLARE(vfs_mount_list);
static MUTEX_DECLARE(vfs_mount_lock, 0);

/** List of registered FS types. */
static LIST_DECLARE(vfs_type_list);
static MUTEX_DECLARE(vfs_type_list_lock, 0);

/** Filesystem node slab cache. */
static slab_cache_t *vfs_node_cache;

static object_type_t vfs_file_object_type;
static object_type_t vfs_dir_object_type;

static int vfs_node_free(vfs_node_t *node);
static int vfs_file_page_flush(vfs_node_t *node, vm_page_t *page);
static int vfs_dir_entry_get(vfs_node_t *node, const char *name, node_id_t *idp);
static int vfs_symlink_cache_dest(vfs_node_t *node);

/** Look up a filesystem type with lock already held.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static vfs_type_t *vfs_type_lookup_internal(const char *name) {
	vfs_type_t *type;

	LIST_FOREACH(&vfs_type_list, iter) {
		type = list_entry(iter, vfs_type_t, header);

		if(strcmp(type->name, name) == 0) {
			return type;
		}
	}

	return NULL;
}

/** Look up a filesystem type and reference it.
 * @param name		Name of filesystem type to look up.
 * @return		Pointer to type structure if found, NULL if not. */
static vfs_type_t *vfs_type_lookup(const char *name) {
	vfs_type_t *type;

	mutex_lock(&vfs_type_list_lock);

	type = vfs_type_lookup_internal(name);
	if(type) {
		refcount_inc(&type->count);
	}

	mutex_unlock(&vfs_type_list_lock);
	return type;
}

/** Determine which filesystem type a device contains.
 * @param handle	Handle to device to probe.
 * @return		Pointer to type structure, or NULL if filesystem type
 *			not recognized. If found, type will be referenced. */
static vfs_type_t *vfs_type_probe(object_handle_t *handle) {
	vfs_type_t *type;

	mutex_lock(&vfs_type_list_lock);

	LIST_FOREACH(&vfs_type_list, iter) {
		type = list_entry(iter, vfs_type_t, header);

		if(!type->probe) {
			continue;
		} else if(type->probe(handle)) {
			refcount_inc(&type->count);
			mutex_unlock(&vfs_type_list_lock);
			return type;
		}
	}

	mutex_unlock(&vfs_type_list_lock);
	return NULL;
}

/** Register a new filesystem type.
 * @param type		Pointer to type structure to register.
 * @return		0 on success, negative error code on failure. */
int vfs_type_register(vfs_type_t *type) {
	/* Check for required operations. */
	if(!type->mount) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&vfs_type_list_lock);

	/* Check if this type already exists. */
	if(vfs_type_lookup_internal(type->name) != NULL) {
		mutex_unlock(&vfs_type_list_lock);
		return -ERR_ALREADY_EXISTS;
	}

	list_init(&type->header);
	list_append(&vfs_type_list, &type->header);

	kprintf(LOG_NORMAL, "vfs: registered filesystem type %p(%s)\n", type, type->name);
	mutex_unlock(&vfs_type_list_lock);
	return 0;
}

/** Remove a filesystem type.
 *
 * Removes a previously registered filesystem type from the list of
 * filesystem types. Will not succeed if the filesystem type is in use by any
 * mounts.
 *
 * @param type		Type to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_type_unregister(vfs_type_t *type) {
	mutex_lock(&vfs_type_list_lock);

	/* Check that the type is actually there. */
	if(vfs_type_lookup_internal(type->name) != type) {
		mutex_unlock(&vfs_type_list_lock);
		return -ERR_NOT_FOUND;
	} else if(refcount_get(&type->count) > 0) {
		mutex_unlock(&vfs_type_list_lock);
		return -ERR_IN_USE;
	}

	list_remove(&type->header);
	mutex_unlock(&vfs_type_list_lock);
	return 0;
}

/** VFS node object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_cache_ctor(void *obj, void *data, int kmflag) {
	vfs_node_t *node = (vfs_node_t *)obj;

	list_init(&node->mount_link);
	mutex_init(&node->lock, "vfs_node_lock", 0);
	refcount_set(&node->count, 0);
	avl_tree_init(&node->pages);
	radix_tree_init(&node->dir_entries);
	return 0;
}

/** VFS node reclaim callback.
 * @note		This could be better.
 * @param data		Cache data (unused).
 * @param force		If true, will reclaim all unused nodes. */
static void vfs_node_cache_reclaim(void *data, bool force) {
	vfs_mount_t *mount;
	vfs_node_t *node;
	size_t count;

	mutex_lock(&vfs_mount_lock);

	/* Iterate through mounts until we can flush at least 2 slabs worth of
	 * node structures, or if forcing, free everything unused. */
	count = (vfs_node_cache->slab_size / vfs_node_cache->obj_size) * 2;
	assert(count);
	LIST_FOREACH(&vfs_mount_list, iter) {
		mount = list_entry(iter, vfs_mount_t, header);

		if(mount->type->flags & VFS_TYPE_CACHE_BASED) {
			continue;
		}

		mutex_lock(&mount->lock);

		LIST_FOREACH_SAFE(&mount->unused_nodes, niter) {
			node = list_entry(niter, vfs_node_t, mount_link);

			/* On success, node is unlocked by vfs_node_free(). */
			mutex_lock(&node->lock);
			if(vfs_node_free(node) != 0) {
				mutex_unlock(&node->lock);
			} else if(--count == 0 && !force) {
				mutex_unlock(&mount->lock);
				mutex_unlock(&vfs_mount_lock);
				return;
			}
		}

		mutex_unlock(&mount->lock);
	}

	mutex_unlock(&vfs_mount_lock);
}

/** Allocate a node structure and set one reference on it.
 * @note		Does not attach to the mount.
 * @param mount		Mount that the node resides on.
 * @param type		Type to give the node.
 * @return		Pointer to node structure allocated. */
vfs_node_t *vfs_node_alloc(vfs_mount_t *mount, vfs_node_type_t type) {
	vfs_node_t *node;

	node = slab_cache_alloc(vfs_node_cache, MM_SLEEP);
	refcount_set(&node->count, 1);
	node->id = 0;
	node->mount = mount;
	node->flags = 0;
	node->type = type;
	node->size = 0;
	node->entry_count = 0;
	node->link_dest = NULL;
	node->mounted = NULL;

	/* Initialise the node's object header. */
	switch(type) {
	case VFS_NODE_FILE:
		object_init(&node->obj, &vfs_file_object_type);
		break;
	case VFS_NODE_DIR:
		object_init(&node->obj, &vfs_dir_object_type);
		break;
	default:
		object_init(&node->obj, NULL);
		break;
	}

	return node;
}

/** Flush all changes to a node.
 * @param node		Node to free. Both the node and its mount should be
 *			locked.
 * @param destroy	Whether to remove cached pages from the cache after
 *			flushing. If any pages are still in use when this is
 *			specified, fatal() will be called.
 * @return		0 on success, negative error code on failure. If a
 *			failure occurs while flushing page data when destroying
 *			an error is returned immediately. Otherwise, it
 *			carries on attempting to flush other pages, but still
 *			returns an error. If multiple errors occur, it is the
 *			most recent that is returned. */
static int vfs_node_flush(vfs_node_t *node, bool destroy) {
	int ret = 0, err;
	vm_page_t *page;

	if(node->type == VFS_NODE_FILE) {
		AVL_TREE_FOREACH_SAFE(&node->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			/* Check reference count. If destroying, shouldn't be used. */
			if(destroy && refcount_get(&page->count) != 0) {
				fatal("Node page still in use while destroying");
			}

			/* Flush the page data. See function documentation
			 * about how failure is handled. */
			if((err = vfs_file_page_flush(node, page)) != 0) {
				if(destroy) {
					return err;
				}
				ret = err;
			}

			/* Destroy the page if required. */
			if(destroy) {
				avl_tree_remove(&node->pages, (key_t)page->offset);
				vm_page_free(page, 1);
			}
		}
	}

	/* Flush node metadata. */
	if(!VFS_NODE_IS_RDONLY(node) && node->mount && node->mount->type->node_flush) {
		if((err = node->mount->type->node_flush(node)) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Flush changes to a node and free it.
 * @note		Never call this function. Use vfs_node_release().
 * @note		Mount lock (if there is a mount) and node lock must be
 *			held. Mount lock will still be locked when the function
 *			returns.
 * @param node		Node to free. Should be unused (zero reference count).
 * @return		0 on success, negative error code on failure (this can
 *			happen, for example, if an error occurs flushing the
 *			node data). */
static int vfs_node_free(vfs_node_t *node) {
	int ret;

	assert(refcount_get(&node->count) == 0);

	/* Flush cached data and metadata. */
	if((ret = vfs_node_flush(node, true)) != 0) {
		kprintf(LOG_WARN, "vfs: warning: failed to flush data for %p(%" PRIu16 ":%" PRIu64 ") (%d)\n",
		        node, (node->mount) ? node->mount->id : -1, node->id, ret);
		mutex_unlock(&node->lock);
		if(node->mount) {
			mutex_unlock(&node->mount->lock);
		}
		return ret;
	}

	/* If the node has a mount, detach it from the node tree/lists and call
	 * the mount's node_free operation (if any). */
	if(node->mount) {
		avl_tree_remove(&node->mount->nodes, (key_t)node->id);
		list_remove(&node->mount_link);
		if(node->mount->type->node_free) {
			node->mount->type->node_free(node);
		}
	}

	/* Free up other bits of data.*/
	radix_tree_clear(&node->dir_entries, kfree);
	if(node->link_dest) {
		kfree(node->link_dest);
	}
	object_destroy(&node->obj);

	dprintf("vfs: freed node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
	        (node->mount) ? node->mount->id : -1, node->id);
	mutex_unlock(&node->lock);
	slab_cache_free(vfs_node_cache, node);
	return 0;
}

/** Look up a node in the filesystem.
 * @param path		Path string to look up.
 * @param node		Node to begin lookup at (locked and referenced).
 *			Does not have to be set if path is absolute.
 * @param follow	Whether to follow last path component if it is a
 *			symbolic link.
 * @param nest		Symbolic link nesting count.
 * @param nodep		Where to store pointer to node found (referenced and
 *			locked).
 * @return		0 on success, negative error code on failure. */
static int vfs_node_lookup_internal(char *path, vfs_node_t *node, bool follow, int nest, vfs_node_t **nodep) {
	vfs_node_t *prev = NULL, *tmp;
	vfs_mount_t *mount;
	char *tok, *link;
	node_id_t id;
	int ret;

	/* Handle absolute paths here rather than in vfs_node_lookup() because
	 * the symbolic link resolution code below calls this function directly
	 * rather than vfs_node_lookup(). */
	if(path[0] == '/') {
		/* Drop the node we were provided, if any. */
		if(node != NULL) {
			mutex_unlock(&node->lock);
			vfs_node_release(node);
		}

		/* Strip off all '/' characters at the start of the path. */
		while(path[0] == '/') {
                        path++;
                }

		assert(curr_proc->ioctx.root_dir);

		node = curr_proc->ioctx.root_dir;
		mutex_lock(&node->lock);
		vfs_node_get(node);

		/* If we have already reached the end of the path string,
		 * return the root node. */
		if(!path[0]) {
			*nodep = node;
			return 0;
		}
	}

	assert(node->type == VFS_NODE_DIR);

	/* Loop through each element of the path string. */
	while(true) {
		tok = strsep(&path, "/");

		/* If the node is a symlink and this is not the last element
		 * of the path, or the caller wishes to follow the link, follow
		 * it. */
		if(node->type == VFS_NODE_SYMLINK && (tok || follow)) {
			/* The previous node should be the link's parent. */
			assert(prev);
			assert(prev->type == VFS_NODE_DIR);

			/* Check whether we have exceeded the maximum nesting
			 * count (TODO: This should be in limits.h). */
			if(++nest > 16) {
				mutex_unlock(&node->lock);
				vfs_node_release(prev);
				vfs_node_release(node);
				return -ERR_LINK_LIMIT;
			}

			/* Ensure that the link destination is cached. */
			if((ret = vfs_symlink_cache_dest(node)) != 0) {
				mutex_unlock(&node->lock);
				vfs_node_release(prev);
				vfs_node_release(node);
				return ret;
			}

			dprintf("vfs: following symbolic link %" PRIu16 ":%" PRIu64 " to %s\n",
			        node->mount->id, node->id, node->link_dest);

			/* Duplicate the link destination as the lookup needs
			 * to modify it. */
			link = kstrdup(node->link_dest, MM_SLEEP);

			/* Move up to the parent node. The previous iteration
			 * of the loop left a reference on previous for us. */
			tmp = node; node = prev; prev = tmp;
			mutex_unlock(&prev->lock);
			mutex_lock(&node->lock);

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. */
			if((ret = vfs_node_lookup_internal(link, node, true, nest, &node)) != 0) {
				vfs_node_release(prev);
				kfree(link);
				return ret;
			}

			dprintf("vfs: followed %s to %" PRIu16 ":%" PRIu64 "\n",
			        prev->link_dest, node->mount->id, node->id);
			kfree(link);

			mutex_unlock(&node->lock);
			vfs_node_release(prev);
			mutex_lock(&node->lock);
		} else if(node->type == VFS_NODE_SYMLINK) {
			/* The new node is a symbolic link but we do not want
			 * to follow it. We must release the previous node. */
			assert(prev != node);
			mutex_unlock(&node->lock);
			vfs_node_release(prev);
			mutex_lock(&node->lock);
		}

		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			*nodep = node;
			return 0;
		} else if(node->type != VFS_NODE_DIR) {
			/* The previous token was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			mutex_unlock(&node->lock);
			vfs_node_release(node);
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

			assert(node != vfs_root_mount->root);

			if(node == node->mount->root) {
				assert(node->mount->mountpoint);
				assert(node->mount->mountpoint->type == VFS_NODE_DIR);

				/* We're at the root of the mount, and the path
				 * wants to move to the parent. Using the '..'
				 * directory entry in the filesystem won't work
				 * in this case. Switch node to point to the
				 * mountpoint of the mount and then go through
				 * the normal lookup mechanism to get the '..'
				 * entry of the mountpoint. It is safe to use
				 * vfs_node_get() here - mountpoints will
				 * always have at least one reference. */
				prev = node;
				node = prev->mount->mountpoint;
				vfs_node_get(node);
				mutex_unlock(&prev->lock);
				vfs_node_release(prev);
				mutex_lock(&node->lock);
			}
		}

		/* Look up this name within the directory entry cache. */
		if((ret = vfs_dir_entry_get(node, tok, &id)) != 0) {
			mutex_unlock(&node->lock);
			vfs_node_release(node);
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

		/* Check if the node is cached in the mount. */
		dprintf("vfs: looking for node %" PRIu64 " in cache for mount %" PRIu16 " (%s)\n",
		        id, mount->id, tok);
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
			 * cache from the filesystem. Check that the filesystem
			 * has a node_get operation. */
			if(!mount->type->node_get) {
				mutex_unlock(&mount->lock);
				vfs_node_release(prev);
				return -ERR_NOT_SUPPORTED;
			}

			/* Request the node from the filesystem. */
			if((ret = mount->type->node_get(mount, id, &node)) != 0) {
				mutex_unlock(&mount->lock);
				vfs_node_release(prev);
				return ret;
			}

			/* Attach the node to the node tree and used list. */
			avl_tree_insert(&mount->nodes, (key_t)id, node, NULL);
			list_append(&mount->used_nodes, &node->mount_link);
			mutex_unlock(&mount->lock);
		}

		/* Do not release the previous node if the current node is a
		 * symbolic link, as the symbolic link code requires it. */
		if(node->type != VFS_NODE_SYMLINK) {
			vfs_node_release(prev);
		}

		/* Lock the new node. */
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
 * @todo		We currently hold the I/O context lock across the
 *			entire lookup to prevent another thread from messing
 *			with the context's root directory while the lookup
 *			is being performed. This could possibly be done in a
 *			better way.
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
static int vfs_node_lookup(const char *path, bool follow, int type, vfs_node_t **nodep) {
	vfs_node_t *node = NULL;
	char *dup;
	int ret;

	if(!path || !path[0] || !nodep) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&curr_proc->ioctx.lock);

	/* Start from the current directory if the path is relative. */
	if(path[0] != '/') {
		assert(curr_proc->ioctx.curr_dir);

		node = curr_proc->ioctx.curr_dir;
		mutex_lock(&node->lock);
		vfs_node_get(node);
	}

	/* Duplicate path so that vfs_node_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_SLEEP);

	/* Look up the path string. */
	if((ret = vfs_node_lookup_internal(dup, node, follow, 0, &node)) == 0) {
		if(type >= 0 && node->type != (unsigned int)type) {
			ret = -ERR_TYPE_INVAL;
			mutex_unlock(&node->lock);
			vfs_node_release(node);
		} else {
			*nodep = node;
			mutex_unlock(&node->lock);
		}
	}

	mutex_unlock(&curr_proc->ioctx.lock);
	kfree(dup);
	return ret;
}

/** Increase the reference count of a node.
 * @note		Should not be used on nodes with a zero reference
 *			count.
 * @param node		Node to increase reference count of. */
void vfs_node_get(vfs_node_t *node) {
	int val = refcount_inc(&node->count);

	if(val == 1) {
		fatal("Called vfs_node_get on unused node %" PRIu16 ":%" PRIu64,
		      (node->mount) ? node->mount->id : -1, node->id);
	}
}

/** Decrease the reference count of a node.
 *
 * Decreases the reference count of a filesystem node. If this causes the
 * node's count to become zero, then the node will be moved on to the mount's
 * unused node list. This function should be called when a node obtained via
 * vfs_node_lookup() or referenced via vfs_node_get() is no longer required;
 * each call to those functions should be matched with a call to this function.
 *
 * @param node		Node to decrease reference count of.
 */
void vfs_node_release(vfs_node_t *node) {
	vfs_mount_t *mount = NULL;
	int ret;

	/* Acquire mount lock then node lock. See note in file header about
	 * locking order. */
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
		if(mount && !(node->flags & VFS_NODE_REMOVED) && !list_empty(&node->mount_link)) {
			list_append(&node->mount->unused_nodes, &node->mount_link);
			dprintf("vfs: transferred node %p to unused list (mount: %p)\n", node, node->mount);
			mutex_unlock(&node->lock);
			mutex_unlock(&mount->lock);
		} else {
			/* This shouldn't fail - the only things that can fail
			 * in vfs_node_free() are cache flushing and metadata
			 * flushing. Since this node has no source to flush to,
			 * or has been removed, this should not fail. */
			if((ret = vfs_node_free(node)) != 0) {
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

/** Common node creation code.
 * @param path		Path to node to create.
 * @param node		Node structure describing the node to be created.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_create(const char *path, vfs_node_t *node) {
	vfs_node_t *parent = NULL;
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

	dprintf("vfs: create(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                ret = -ERR_ALREADY_EXISTS;
		goto out;
        }

	/* Look up the parent node. */
	if((ret = vfs_node_lookup(dir, true, VFS_NODE_DIR, &parent)) != 0) {
		goto out;
	}

	mutex_lock(&parent->mount->lock);
	mutex_lock(&parent->lock);

	/* Ensure that we are on a writable filesystem, and that the FS
	 * supports node creation. */
	if(VFS_NODE_IS_RDONLY(parent)) {
		ret = -ERR_READ_ONLY;
		goto out;
	} else if(!parent->mount->type->node_create) {
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	}

	/* Check if the name we're creating already exists. This will populate
	 * the entry cache so it will be OK to add the node to it. */
	if((ret = vfs_dir_entry_get(parent, name, &id)) != -ERR_NOT_FOUND) {
		if(ret == 0) {
			ret = -ERR_ALREADY_EXISTS;
		}
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	node->mount = parent->mount;
	if((ret = node->mount->type->node_create(parent, name, node)) != 0) {
		goto out;
	}

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&node->mount->nodes, (key_t)node->id, node, NULL);
	list_append(&node->mount->used_nodes, &node->mount_link);

	/* Insert the node into the parent's entry cache. */
	vfs_dir_entry_add(parent, node->id, name);

	dprintf("vfs: created %s (node: %" PRIu16 ":%" PRIu64 ", parent: %" PRIu16 ":%" PRIu64 ")\n",
	        path, node->mount->id, node->id, parent->mount->id, parent->id);
	ret = 0;
out:
	if(parent) {
		mutex_unlock(&parent->lock);
		mutex_unlock(&parent->mount->lock);
		vfs_node_release(parent);
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
 * @param node		Node to get information for.
 * @param info		Structure to store information in. */
static void vfs_node_info(vfs_node_t *node, fs_info_t *info) {
	mutex_lock(&node->lock);

	/* Fill in default values for everything. */
	memset(info, 0, sizeof(fs_info_t));
	info->id = node->id;
	info->mount = (node->mount) ? node->mount->id : -1;
	info->blksize = PAGE_SIZE;
	info->size = node->size;
	info->links = 1;

	if(node->mount && node->mount->type->node_info) {
		node->mount->type->node_info(node, info);
	}

	mutex_unlock(&node->lock);
}

/** Create a handle to a node.
 * @param node		Node to create handle to. Will have an extra reference
 *			added to it.
 * @param flags		Flags for the handle.
 * @return		Pointer to handle structure. */
static object_handle_t *vfs_handle_create(vfs_node_t *node, int flags) {
	object_handle_t *handle;
	vfs_handle_t *data;

	/* Allocate the per-handle data structure. */
	data = kmalloc(sizeof(vfs_handle_t), MM_SLEEP);
	rwlock_init(&data->lock, "vfs_handle_lock");
	data->offset = 0;
	data->flags = flags;

	/* Create the handle. */
	vfs_node_get(node);
	handle = object_handle_create(&node->obj, data);
	dprintf("vfs: opened handle %p to node %p (data: %p)\n", handle, node, data);
	return handle;
}

/** Get a page from a file's cache.
 * @note		Should not be passed both mappingp and pagep.
 * @param node		Node to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in from
 *			the filesystem if it is not in the cache, it will only
 *			allocate a page - useful if the caller is about to
 *			overwrite the page data.
 * @param pagep		Where to store pointer to page structure.
 * @param mappingp	Where to store address of virtual mapping. If this is
 *			set the calling thread will be wired to its CPU when
 *			the function returns.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared. Only used if mappingp is set.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_page_get_internal(vfs_node_t *node, offset_t offset, bool overwrite,
                                      vm_page_t **pagep, void **mappingp, bool *sharedp) {
	void *mapping = NULL;
	vm_page_t *page;
	int ret;

	assert(node->type == VFS_NODE_FILE);
	assert((pagep && !mappingp) || (mappingp && !pagep));

	mutex_lock(&node->lock);

	/* Check whether it is within the size of the node. */
	if((file_size_t)offset >= node->size) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_FOUND;
	}

	/* Check if we have it cached. */
	if((page = avl_tree_lookup(&node->pages, (key_t)offset))) {
		refcount_inc(&page->count);
		mutex_unlock(&node->lock);

		/* Map it in if required. Wire the thread to the current CPU
		 * and specify that the mapping is not being shared - the
		 * mapping will only be accessed by this thread, so we can
		 * save having to do an expensive remote TLB invalidation. */
		if(mappingp) {
			assert(sharedp);

			thread_wire(curr_thread);
			*mappingp = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
			*sharedp = false;
		} else {
			*pagep = page;
		}

		dprintf("vfs: retreived cached page 0x%" PRIpp " from offset %" PRId64 " in %p\n",
		        page->addr, offset, node);
		return 0;
	}

	/* Need to read the page in if not about to completely overwrite it. */
	if(!overwrite) {
		/* If a read operation is provided, read the page data into an
		 * unzeroed page. Otherwise get a zeroed page. */
		if(node->mount && node->mount->type->page_read) {
			page = vm_page_alloc(1, MM_SLEEP);

			/* When reading in page data we cannot guarantee that
			 * the mapping won't be shared, because it's possible
			 * that a device driver will do work in another thread,
			 * which may be on another CPU. */
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);

			if((ret = node->mount->type->page_read(node, mapping, offset, false)) != 0) {
				page_phys_unmap(mapping, PAGE_SIZE, true);
				vm_page_free(page, 1);
				mutex_unlock(&node->lock);
				return ret;
			}
		} else {
			page = vm_page_alloc(1, MM_SLEEP | PM_ZERO);
		}
	} else {
		/* Overwriting - allocate a new page, don't have to zero. */
		page = vm_page_alloc(1, MM_SLEEP);
	}

	/* Cache the page and unlock. */
	refcount_inc(&page->count);
	page->offset = offset;
	avl_tree_insert(&node->pages, (key_t)offset, page, NULL);
	mutex_unlock(&node->lock);

	dprintf("vfs: cached new page 0x%" PRIpp " at offset %" PRId64 " in %p\n",
	        page->addr, offset, node);

	if(mappingp) {
		/* If we had to read page data in, reuse the mapping created,
		 * and specify that it may be shared across CPUs (see comment
		 * above). Otherwise wire the thread and specify that it won't
		 * be shared. */
		assert(sharedp);
		if(!mapping) {
			thread_wire(curr_thread);
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
			*sharedp = false;
		} else {
			*sharedp = true;
		}
		*mappingp = mapping;
	} else {
		/* Page mapping is not required, get rid of it. */
		if(mapping) {
			page_phys_unmap(mapping, PAGE_SIZE, true);
		}
		*pagep = page;
	}
	return 0;
}

/** Release a page from a file.
 * @param node		Node that the page belongs to.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied. */
static void vfs_file_page_release_internal(vfs_node_t *node, offset_t offset, bool dirty) {
	vm_page_t *page;

	assert(node->type == VFS_NODE_FILE);

	mutex_lock(&node->lock);

	if(!(page = avl_tree_lookup(&node->pages, (key_t)offset))) {
		fatal("Tried to release page that isn't cached");
	}

	dprintf("vfs: released page 0x%" PRIpp " at offset %" PRId64 " in %p\n",
	        page->addr, offset, node);

	/* Mark as dirty if requested. */
	if(dirty) {
		page->modified = true;
	}

	/* Decrease the reference count. If it reaches 0, and the page is
	 * outside the node's size (i.e. file has been truncated with pages in
	 * use), discard it. */
	if(refcount_dec(&page->count) == 0 && (file_size_t)offset >= node->size) {
		avl_tree_remove(&node->pages, (key_t)offset);
		vm_page_free(page, 1);
	}

	mutex_unlock(&node->lock);
}

/** Flush a page from a file.
 * @param node		Node of page to flush. Should be locked.
 * @param page		Page to flush.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_page_flush(vfs_node_t *node, vm_page_t *page) {
	void *mapping;
	int ret = 0;

	/* If the page is outside of the file, it may be there because the file
	 * was truncated but with the page in use. Ignore this. Also ignore
	 * pages that aren't dirty. */
	if((file_size_t)page->offset >= node->size || !page->modified) {
		return 0;
	}

	/* Page shouldn't be dirty if mount read only. */
	assert(!VFS_NODE_IS_RDONLY(node));

	if(node->mount && node->mount->type->page_flush) {
		mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);

		if((ret = node->mount->type->page_flush(node, mapping, page->offset, false)) == 0) {
			/* Clear dirty flag if the page reference count is
			 * zero. This is because a page may be mapped into an
			 * address space as read-write, but has not yet been
			 * written to. */
			if(refcount_get(&page->count) == 0) {
				page->modified = false;
			}
		}

		page_phys_unmap(mapping, PAGE_SIZE, true);
	}

	return ret;
}

/** Get and map a page from a file's data cache.
 * @param node		Node to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in from
 *			the filesystem if it is not in the cache - useful if
 *			the caller is about to overwrite the page data.
 * @param addrp		Where to store address of mapping.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_page_map(vfs_node_t *node, offset_t offset, bool overwrite, void **addrp, bool *sharedp) {
	assert(addrp && sharedp);
	return vfs_file_page_get_internal(node, offset, overwrite, NULL, addrp, sharedp);
}

/** Unmap and release a page from a node's data cache.
 * @param node		Node to release page in.
 * @param mapping	Address of mapping.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied.
 * @param shared	Shared value returned from vfs_file_page_map(). */
static void vfs_file_page_unmap(vfs_node_t *node, void *mapping, offset_t offset, bool dirty, bool shared) {
	page_phys_unmap(mapping, PAGE_SIZE, shared);
	if(!shared) {
		thread_unwire(curr_thread);
	}
	vfs_file_page_release_internal(node, offset, dirty);
}

/** Close a handle to a file.
 * @param handle	Handle to close. */
static void vfs_file_object_close(object_handle_t *handle) {
	vfs_node_release((vfs_node_t *)handle->object);
	kfree(handle->data);
}

/** Check if a file can be memory-mapped.
 * @param handle	Handle to fi;e.
 * @param flags		Mapping flags (VM_MAP_*).
 * @return		0 if can be mapped, negative error code if not. */
static int vfs_file_object_mappable(object_handle_t *handle, int flags) {
	vfs_handle_t *data = handle->data;

	/* If shared write access is required, ensure that the handle flags
	 * allow this. */
	if(!(flags & VM_MAP_PRIVATE) && flags & VM_MAP_WRITE && !(data->flags & FS_FILE_WRITE)) {
		return -ERR_PERM_DENIED;
	} else {
		return 0;
	}
}

/** Get a page from a file object.
 * @param handle	Handle to file to get page from.
 * @param offset	Offset of page to get.
 * @param physp		Where to store physical address of page.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_object_page_get(object_handle_t *handle, offset_t offset, phys_ptr_t *physp) {
	vm_page_t *page;
	int ret;

	ret = vfs_file_page_get_internal((vfs_node_t *)handle->object, offset, false, &page, NULL, NULL);
	if(ret == 0) {
		*physp = page->addr;
	}

	return ret;
}

/** Release a page from a file VM object.
 * @param handle	Handle to file that the page belongs to.
 * @param offset	Offset of page to release.
 * @param paddr		Physical address of page that was unmapped. */
static void vfs_file_object_page_release(object_handle_t *handle, offset_t offset, phys_ptr_t paddr) {
	vfs_file_page_release_internal((vfs_node_t *)handle->object, offset, false);
}

/** File object operations. */
static object_type_t vfs_file_object_type = {
	.id = OBJECT_TYPE_FILE,
	.close = vfs_file_object_close,
	.mappable = vfs_file_object_mappable,
	.get_page = vfs_file_object_page_get,
	.release_page = vfs_file_object_page_release,
};

/** Create a regular file in the file system.
 * @param path		Path to file to create.
 * @return		0 on success, negative error code on failure. */
int vfs_file_create(const char *path) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, VFS_NODE_FILE);

	/* Call the common creation code. */
	ret = vfs_node_create(path, node);
	vfs_node_release(node);
	return ret;
}

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
int vfs_file_from_memory(const void *buf, size_t size, int flags, object_handle_t **handlep) {
	object_handle_t *handle;
	vfs_node_t *node;
	int ret;

	if(!buf || !size || !flags || !handlep) {
		return -ERR_PARAM_INVAL;
	}

	/* Create a node to store the data. */
	node = vfs_node_alloc(NULL, VFS_NODE_FILE);
	node->size = size;

	/* Create a temporary handle to the file with write permission and
	 * write the data to the file. */
	handle = vfs_handle_create(node, FS_FILE_WRITE);
	if((ret = vfs_file_write(handle, buf, size, 0, NULL)) == 0) {
		*handlep = vfs_handle_create(node, flags);
	}

	object_handle_release(handle);
	vfs_node_release(node);
	return ret;
}

/** Open a handle to a file.
 * @param path		Path to file to open.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		0 on success, negative error code on failure. */
int vfs_file_open(const char *path, int flags, object_handle_t **handlep) {
	vfs_node_t *node;
	int ret;

	/* Look up the filesystem node and check if it is suitable. */
	if((ret = vfs_node_lookup(path, true, VFS_NODE_FILE, &node)) != 0) {
		return ret;
	} else if(flags & FS_FILE_WRITE && VFS_NODE_IS_RDONLY(node)) {
		vfs_node_release(node);
		return -ERR_READ_ONLY;
	}

	*handlep = vfs_handle_create(node, flags);
	vfs_node_release(node);
	return 0;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. If a non-negative offset is supplied,
 * then it will be used as the offset to read from, and the offset of the file
 * handle will not be taken into account or updated. Otherwise, the read will
 * occur from the file handle's current offset, and before returning the offset
 * will be incremented by the number of bytes read.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset within the file to read from (if non-negative).
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_read(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	bool update = false;
	vfs_handle_t *data;
	vfs_node_t *node;
	size_t total = 0;
	void *mapping;
	bool shared;
	int ret = 0;

	if(!handle || !buf) {
		ret = -ERR_PARAM_INVAL;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	}

	node = (vfs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == VFS_NODE_FILE);

	if(!(data->flags & FS_FILE_READ)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* If not overriding the handle's offset, pull the offset out of the
	 * handle structure. */
	if(offset < 0) {
		rwlock_read_lock(&data->lock);
		offset = data->offset;
		rwlock_unlock(&data->lock);
		update = true;
	}

	mutex_lock(&node->lock);

	/* Ensure that we do not go pass the end of the node. */
	if(offset >= (offset_t)node->size) {
		mutex_unlock(&node->lock);
		goto out;
	} else if((offset + (offset_t)count) >= (offset_t)node->size) {
		count = (size_t)((offset_t)node->size - offset);
	}

	mutex_unlock(&node->lock);

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, false, &mapping, &shared)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(buf, mapping + (offset % PAGE_SIZE), size);
		vfs_file_page_unmap(node, mapping, start, false, shared);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, false, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, PAGE_SIZE);
		vfs_file_page_unmap(node, mapping, start, false, shared);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vfs_file_page_map(node, start, false, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, count);
		vfs_file_page_unmap(node, mapping, start, false, shared);
		total += count;
	}

	dprintf("vfs: read %zu bytes from offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
	        total, offset, node, (node->mount) ? node->mount->id : -1, node->id);
	ret = 0;
out:
	/* Update handle offset if required. */
	if(update && total) {
		rwlock_write_lock(&data->lock);
		data->offset += total;
		rwlock_unlock(&data->lock);
	}

	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Write to a file.
 *
 * Writes data from a buffer into a file. If a non-negative offset is supplied,
 * then it will be used as the offset to write to. In this case, neither the
 * offset of the file handle or the FS_FILE_APPEND flag will be taken into
 * account, and the handle's offset will not be modified. Otherwise, the write
 * will occur at the file handle's current offset (if the FS_FILE_APPEND flag
 * is set, the offset will be set to the end of the file and the write will
 * take place there), and before returning the handle's offset will be
 * incremented by the number of bytes written.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FS_FILE_APPEND is
 *			set).
 * @param offset	Offset within the file to write to (if non-negative).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even if the call fails, as it can fail when
 *			part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_write(object_handle_t *handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	bool update = false;
	vfs_handle_t *data;
	vfs_node_t *node;
	size_t total = 0;
	void *mapping;
	bool shared;
	int ret = 0;

	if(!handle || !buf) {
		ret = -ERR_PARAM_INVAL;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	}

	node = (vfs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == VFS_NODE_FILE);

	if(!(data->flags & FS_FILE_WRITE)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* If not overriding the handle's offset, pull the offset out of the
	 * handle structure, and handle the FS_FILE_APPEND flag. */
	if(offset < 0) {
		rwlock_write_lock(&data->lock);
		if(data->flags & FS_FILE_APPEND) {
			data->offset = node->size;
		}
		offset = data->offset;
		rwlock_unlock(&data->lock);
		update = true;
	}

	mutex_lock(&node->lock);

	/* Attempt to resize the node if necessary. */
	if((offset + (offset_t)count) >= (offset_t)node->size) {
		/* If the resize operation is not provided, we can only write
		 * within the space that we have. */
		if(!node->mount || !node->mount->type->file_resize) {
			if(offset >= (offset_t)node->size) {
				ret = 0;
				mutex_unlock(&node->lock);
				goto out;
			} else {
				count = (size_t)((offset_t)node->size - offset);
			}
		} else {
			if((ret = node->mount->type->file_resize(node, (file_size_t)(offset + count))) != 0) {
				mutex_unlock(&node->lock);
				goto out;
			}

			node->size = (file_size_t)(offset + count);
		}
	}

	/* Exclusive access no longer required. */
	mutex_unlock(&node->lock);

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, false, &mapping, &shared)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(mapping + (offset % PAGE_SIZE), buf, size);
		vfs_file_page_unmap(node, mapping, start, true, shared);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. We pass the overwrite parameter as true to
	 * vfs_file_page_map() here, so that if the page is not in the cache,
	 * its data will not be read in - we're about to overwrite it, so it
	 * would not be necessary. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, true, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, PAGE_SIZE);
		vfs_file_page_unmap(node, mapping, start, true, shared);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vfs_file_page_map(node, start, false, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, count);
		vfs_file_page_unmap(node, mapping, start, true, shared);
		total += count;
	}

	dprintf("vfs: wrote %zu bytes to offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
	        total, offset, node, (node->mount) ? node->mount->id : -1, node->id);
	ret = 0;
out:
	/* Update handle offset if required. */
	if(update && total) {
		rwlock_write_lock(&data->lock);
		data->offset += total;
		rwlock_unlock(&data->lock);
	}

	if(bytesp) {
		*bytesp = total;
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
int vfs_file_resize(object_handle_t *handle, file_size_t size) {
	vfs_handle_t *data;
	vfs_node_t *node;
	vm_page_t *page;
	int ret;

	if(!handle) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return -ERR_TYPE_INVAL;
	}

	node = (vfs_node_t *)handle->object;
	data = handle->data;
	mutex_lock(&node->lock);
	assert(node->type == VFS_NODE_FILE);

	/* Check if resizing is allowed. */
	if(!(data->flags & FS_FILE_WRITE)) {
		mutex_unlock(&node->lock);
		return -ERR_PERM_DENIED;
	} else if(!node->mount->type->file_resize) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_SUPPORTED;
	}

	if((ret = node->mount->type->file_resize(node, size)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	}

	/* Shrink the cache if the new size is smaller. If any pages are in use
	 * they will get freed once they are released. */
	if(size < node->size) {
		AVL_TREE_FOREACH(&node->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			if((file_size_t)page->offset >= size && refcount_get(&page->count) == 0) {
				avl_tree_remove(&node->pages, (key_t)page->offset);
				vm_page_free(page, 1);
			}
		}
	}

	node->size = (file_size_t)size;
	mutex_unlock(&node->lock);
	return 0;
}

/** Close a handle to a directory.
 * @param handle	Handle to close. */
static void vfs_dir_object_close(object_handle_t *handle) {
	vfs_node_release((vfs_node_t *)handle->object);
	kfree(handle->data);
}

/** Directory object operations. */
static object_type_t vfs_dir_object_type = {
	.id = OBJECT_TYPE_DIR,
	.close = vfs_dir_object_close,
};

/** Populate a directory's entry cache if it is empty.
 * @param node		Node of directory.
 * @return		0 on success, negative error code on failure. */
static int vfs_dir_cache_entries(vfs_node_t *node) {
	int ret = 0;

	/* If the entry count is 0, we consider the cache to be empty - even
	 * if the directory is empty, the cache should at least have '.' and
	 * '..' entries. */
	if(!node->entry_count) {
		if(!node->mount->type->dir_cache) {
			kprintf(LOG_WARN, "vfs: entry cache empty, but filesystem %p lacks dir_cache!\n",
			        node->mount->type);
			ret = -ERR_NOT_FOUND;
		} else {
			ret = node->mount->type->dir_cache(node);
		}
	}

	return ret;
}

/** Get the node ID of a directory entry.
 * @param node		Node of directory (should be locked).
 * @param name		Name of entry to get.
 * @param idp		Where to store ID of node.
 * @return		0 on success, negative error code on failure. */
static int vfs_dir_entry_get(vfs_node_t *node, const char *name, node_id_t *idp) {
	fs_dir_entry_t *entry;
	int ret;

	assert(node->type == VFS_NODE_DIR);
	assert(node->mount);

	/* Populate the entry cache if it is empty. */
	if((ret = vfs_dir_cache_entries(node)) != 0) {
		return ret;
	}

	/* Look up the entry. */
	if((entry = radix_tree_lookup(&node->dir_entries, name))) {
		*idp = entry->id;
		return 0;
	} else {
		return -ERR_NOT_FOUND;
	}
}

/** Add an entry to a directory's entry cache.
 *
 * Adds an entry to a directory node's entry cache. This function should only
 * be used by filesystem type operations and the VFS itself.
 *
 * @param node		Node to add entry to.
 * @param id		ID of node entry points to.
 * @param name		Name of entry.
 */
void vfs_dir_entry_add(vfs_node_t *node, node_id_t id, const char *name) {
	fs_dir_entry_t *entry;
	size_t len;

	/* Work out the length we need. */
	len = sizeof(fs_dir_entry_t) + strlen(name) + 1;

	/* Allocate the buffer for it and fill it in. */
	entry = kmalloc(len, MM_SLEEP);
	entry->length = len;
	entry->id = id;
	strcpy(entry->name, name);

	/* Insert into the cache. */
	radix_tree_insert(&node->dir_entries, name, entry);

	/* Increase count. */
	node->entry_count++;
}

/** Remove an entry from a directory's entry cache.
 * @param node		Node to remove entry from.
 * @param name		Name of entry to remove. */
static void vfs_dir_entry_remove(vfs_node_t *node, const char *name) {
	radix_tree_remove(&node->dir_entries, name, kfree);
	node->entry_count--;
}

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @return		0 on success, negative error code on failure. */
int vfs_dir_create(const char *path) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, VFS_NODE_DIR);

	/* Call the common creation code. */
	ret = vfs_node_create(path, node);
	vfs_node_release(node);
	return ret;
}

/** Open a handle to a directory.
 * @param path		Path to directory to open.
 * @param flags		Behaviour flags for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		0 on success, negative error code on failure. */
int vfs_dir_open(const char *path, int flags, object_handle_t **handlep) {
	vfs_node_t *node;
	int ret;

	/* Look up the filesystem node. */
	if((ret = vfs_node_lookup(path, true, VFS_NODE_DIR, &node)) != 0) {
		return ret;
	}

	*handlep = vfs_handle_create(node, flags);
	vfs_node_release(node);
	return 0;
}

/** Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed. If the index provided is a
 * non-negative value, then the handle's current index will not be used or
 * modified, and the supplied value will be used instead. Otherwise, the
 * current index will be used, and upon success it will be incremented by 1.
 *
 * @param handle	Handle to directory to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_BUF_TOO_SMALL
 *			will be returned).
 * @param index		Index of the directory entry to read, if not negative.
 *			If not found, -ERR_NOT_FOUND will be returned.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_dir_read(object_handle_t *handle, fs_dir_entry_t *buf, size_t size, offset_t index) {
	fs_dir_entry_t *entry = NULL;
	vfs_node_t *child, *node;
	bool update = false;
	vfs_handle_t *data;
	offset_t i = 0;
	int ret;

	if(!handle || !buf) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (vfs_node_t *)handle->object;
	data = handle->data;
	assert(node->type == VFS_NODE_DIR);

	/* If not overriding the handle's offset, pull the offset out of the
	 * handle structure. */
	if(index < 0) {
		rwlock_read_lock(&data->lock);
		index = data->offset;
		rwlock_unlock(&data->lock);
		update = true;
	}

	mutex_lock(&node->lock);

	/* Cache the directory entries if we do not already have them, and
	 * check that the index is valid. */
	if((ret = vfs_dir_cache_entries(node)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	} else if(index >= (offset_t)node->entry_count) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_FOUND;
	}

	/* Iterate through the tree to find the entry. */
	RADIX_TREE_FOREACH(&node->dir_entries, iter) {
		if(i++ == index) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);
			break;
		}
	}

	/* We should have it because we checked against the entry count. */
	if(!entry) {
		fatal("Entry %" PRId64 " within size but not found (%p)", index, node);
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
			if((ret = vfs_dir_entry_get(node->mount->mountpoint, "..", &buf->id)) != 0) {
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
				if(child->type == VFS_NODE_DIR && child->mounted) {
					buf->id = child->mounted->root->id;
				}
				mutex_unlock(&child->lock);
			}
		}
	}

	mutex_unlock(&node->mount->lock);
	mutex_unlock(&node->lock);

	/* Update offset in the handle. */
	if(update) {
		rwlock_read_lock(&data->lock);
		data->offset++;
		rwlock_unlock(&data->lock);
	}
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
int vfs_handle_seek(object_handle_t *handle, int action, offset_t offset, offset_t *newp) {
	vfs_handle_t *data;
	vfs_node_t *node;
	int ret;

	if(!handle || (action != FS_SEEK_SET && action != FS_SEEK_ADD && action != FS_SEEK_END)) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (vfs_node_t *)handle->object;
	data = handle->data;
	rwlock_write_lock(&data->lock);

	/* Perform the action. */
	switch(action) {
	case FS_SEEK_SET:
		data->offset = offset;
		break;
	case FS_SEEK_ADD:
		data->offset += offset;
		break;
	case FS_SEEK_END:
		mutex_lock(&node->lock);

		if(node->type == VFS_NODE_DIR) {
			/* To do this on directories, we must cache the entries
			 * to know the entry count. */
			if((ret = vfs_dir_cache_entries(node)) != 0) {
				mutex_unlock(&node->lock);
				rwlock_unlock(&data->lock);
				return ret;
			}
			data->offset = node->entry_count + offset;
		} else {
			data->offset = node->size + offset;
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
int vfs_handle_info(object_handle_t *handle, fs_info_t *info) {
	if(!handle || !info) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	vfs_node_info((vfs_node_t *)handle->object, info);
	return 0;
}

/** Flush changes to a filesystem node to the FS.
 * @param handle	Handle to node to flush.
 * @return		0 on success, negative error code on failure. */
int vfs_handle_sync(object_handle_t *handle) {
	vfs_node_t *node;
	int ret;

	if(!handle) {
		return -ERR_PARAM_INVAL;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE &&
	          handle->object->type->id != OBJECT_TYPE_DIR) {
		return -ERR_TYPE_INVAL;
	}

	node = (vfs_node_t *)handle->object;

	mutex_lock(&node->mount->lock);
	mutex_lock(&node->lock);
	ret = vfs_node_flush(node, false);
	mutex_unlock(&node->lock);
	mutex_unlock(&node->mount->lock);

	return ret;
}

/** Ensure that a symbolic link's destination is cached.
 * @param node		Node of link (should be locked).
 * @return		0 on success, negative error code on failure. */
static int vfs_symlink_cache_dest(vfs_node_t *node) {
	int ret;

	assert(node->type == VFS_NODE_SYMLINK);

	if(!node->link_dest) {
		/* Assume that if the node is a symbolic link and it does not
		 * have its destination cached its mount has a read link
		 * operation. Only check this if there is no destination, for
		 * example to allow RamFS to have the destination permanently
		 * cached without the operation implemented. */
		assert(node->mount->type->symlink_read);

		if((ret = node->mount->type->symlink_read(node, &node->link_dest)) != 0) {
			return ret;
		}

		assert(node->link_dest);
	}

	return 0;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		0 on success, negative error code on failure. */
int vfs_symlink_create(const char *path, const char *target) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, VFS_NODE_SYMLINK);
	node->link_dest = kstrdup(target, MM_SLEEP);

	/* Call the common creation code. */
	ret = vfs_node_create(path, node);
	vfs_node_release(node);
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
int vfs_symlink_read(const char *path, char *buf, size_t size) {
	vfs_node_t *node;
	size_t len;
	int ret;

	if(!path || !buf || !size) {
		return -ERR_PARAM_INVAL;
	} else if((ret = vfs_node_lookup(path, false, VFS_NODE_SYMLINK, &node)) != 0) {
		return ret;
	}

	mutex_lock(&node->lock);

	/* Ensure destination is cached. */
	if((ret = vfs_symlink_cache_dest(node)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	}

	len = ((len = strlen(node->link_dest) + 1) > size) ? size : len;
	memcpy(buf, node->link_dest, len);
	mutex_unlock(&node->lock);
	vfs_node_release(node);
	return (int)len;
}

/** Look up a mount by ID.
 * @note		Does not take the mount lock.
 * @param id		ID of mount to look up.
 * @return		Pointer to mount if found, NULL if not. */
static vfs_mount_t *vfs_mount_lookup(mount_id_t id) {
	vfs_mount_t *mount;

	LIST_FOREACH(&vfs_mount_list, iter) {
		mount = list_entry(iter, vfs_mount_t, header);
		if(mount->id == id) {
			return mount;
		}
	}

	return NULL;
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Some filesystem types are read-only by design - when mounting these the
 * FS_MOUNT_RDONLY flag will automatically be set. It may also be set if the
 * device the filesystem resides on is read-only. Mounting multiple filesystems
 * on one directory at a time is not allowed.
 *
 * @param dev		Device path for backing device for mount (can be NULL
 *			if the filesystem does not require a backing device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type (if not specified, device will
 *			be probed to determine the correct type - in this case,
 *			a device must be specified).
 * @param flags		Flags to specify mount behaviour.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_mount(const char *dev, const char *path, const char *type, int flags) {
	vfs_mount_t *mount = NULL;
	vfs_node_t *node = NULL;
	device_t *device;
	int ret;

	if(!path || (!dev && !type)) {
		return -ERR_PARAM_INVAL;
	}

	/* Lock the mount lock across the entire operation, so that only one
	 * mount can take place at a time. */
	mutex_lock(&vfs_mount_lock);

	/* If the root filesystem is not yet mounted, the only place we can
	 * mount is '/'. */
	if(!vfs_root_mount) {
		assert(curr_proc == kernel_proc);
		if(strcmp(path, "/") != 0) {
			ret = -ERR_NOT_FOUND;
			goto fail;
		}
	} else {
		/* Look up the destination directory. */
		if((ret = vfs_node_lookup(path, true, VFS_NODE_DIR, &node)) != 0) {
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
	mount = kmalloc(sizeof(vfs_mount_t), MM_SLEEP);
	list_init(&mount->header);
	list_init(&mount->used_nodes);
	list_init(&mount->unused_nodes);
	avl_tree_init(&mount->nodes);
	mutex_init(&mount->lock, "vfs_mount_lock", 0);
	mount->type = NULL;
	mount->device = NULL;
	mount->root = NULL;
	mount->flags = flags;
	mount->mountpoint = node;

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
	if(!type) {
		if(!(mount->type = vfs_type_probe(mount->device))) {
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}
	} else {
		if(!(mount->type = vfs_type_lookup(type))) {
			ret = -ERR_PARAM_INVAL;
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
		} else if(!mount->type->probe(mount->device)) {
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}
	}

	assert(mount->type->mount);

	/* Allocate a mount ID. */
	if(vfs_next_mount_id == UINT16_MAX) {
		ret = -ERR_NO_SPACE;
		goto fail;
	}
	mount->id = vfs_next_mount_id++;

	/* If the type is read-only, set read-only in the mount flags. */
	if(mount->type->flags & VFS_TYPE_RDONLY) {
		mount->flags |= FS_MOUNT_RDONLY;
	}

	/* Call the filesystem's mount operation. */
	ret = mount->type->mount(mount);
	if(ret != 0) {
		goto fail;
	}

	assert(mount->root);

	/* Put the root node into the node tree/used list. */
	avl_tree_insert(&mount->nodes, (key_t)mount->root->id, mount->root, NULL);
	list_append(&mount->used_nodes, &mount->root->mount_link);

	/* Make the mount point point to the new mount. */
	if(mount->mountpoint) {
		mount->mountpoint->mounted = mount;
		mutex_unlock(&mount->mountpoint->lock);
	}

	/* Store mount in mounts list and unlock the mount lock. */
	list_append(&vfs_mount_list, &mount->header);
	if(!vfs_root_mount) {
		vfs_root_mount = mount;

		/* Give the kernel process a correct current/root directory. */
		vfs_node_get(vfs_root_mount->root);
		curr_proc->ioctx.root_dir = vfs_root_mount->root;
		vfs_node_get(vfs_root_mount->root);
		curr_proc->ioctx.curr_dir = vfs_root_mount->root;
	}
	mutex_unlock(&vfs_mount_lock);

	dprintf("vfs: mounted %s on %s (mount: %p(%" PRIu16 "), root: %p, device: %s)\n",
	        mount->type->name, path, mount, mount->id, mount->root,
	        (dev) ? dev : "<none>");
	return 0;
fail:
	if(mount) {
		if(mount->device) {
			object_handle_release(mount->device);
		}
		if(mount->root) {
			slab_cache_free(vfs_node_cache, mount->root);
		}
		if(mount->type) {
			refcount_dec(&mount->type->count);
		}
		kfree(mount);
	}
	if(node) {
		mutex_unlock(&node->lock);
		vfs_node_release(node);
	}
	mutex_unlock(&vfs_mount_lock);
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
int vfs_unmount(const char *path) {
	vfs_node_t *node = NULL, *child;
	vfs_mount_t *mount = NULL;
	int ret;

	if(!path) {
		return -ERR_PARAM_INVAL;
	}

	/* Serialise mount/unmount operations. */
	mutex_lock(&vfs_mount_lock);

	/* Look up the destination directory. */
	if((ret = vfs_node_lookup(path, true, VFS_NODE_DIR, &node)) != 0) {
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
		child = list_entry(iter, vfs_node_t, mount_link);

		/* On success, the child is unlocked by vfs_node_free(). */
		mutex_lock(&child->lock);
		if((ret = vfs_node_free(child)) != 0) {
			mutex_unlock(&child->lock);
			goto fail;
		}
	}

	/* Free the root node itself. */
	refcount_dec(&node->count);
	if((ret = vfs_node_free(node)) != 0) {
		refcount_inc(&node->count);
		goto fail;
	}

	/* Detach from the mountpoint. */
	mount->mountpoint->mounted = NULL;
	mutex_unlock(&mount->mountpoint->mount->lock);
	vfs_node_release(mount->mountpoint);

	/* Call unmount operation and release device/type. */
	if(mount->type->unmount) {
		mount->type->unmount(mount);
	}
	if(mount->device) {
		object_handle_release(mount->device);
	}
	refcount_dec(&mount->type->count);

	list_remove(&mount->header);
	mutex_unlock(&vfs_mount_lock);
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
			vfs_node_release(node);
		}
	}
	mutex_unlock(&vfs_mount_lock);
	return ret;
}

/** Get information about a filesystem entry.
 * @param path		Path to get information on.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param info		Information structure to fill in.
 * @return		0 on success, negative error code on failure. */
int vfs_info(const char *path, bool follow, fs_info_t *info) {
	vfs_node_t *node;
	int ret;

	if(!path || !info) {
		return -ERR_PARAM_INVAL;
	} else if((ret = vfs_node_lookup(path, follow, -1, &node)) != 0) {
		return ret;
	}

	vfs_node_info(node, info);
	vfs_node_release(node);
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
int vfs_unlink(const char *path) {
	vfs_node_t *parent = NULL, *node = NULL;
	fs_dir_entry_t *entry;
	char *dir, *name;
	int ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	dprintf("vfs: unlink(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	if((ret = vfs_node_lookup(dir, true, VFS_NODE_DIR, &parent)) != 0) {
		goto out;
	} else if((ret = vfs_node_lookup(path, false, -1, &node)) != 0) {
		goto out;
	}

	mutex_lock(&parent->lock);
	mutex_lock(&node->lock);

	if(parent->mount != node->mount) {
		ret = -ERR_IN_USE;
		goto out;
	} else if(VFS_NODE_IS_RDONLY(node)) {
		ret = -ERR_READ_ONLY;
		goto out;
	} else if(!node->mount->type->node_unlink) {
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	}

	/* If it is a directory, ensure that it is empty. */
	if(node->type == VFS_NODE_DIR) {
		if((ret = vfs_dir_cache_entries(node)) != 0) {
			goto out;
		}

		RADIX_TREE_FOREACH(&node->dir_entries, iter) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);

			if(strcmp(entry->name, "..") && strcmp(entry->name, ".")) {
				ret = -ERR_IN_USE;
				goto out;
			}
		}
	}

	/* Call the filesystem's unlink operation. */
	if((ret = node->mount->type->node_unlink(parent, name, node)) == 0) {
		/* Update the directory entry cache. */
		vfs_dir_entry_remove(parent, name);
	}
out:
	if(node) {
		mutex_unlock(&node->lock);
		mutex_unlock(&parent->lock);
		vfs_node_release(node);
		vfs_node_release(parent);
	} else if(parent) {
		vfs_node_release(parent);
	}
	kfree(dir);
	kfree(name);
	return ret;
}

/** Print a list of mounts.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Always returns KDBG_OK. */
int kdbg_cmd_mounts(int argc, char **argv) {
	vfs_mount_t *mount;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out a list of all mounted filesystems.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID    Flags Type       Data               Root               Mountpoint\n");
	kprintf(LOG_NONE, "==    ===== ====       ====               ====               ==========\n");

	LIST_FOREACH(&vfs_mount_list, iter) {
		mount = list_entry(iter, vfs_mount_t, header);

		kprintf(LOG_NONE, "%-5" PRIu16 " %-5d %-10s %-18p %-18p %-18p\n",
		        mount->id, mount->flags, (mount->type) ? mount->type->name : "invalid",
		        mount->data, mount->root, mount->mountpoint);
	}

	return KDBG_OK;
}

/** Print a list of nodes.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_vnodes(int argc, char **argv) {
	vfs_mount_t *mount;
	vfs_node_t *node;
	unative_t id;
	list_t *list;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<--unused|--used>] <mount ID>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of nodes currently in memory for a mount. If no argument is\n");
		kprintf(LOG_NONE, "specified, then all nodes will be printed, else the nodes from the specified\n");
		kprintf(LOG_NONE, "list will be printed.\n");
		return KDBG_OK;
	} else if(argc < 2 || argc > 3) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	} else if(argc == 3 && strcmp(argv[1], "--unused") != 0 && strcmp(argv[1], "--used") != 0) {
		kprintf(LOG_NONE, "Unrecognized argument '%s'.\n", argv[1]);
		return KDBG_FAIL;
	}

	/* Get the mount ID. */
	if(kdbg_parse_expression((argc == 3) ? argv[2] : argv[1], &id, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}

	/* Search for the mount. */
	if(!(mount = vfs_mount_lookup((mount_id_t)id))) {
		kprintf(LOG_NONE, "Unknown mount ID %" PRIun ".\n", id);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID       Flags Count Locked Type Size         Pages      Entries Mount\n");
	kprintf(LOG_NONE, "==       ===== ===== ====== ==== ====         =====      ======= =====\n");

	if(argc == 3) {
		list = (strcmp(argv[1], "--unused") == 0) ? &mount->unused_nodes : &mount->used_nodes;

		LIST_FOREACH(list, iter) {
			node = list_entry(iter, vfs_node_t, mount_link);

			kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-5d %-6d %-4d %-12" PRIu64 " %-10zu %-7zu %p\n",
			        node->id, node->flags, refcount_get(&node->count),
			        atomic_get(&node->lock.locked), node->type, node->size,
			        (size_t)(ROUND_UP(node->size, PAGE_SIZE) / PAGE_SIZE),
			        node->entry_count, node->mount);
		}
	} else {
		AVL_TREE_FOREACH(&mount->nodes, iter) {
			node = avl_tree_entry(iter, vfs_node_t);

			kprintf(LOG_NONE, "%-8" PRIu64 " %-5d %-5d %-6d %-4d %-12" PRIu64 " %-10zu %-7zu %p\n",
			        node->id, node->flags, refcount_get(&node->count),
			        atomic_get(&node->lock.locked), node->type, node->size,
			        (size_t)(ROUND_UP(node->size, PAGE_SIZE) / PAGE_SIZE),
			        node->entry_count, node->mount);
		}
	}
	return KDBG_FAIL;
}

/** Print information about a node.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_vnode(int argc, char **argv) {
	fs_dir_entry_t *entry;
	vfs_mount_t *mount;
	vfs_node_t *node;
	vm_page_t *page;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <mount ID> <node ID>\n", argv[0]);
		kprintf(LOG_NONE, "       %s <address>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints details of a single filesystem node that's currently in memory.\n");
		return KDBG_OK;
	}

	/* Look up the node according to the arguments. */
	if(argc == 3) {
		/* Get the mount ID and search for it. */
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}
		if(!(mount = vfs_mount_lookup((mount_id_t)val))) {
			kprintf(LOG_NONE, "Unknown mount ID %" PRIun ".\n", val);
			return KDBG_FAIL;
		}

		/* Get the node ID and search for it. */
		if(kdbg_parse_expression(argv[2], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}
		if(!(node = avl_tree_lookup(&mount->nodes, (key_t)val))) {
			kprintf(LOG_NONE, "Unknown node ID %" PRIun ".\n", val);
			return KDBG_FAIL;
		}
	} else if(argc == 2) {
		/* Get the address. */
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}

		node = (vfs_node_t *)((ptr_t)val);
	} else {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Print out basic node information. */
	kprintf(LOG_NONE, "Node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
	        (node->mount) ? node->mount->id : -1, node->id);
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
	kprintf(LOG_NONE, "Data:         %p\n", node->data);
	kprintf(LOG_NONE, "Flags:        %d\n", node->flags);
	kprintf(LOG_NONE, "Type:         %d\n", node->type);
	if(node->type == VFS_NODE_FILE) {
		kprintf(LOG_NONE, "Data Size:    %" PRIu64 "\n", node->size);
	}
	if(node->type == VFS_NODE_SYMLINK) {
		kprintf(LOG_NONE, "Destination:  %p(%s)\n", node->link_dest,
		        (node->link_dest) ? node->link_dest : "<not cached>");
	}
	if(node->type == VFS_NODE_DIR) {
		kprintf(LOG_NONE, "Entries:      %zu\n", node->entry_count);
		if(node->mounted) {
			kprintf(LOG_NONE, "Mounted:      %p(%" PRIu16 ")\n", node->mounted,
			        node->mounted->id);
		}
	}

	/* If it is a directory, print out a list of cached entries. If it is
	 * a file, print out a list of cached pages. */
	if(node->type == VFS_NODE_DIR) {
		kprintf(LOG_NONE, "\nCached directory entries:\n");

		RADIX_TREE_FOREACH(&node->dir_entries, iter) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);

			kprintf(LOG_NONE, "  Entry %p - %" PRIu64 "(%s)\n",
			        entry, entry->id, entry->name);
		}
	} else if(node->type == VFS_NODE_FILE) {
		kprintf(LOG_NONE, "\nCached pages:\n");

		AVL_TREE_FOREACH(&node->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			kprintf(LOG_NONE, "  Page 0x%016" PRIpp " - Offset: %-10" PRId64 " Modified: %-1d Count: %d\n",
			        page->addr, page->offset, page->modified, refcount_get(&page->count));
		}
	}

	return KDBG_OK;
}

/** Mount the root filesystem.
 * @param args		Kernel arguments structure. */
void __init_text vfs_mount_root(kernel_args_t *args) {
	if(!vfs_root_mount) {
		fatal("Root filesystem probe not implemented");
	}
}

/** Initialisation function for the VFS. */
void __init_text vfs_init(void) {
	vfs_node_cache = slab_cache_create("vfs_node_cache", sizeof(vfs_node_t), 0,
	                                   vfs_node_cache_ctor, NULL, vfs_node_cache_reclaim,
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

	ret = vfs_file_create(kpath);
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
	} else if((ret = vfs_file_open(kpath, flags, &handle)) != 0) {
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
 * Reads data from a file into a buffer. If a non-negative offset is supplied,
 * then it will be used as the offset to read from, and the offset of the file
 * handle will not be taken into account or updated. Otherwise, the read will
 * occur from the file handle's current offset, and before returning the offset
 * will be incremented by the number of bytes read.
 *
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset within the file to read from (if non-negative).
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_read(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
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
	ret = vfs_file_read(obj, kbuf, count, offset, &bytes);
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
 * Writes data from a buffer into a file. If a non-negative offset is supplied,
 * then it will be used as the offset to write to. In this case, neither the
 * offset of the file handle or the FS_FILE_APPEND flag will be taken into
 * account, and the handle's offset will not be modified. Otherwise, the write
 * will occur at the file handle's current offset (if the FS_FILE_APPEND flag
 * is set, the offset will be set to the end of the file and the write will
 * take place there), and before returning the handle's offset will be
 * incremented by the number of bytes written.
 *
 * @param handle	Handle to file to write to.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FS_FILE_APPEND is
 *			set).
 * @param offset	Offset within the file to write to (if non-negative).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even if the call fails, as it can fail when
 *			part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_write(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
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

	/* Perform the actual write and update file offset if necessary. */
	ret = vfs_file_write(obj, kbuf, count, offset, &bytes);
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
int sys_fs_file_resize(handle_t handle, file_size_t size) {
	object_handle_t *obj;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, OBJECT_TYPE_FILE, &obj)) != 0) {
		return ret;
	}

	ret = vfs_file_resize(obj, size);
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

	ret = vfs_dir_create(kpath);
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
	} else if((ret = vfs_dir_open(kpath, flags, &handle)) != 0) {
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
 * to ensure that the buffer isn't overflowed. If the index provided is a
 * non-negative value, then the handle's current index will not be used or
 * modified, and the supplied value will be used instead. Otherwise, the
 * current index will be used, and upon success it will be incremented by 1.
 *
 * @param handle	Handle to directory to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_BUF_TOO_SMALL
 *			will be returned).
 * @param index		Index of the directory entry to read, if not negative.
 *			If not found, -ERR_NOT_FOUND will be returned.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_dir_read(handle_t handle, fs_dir_entry_t *buf, size_t size, offset_t index) {
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
	if((ret = vfs_dir_read(obj, kbuf, size, index)) == 0) {
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
int sys_fs_handle_seek(handle_t handle, int action, offset_t offset, offset_t *newp) {
	object_handle_t *obj;
	offset_t new;
	int ret;

	if((ret = object_handle_lookup(curr_proc, handle, -1, &obj)) != 0) {
		return ret;
	}

	if((ret = vfs_handle_seek(obj, action, offset, &new)) == 0 && newp) {
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

	if((ret = vfs_handle_info(obj, &kinfo)) == 0) {
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

	ret = vfs_handle_sync(obj);
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

	ret = vfs_symlink_create(kpath, ktarget);
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

	ret = vfs_symlink_read(kpath, kbuf, size);
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
 * Some filesystem types are read-only by design - when mounting these the
 * FS_MOUNT_RDONLY flag will automatically be set. It may also be set if the
 * device the filesystem resides on is read-only. Mounting multiple filesystems
 * on one directory at a time is not allowed.
 *
 * @param dev		Device path for backing device for mount (can be NULL
 *			if the filesystem does not require a backing device).
 * @param path		Path to directory to mount on.
 * @param type		Name of filesystem type.
 * @param flags		Flags to specify mount behaviour.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_mount(const char *dev, const char *path, const char *type, int flags) {
	char *kdev = NULL, *kpath = NULL, *ktype = NULL;
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

	ret = vfs_mount(kdev, kpath, ktype, flags);
out:
	if(kdev) { kfree(kdev); }
	if(kpath) { kfree(kpath); }
	if(ktype) { kfree(ktype); }
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

	ret = vfs_unmount(kpath);
	kfree(kpath);
	return ret;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to store in.
 * @param size		Size of buffer.
 * @return		0 on success, negative error code on failure. */
int sys_fs_getcwd(char *buf, size_t size) {
	char *kbuf = NULL, *tmp, path[3];
	fs_dir_entry_t *entry;
	vfs_node_t *node;
	size_t len = 0;
	node_id_t id;
	int ret;

	if(!buf || !size) {
		return -ERR_PARAM_INVAL;
	}

	/* Get the working directory. */
	node = curr_proc->ioctx.curr_dir;
	mutex_lock(&node->lock);
	vfs_node_get(node);

	/* Loop through until we reach the root. */
	while(node != curr_proc->ioctx.root_dir) {
		/* Save the current node's ID. Use the mountpoint ID if this is
		 * the root of the mount. */
		id = (node == node->mount->root) ? node->mount->mountpoint->id : node->id;

		/* Get the parent of the node. */
		strcpy(path, "..");
		if((ret = vfs_node_lookup_internal(path, node, false, 0, &node)) != 0) {
			kfree(kbuf);
			return ret;
		} else if(node->type != VFS_NODE_DIR) {
			dprintf("vfs: node %p(%" PRIu64 ") should be a directory but it isn't!\n",
			        node, node->id);
			mutex_unlock(&node->lock);
			vfs_node_release(node);
			kfree(kbuf);
			return -ERR_TYPE_INVAL;
		}

		/* Now try to find the old node in this directory. */
		if((ret = vfs_dir_cache_entries(node)) != 0) {
			mutex_unlock(&node->lock);
			vfs_node_release(node);
			kfree(kbuf);
			return ret;
		}
		entry = NULL;
		RADIX_TREE_FOREACH(&node->dir_entries, iter) {
			entry = radix_tree_entry(iter, fs_dir_entry_t);
			if(entry->id == id) {
				break;
			} else {
				entry = NULL;
			}
		}
		if(!entry) {
			/* Directory has probably been unlinked. */
			mutex_unlock(&node->lock);
			vfs_node_release(node);
			kfree(kbuf);
			return -ERR_NOT_FOUND;
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
	vfs_node_release(node);

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
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		0 on success, negative error code on failure. */
int sys_fs_setcwd(const char *path) {
	vfs_node_t *node;
	char *kpath;
	int ret;

	/* Get the path and look it up. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = vfs_node_lookup(kpath, true, VFS_NODE_DIR, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. Release the node no matter what, as upon success it
	 * is referenced by io_context_setcwd(). */
	ret = io_context_setcwd(&curr_proc->ioctx, node);
	vfs_node_release(node);
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
	vfs_node_t *node;
	char *kpath;
	int ret;

	/* Get the path and look it up. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = vfs_node_lookup(kpath, true, VFS_NODE_DIR, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. Release the node no matter what, as upon success it
	 * is referenced by io_context_setroot(). */
	ret = io_context_setroot(&curr_proc->ioctx, node);
	vfs_node_release(node);
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

	if((ret = vfs_info(kpath, follow, &kinfo)) == 0) {
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

	ret = vfs_unlink(kpath);
	kfree(kpath);
	return ret;
}

int sys_fs_rename(const char *source, const char *dest) {
	return -ERR_NOT_IMPLEMENTED;
}
