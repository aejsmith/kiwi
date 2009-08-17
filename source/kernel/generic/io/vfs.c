/* Kiwi virtual file system (VFS)
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
 * @brief		Virtual file system (VFS).
 *
 * @note		Mount locks should be taken before node locks. If a
 *			node lock is held and it is desired to lock its mount,
 *			you should unlock the node, lock the mount, then relock
 *			the node. If the node lock is taken first, a deadlock
 *			can occur (lock node, attempt to lock mount which
 *			blocks because node is being searched for, search
 *			attempts to lock node, deadlock).
 */

#include <console/kprintf.h>

#include <io/context.h>
#include <io/vfs.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/handle.h>
#include <proc/process.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_VFS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern vfs_type_t ramfs_fs_type;

/** List of all mounts. */
static identifier_t vfs_next_mount_id = 0;
static LIST_DECLARE(vfs_mount_list);
static MUTEX_DECLARE(vfs_mount_lock, 0);

/** List of registered FS types. */
static LIST_DECLARE(vfs_type_list);
static MUTEX_DECLARE(vfs_type_list_lock, 0);

/** Filesystem node slab cache. */
static slab_cache_t *vfs_node_cache;

/** Pointer to mount at root of the filesystem. */
vfs_mount_t *vfs_root_mount = NULL;

static vm_object_ops_t vfs_vm_object_ops;

static int vfs_file_page_flush(vfs_node_t *node, vm_page_t *page);
static identifier_t vfs_dir_entry_get(vfs_node_t *node, const char *name);
static int vfs_symlink_cache_dest(vfs_node_t *node);

#if 0
# pragma mark Filesystem type functions.
#endif

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

	mutex_lock(&vfs_type_list_lock, 0);

	type = vfs_type_lookup_internal(name);
	if(type) {
		refcount_inc(&type->count);
	}

	mutex_unlock(&vfs_type_list_lock);
	return type;
}

/** Register a new filesystem type.
 *
 * Registers a new filesystem type with the VFS.
 *
 * @param type		Pointer to type structure to register.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_type_register(vfs_type_t *type) {
	mutex_lock(&vfs_type_list_lock, 0);

	/* Check if this type already exists. */
	if(vfs_type_lookup_internal(type->name) != NULL) {
		mutex_unlock(&vfs_type_list_lock);
		return -ERR_ALREADY_EXISTS;
	}

	list_init(&type->header);
	list_append(&vfs_type_list, &type->header);

	dprintf("vfs: registered filesystem type %p(%s)\n", type, type->name);
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
	mutex_lock(&vfs_type_list_lock, 0);

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

#if 0
# pragma mark Node functions.
#endif

/** VFS node object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_cache_ctor(void *obj, void *data, int kmflag) {
	vfs_node_t *node = (vfs_node_t *)obj;

	vm_object_init(&node->vobj, &vfs_vm_object_ops);
	list_init(&node->header);
	mutex_init(&node->lock, "vfs_node_lock", 0);
	refcount_set(&node->count, 0);
	avl_tree_init(&node->pages);
	radix_tree_init(&node->dir_entries);
	return 0;
}

/** VFS node object destructor.
 * @param obj		Object to destruct.
 * @param data		Cache data (unused). */
static void vfs_node_cache_dtor(void *obj, void *data) {
	vfs_node_t *node = (vfs_node_t *)obj;

	vm_object_destroy(&node->vobj);
}

/** Allocate a node structure and set one reference on it.
 * @note		Does not attach to the mount.
 * @param mount		Mount that the node resides on.
 * @param mmflag	Allocation flags.
 * @return		Pointer to node on success, NULL on failure (always
 *			succeeds if MM_SLEEP is specified). */
static vfs_node_t *vfs_node_alloc(vfs_mount_t *mount, int mmflag) {
	vfs_node_t *node;

	node = slab_cache_alloc(vfs_node_cache, mmflag);
	if(node == NULL) {
		return NULL;
	}

	node->id = 0;
	node->mount = mount;
	node->flags = 0;
	node->type = VFS_NODE_FILE;
	node->size = 0;
	node->link_dest = NULL;
	node->mounted = NULL;

	refcount_inc(&node->count);
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

			/* Flush the page data. See function documentation about how
			 * failure is handled. */
			if((err = vfs_file_page_flush(node, page)) != 0) {
				if(destroy) {
					return err;
				}
				ret = err;
			}

			/* Destroy the page if required. */
			if(destroy) {
				avl_tree_remove(&node->pages, (key_t)page->offset);
				vm_page_free(page);
			}
		}
	}

	/* Flush node metadata. */
	if(node->mount && node->mount->type->node_flush) {
		if((err = node->mount->type->node_flush(node)) != 0) {
			ret = err;
		}
	}
	return ret;
}

/** Flush changes to a node and free it.
 * @note		Never call this function. Use vfs_node_release().
 * @note		Mount lock (if there is a mount) and node lock must be
 *			held.
 * @param node		Node to free. Should be unused (zero reference count).
 * @return		0 on success, negative error code on failure (this can
 *			happen, for example, if an error occurs flushing the
 *			node data). */
static int vfs_node_free(vfs_node_t *node) {
	int ret;

	assert(refcount_get(&node->count) == 0);

	/* Flush cached data and metadata. */
	if((ret = vfs_node_flush(node, true)) != 0) {
		kprintf(LOG_WARN, "vfs: warning: failed to flush data for %p(%" PRId32 ":%" PRId32 ") (%d)\n",
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
		list_remove(&node->header);
		if(node->mount->type->node_free) {
			node->mount->type->node_free(node);
		}

		mutex_unlock(&node->mount->lock);
	}

	/* Free up other cached bits of data.*/
	radix_tree_clear(&node->dir_entries, kfree);
	if(node->link_dest) {
		kfree(node->link_dest);
	}

	dprintf("vfs: freed node %p(%" PRId32 ":%" PRId32 ")\n", node,
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
	identifier_t id;
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
		mutex_lock(&node->lock, 0);
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

			dprintf("vfs: following symbolic link %" PRId32 ":%" PRId32 " to %s\n",
			        node->mount->id, node->id, node->link_dest);

			/* Duplicate the link destination as the lookup needs
			 * to modify it. */
			link = kstrdup(node->link_dest, MM_SLEEP);

			/* Move up to the parent node. The previous iteration
			 * of the loop left a reference on previous for us. */
			tmp = node; node = prev; prev = tmp;
			mutex_unlock(&prev->lock);
			mutex_lock(&node->lock, 0);

			/* Recurse to find the link destination. The check
			 * above ensures we do not infinitely recurse. */
			if((ret = vfs_node_lookup_internal(link, node, true, nest, &node)) != 0) {
				vfs_node_release(prev);
				kfree(link);
				return ret;
			}

			dprintf("vfs: followed %s to %" PRId32 ":%" PRId32 "\n",
			        prev->link_dest, node->mount->id, node->id);
			kfree(link);

			vfs_node_release(prev);
		} else if(node->type == VFS_NODE_SYMLINK) {
			/* The new node is a symbolic link but we do not want
			 * to follow it. We must release the previous node. */
			assert(prev != node);
			vfs_node_release(prev);
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
				mutex_lock(&node->lock, 0);
			}
		}

		/* Look up this name within the directory entry cache. */
		if((id = vfs_dir_entry_get(node, tok)) < 0) {
			mutex_unlock(&node->lock);
			vfs_node_release(node);
			return (int)id;
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
		mutex_lock(&mount->lock, 0);

		prev = node;

		/* Check if the node is cached in the mount. */
		dprintf("vfs: looking for node %" PRId32 " in cache for mount %" PRId32 " (%s)\n", id, mount->id, tok);
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
				mutex_lock(&node->lock, 0);
				mutex_unlock(&mount->lock);
			} else {
				/* Reference the node and lock it, and move it
				 * to the used list if it was unused before. */
				if(refcount_inc(&node->count) == 1) {
					list_append(&mount->used_nodes, &node->header);
				}

				mutex_lock(&node->lock, 0);
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

			/* Allocate a new node structure. */
			node = vfs_node_alloc(mount, MM_SLEEP);

			/* Request the node from the filesystem. */
			if((ret = mount->type->node_get(node, id)) != 0) {
				mutex_unlock(&mount->lock);
				slab_cache_free(vfs_node_cache, node);
				vfs_node_release(prev);
				return ret;
			}

			/* Attach the node to the node tree and used list. */
			avl_tree_insert(&mount->nodes, (key_t)id, node, NULL);
			list_append(&mount->used_nodes, &node->header);
			mutex_unlock(&mount->lock);
		}

		/* Do not release the previous node if the current node is a
		 * symbolic link, as the symbolic link code requires it. */
		if(node->type != VFS_NODE_SYMLINK) {
			vfs_node_release(prev);
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
 * @param nodep		Where to store pointer to node found (referenced,
 *			unlocked).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_lookup(const char *path, bool follow, vfs_node_t **nodep) {
	vfs_node_t *node = NULL;
	char *dup;
	int ret;

	if(!path || !path[0] || !nodep) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&curr_proc->ioctx.lock, 0);

	/* Start from the current directory if the path is relative. */
	if(path[0] != '/') {
		assert(curr_proc->ioctx.curr_dir);

		node = curr_proc->ioctx.curr_dir;
		mutex_lock(&node->lock, 0);
		vfs_node_get(node);
	}

	/* Duplicate path so that vfs_node_lookup_internal() can modify it. */
	dup = kstrdup(path, MM_SLEEP);

	/* Look up the path string. */
	if((ret = vfs_node_lookup_internal(dup, node, follow, 0, &node)) == 0) {
		mutex_unlock(&node->lock);
		*nodep = node;
	}

	mutex_unlock(&curr_proc->ioctx.lock);
	kfree(dup);
	return ret;
}

/** Increase the reference count of a node.
 *
 * Increases the reference count of a filesystem node. This function should not
 * be used on nodes with a zero reference count, as nothing outside the VFS
 * should access a node with a zero reference count.
 *
 * @param node		Node to increase reference count of.
 */
void vfs_node_get(vfs_node_t *node) {
	int val = refcount_inc(&node->count);

	if(val == 1) {
		fatal("Called vfs_node_get on unused node %" PRId32 ":%" PRId32,
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
	int ret;

	/* Acquire mount lock then node lock. See note in file header about
	 * locking order. */
	if(node->mount) {
		mutex_lock(&node->mount->lock, 0);
	}
	mutex_lock(&node->lock, 0);

	if(refcount_dec(&node->count) == 0) {
		assert(!node->mounted);

		/* Node has no references remaining, move it to its mount's
		 * unused list if it has a mount. If the node is not attached
		 * to anything, then destroy it immediately. */
		if(node->mount && !(node->flags & VFS_NODE_REMOVED)) {
			list_append(&node->mount->unused_nodes, &node->header);
			dprintf("vfs: transferred node %p to unused list (mount: %p)\n", node, node->mount);
			mutex_unlock(&node->lock);
			mutex_unlock(&node->mount->lock);
		} else {
			/* This shouldn't fail - the only things that can fail
			 * in vfs_node_free() are cache flushing and metadata
			 * flushing. Since this node has no source to flush to,
			 * or has been removed, this should not fail. */
			if((ret = vfs_node_free(node)) != 0) {
				fatal("Could not destroy %s (%d)",
				      (node->mount) ? "removed node" : "node with no mount",
				      ret);
			}
		}
	} else {
		mutex_unlock(&node->lock);
		if(node->mount) {
			mutex_unlock(&node->mount->lock);
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
	identifier_t id;
	int ret;

	assert(!node->mount);

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	dprintf("vfs: create(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Check for disallowed names. */
	if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                ret = -ERR_ALREADY_EXISTS;
		goto out;
        }

	/* Look up the parent node. */
	if((ret = vfs_node_lookup(dir, true, &parent)) != 0) {
		goto out;
	}

	mutex_lock(&parent->mount->lock, 0);
	mutex_lock(&parent->lock, 0);

	/* Ensure that we have a directory, are on a writeable filesystem, and
	 * that the FS supports node creation. */
	if(parent->type != VFS_NODE_DIR) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	} else if(VFS_NODE_IS_RDONLY(parent)) {
		ret = -ERR_READ_ONLY;
		goto out;
	} else if(!parent->mount->type->node_create) {
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	}

	/* Check if the name we're creating already exists. This will populate
	 * the entry cache so it will be OK to add the node to it. */
	if((id = vfs_dir_entry_get(parent, name)) != -ERR_NOT_FOUND) {
		ret = (id >= 0) ? -ERR_ALREADY_EXISTS : (int)id;
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	node->mount = parent->mount;
	if((ret = node->mount->type->node_create(parent, name, node)) != 0) {
		goto out;
	}

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&node->mount->nodes, (key_t)node->id, node, NULL);
	list_append(&node->mount->used_nodes, &node->header);

	/* Insert the node into the parent's entry cache. */
	vfs_dir_entry_add(parent, node->id, name);

	dprintf("vfs: created %s (node: %" PRId32 ":%" PRId32 ", parent: %" PRId32 ":%" PRId32 ")\n",
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
 *
 * Gets information about a filesystem node and stores it in the provided
 * structure.
 *
 * @param node		Node to get information for.
 * @param infop		Structure to store information in.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_info(vfs_node_t *node, vfs_info_t *infop) {
	return -ERR_NOT_IMPLEMENTED;
}

#if 0
# pragma mark Regular file operations.
#endif

/** Get a page from a file's cache.
 * @note		Should not be passed both mappingp and pagep.
 * @param node		Node to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in from
 *			the filesystem if it is not in the cache, it will only
 *			allocate a page - useful if the caller is about to
 *			overwrite the page data.
 * @param pagep		Where to store pointer to page structure.
 * @param mappingp	Where to store address of virtual mapping.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_page_get_internal(vfs_node_t *node, offset_t offset, bool overwrite, vm_page_t **pagep, void **mappingp) {
	void *mapping = NULL;
	vm_page_t *page;
	int ret;

	assert(node->type == VFS_NODE_FILE);
	assert((pagep && !mappingp) || (mappingp && !pagep));

	mutex_lock(&node->lock, 0);

	/* Check whether it is within the size of the node. */
	if((file_size_t)offset >= node->size) {
		return -ERR_NOT_FOUND;
	}

	/* Check if we have it cached. */
	if((page = avl_tree_lookup(&node->pages, (key_t)offset))) {
		refcount_inc(&page->count);
		mutex_unlock(&node->lock);

		/* Map it in if required. */
		if(mappingp) {
			*mappingp = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
		} else {
			*pagep = page;
		}

		dprintf("vfs: retreived cached page 0x%" PRIpp " from offset %" PRId64 " in %p\n",
		        page->addr, offset, node);
		return 0;
	}

	/* Need to read the page in. If a read operation is provided and we're
	 * not about to overwrite, read the page data into an unzeroed page.
	 * If we're about to overwrite, allocate a page without zeroing.
	 * Otherwise, allocate a zeroed page. */
	if(!overwrite) {
		if(node->mount && node->mount->type->page_read) {
			page = vm_page_alloc(MM_SLEEP);
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);

			ret = node->mount->type->page_read(node, mapping, offset, false);
			if(ret != 0) {
				page_phys_unmap(mapping, PAGE_SIZE);
				refcount_dec(&page->count);
				vm_page_free(page);
				mutex_unlock(&node->lock);
				return ret;
			}
		} else {
			page = vm_page_alloc(MM_SLEEP | PM_ZERO);
		}
	} else {
		page = vm_page_alloc(MM_SLEEP);
	}

	/* Cache the page and unlock. */
	page->offset = offset;
	avl_tree_insert(&node->pages, (key_t)offset, page, NULL);
	mutex_unlock(&node->lock);

	dprintf("vfs: cached new page 0x%" PRIpp " at offset %" PRId64 " in %p\n",
	        page->addr, offset, node);

	/* Map it in if required. If we had to read page data in, reuse the
	 * mapping created for that. */
	if(mappingp) {
		if(!mapping) {
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
		}
		*mappingp = mapping;
	} else {
		if(mapping) {
			page_phys_unmap(mapping, PAGE_SIZE);
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

	mutex_lock(&node->lock, 0);

	if(!(page = avl_tree_lookup(&node->pages, (key_t)offset))) {
		fatal("Tried to release page that isn't cached");
	}

	dprintf("vfs: released page 0x%" PRIpp " at offset %" PRId64 " in %p\n",
	        page->addr, offset, node);

	/* Mark as dirty if requested. */
	if(dirty) {
		page->flags |= VM_PAGE_DIRTY;
	}

	/* Decrease the reference count. If it reaches 0, and the page is
	 * outside the node's size (i.e. file has been truncated with pages in
	 * use), discard it. */
	if(refcount_dec(&page->count) == 0 && (file_size_t)offset >= node->size) {
		avl_tree_remove(&node->pages, (key_t)offset);
		vm_page_free(page);
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
	if((file_size_t)page->offset >= node->size || !(page->flags & VM_PAGE_DIRTY)) {
		return 0;
	}

	if(node->mount && node->mount->type->page_flush) {
		mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);

		if((ret = node->mount->type->page_flush(node, mapping, page->offset, false)) == 0) {
			/* Clear dirty flag if the page reference count is
			 * zero. This is because a page may be mapped into an
			 * address space as read-write, but has not yet been
			 * written to. */
			if(refcount_get(&page->count) == 0) {
				page->flags &= ~VM_PAGE_DIRTY;
			}
		}

		page_phys_unmap(mapping, PAGE_SIZE);
	}

	return ret;
}

/** Increase the reference count of a file VM object.
 * @param obj		Object to reference.
 * @param region	Region referencing the object. */
static void vfs_vm_object_get(vm_object_t *obj, vm_region_t *region) {
	vfs_node_get((vfs_node_t *)obj);
}

/** Decrease the reference count of a file VM object.
 * @param obj		Object to decrease count of.
 * @param region	Region to detach. */
static void vfs_vm_object_release(vm_object_t *obj, vm_region_t *region) {
	vfs_node_release((vfs_node_t *)obj);
}

/** Get a page from a file VM object.
 * @param obj		Object to get page from.
 * @param offset	Offset of page to get.
 * @param pagep		Where to store pointer to page structure.
 * @return		0 on success, negative error code on failure. */
static int vfs_vm_object_page_get(vm_object_t *obj, offset_t offset, vm_page_t **pagep) {
	return vfs_file_page_get_internal((vfs_node_t *)obj, offset, false, pagep, NULL);
}

/** Release a page from a file VM object.
 * @param obj		Object that the page belongs to.
 * @param offset	Offset of page to release.
 * @param paddr		Physical address of page that was unmapped. */
static void vfs_vm_object_page_release(vm_object_t *obj, offset_t offset, phys_ptr_t paddr) {
	vfs_file_page_release_internal((vfs_node_t *)obj, offset, false);
}

/** File VM object operations. */
static vm_object_ops_t vfs_vm_object_ops = {
	.get = vfs_vm_object_get,
	.release = vfs_vm_object_release,
	.page_get = vfs_vm_object_page_get,
	.page_release = vfs_vm_object_page_release,
};

/** Get and map a page from a file's data cache.
 * @param node		Node to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in from
 *			the filesystem if it is not in the cache - useful if
 *			the caller is about to overwrite the page data.
 * @param addrp		Where to store address of mapping.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_page_map(vfs_node_t *node, offset_t offset, bool overwrite, void **addrp) {
	return vfs_file_page_get_internal(node, offset, overwrite, NULL, addrp);
}

/** Unmap and release a page from a node's data cache.
 * @param node		Node to release page in.
 * @param mapping	Address of mapping.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied. */
static void vfs_file_page_unmap(vfs_node_t *node, void *mapping, offset_t offset, bool dirty) {
	page_phys_unmap(mapping, PAGE_SIZE);
	vfs_file_page_release_internal(node, offset, dirty);
}

/** Create a file in the file system.
 *
 * Creates a new regular file in the filesystem.
 *
 * @param path		Path to file to create.
 * @param nodep		Where to store pointer to node for file (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_create(const char *path, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, MM_SLEEP);
	node->type = VFS_NODE_FILE;

	/* Call the common creation code. */
	if((ret = vfs_node_create(path, node)) != 0) {
		vfs_node_release(node);
		return ret;
	}

	/* Store a pointer to the node or release it if it is not wanted. */
	if(nodep) {
		*nodep = node;
	} else {
		vfs_node_release(node);
	}
	return 0;
}

/** Create a special node backed by a chunk of memory.
 *
 * Creates a special VFS node structure that is backed by the specified chunk
 * of memory. This is useful to pass data stored in memory to code that expects
 * to be operating on filesystem nodes, such as the program loader.
 *
 * When the node is created, the data in the given memory area is duplicated
 * into the node's data cache, so updates to the memory area after this
 * function has be called will not show on reads from the node. Similarly,
 * writes to the node will not be written back to the memory area.
 *
 * The node is not attached anywhere in the filesystem, and therefore once its
 * reference count reaches 0, it will be immediately destroyed.
 *
 * @param buf		Pointer to memory area to use.
 * @param size		Size of memory area.
 * @param nodep		Where to store pointer to created node.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_from_memory(const void *buf, size_t size, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	if(!buf || !size || !nodep) {
		return -ERR_PARAM_INVAL;
	}

	node = vfs_node_alloc(NULL, MM_SLEEP);
	node->type = VFS_NODE_FILE;
	node->size = size;

	/* Write the data into the node. */
	if((ret = vfs_file_write(node, buf, size, 0, NULL)) != 0) {
		vfs_node_release(node);
		return ret;
	}

	*nodep = node;
	return 0;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer.
 *
 * @param node		Node to read from (must be VFS_NODE_FILE).
 * @param buf		Buffer to read data into. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to read.
 * @param offset	Offset within the file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_read(vfs_node_t *node, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	int ret;

	if(!node || !buf || offset < 0) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Check if the node is a suitable type. */
	if(node->type != VFS_NODE_FILE) {
		ret = -ERR_TYPE_INVAL;
		mutex_unlock(&node->lock);
		goto out;
	}

	/* Ensure that we do not go pass the end of the node. */
	if(offset > (offset_t)node->size) {
		ret = 0;
		mutex_unlock(&node->lock);
		goto out;
	} else if((offset + (offset_t)count) > (offset_t)node->size) {
		count = (size_t)((offset_t)node->size - offset);
	}

	/* It is not an error to pass a zero count, just return silently if
	 * this happens, however do it after all the other checks so we do
	 * return errors where appropriate. */
	if(count == 0) {
		ret = 0;
		mutex_unlock(&node->lock);
		goto out;
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
		if((ret = vfs_file_page_map(node, start, false, &mapping)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(buf, mapping + (offset % PAGE_SIZE), size);
		vfs_file_page_unmap(node, mapping, start, false);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, false, &mapping)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, PAGE_SIZE);
		vfs_file_page_unmap(node, mapping, start, false);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vfs_file_page_map(node, start, false, &mapping)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, count);
		vfs_file_page_unmap(node, mapping, start, false);
		total += count;
	}

	dprintf("vfs: read %zu bytes from offset 0x%" PRIx64 " in %p(%" PRId32 ":%" PRId32 ")\n",
	        total, offset, node, (node->mount) ? node->mount->id : -1, node->id);
	ret = 0;
out:
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Write to a file.
 *
 * Writes data from a buffer into a file.
 *
 * @param node		Node to write to (must be VFS_NODE_FILE).
 * @param buf		Buffer to write data from. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to write.
 * @param offset	Offset within the file to write to.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_write(vfs_node_t *node, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	int ret;

	if(!node || !buf || offset < 0) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Check if the node is a suitable type, and if it's on a writeable
	 * filesystem. */
	if(node->type != VFS_NODE_FILE) {
		ret = -ERR_TYPE_INVAL;
		mutex_unlock(&node->lock);
		goto out;
	} else if(VFS_NODE_IS_RDONLY(node)) {
		ret = -ERR_READ_ONLY;
		mutex_unlock(&node->lock);
		goto out;
	}

	/* Attempt to resize the node if necessary. */
	if((offset + (offset_t)count) > (offset_t)node->size) {
		/* If the resize operation is not provided, we can only write
		 * within the space that we have. */
		if(!node->mount || !node->mount->type->file_resize) {
			if(offset > (offset_t)node->size) {
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
		if((ret = vfs_file_page_map(node, start, false, &mapping)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(mapping + (offset % PAGE_SIZE), buf, size);
		vfs_file_page_unmap(node, mapping, start, true);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. We pass the overwrite parameter as true to
	 * vfs_file_page_map() here, so that if the page is not in the cache,
	 * its data will not be read in - we're about to overwrite it, so it
	 * would not be necessary. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vfs_file_page_map(node, start, true, &mapping)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, PAGE_SIZE);
		vfs_file_page_unmap(node, mapping, start, true);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vfs_file_page_map(node, start, false, &mapping)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, count);
		vfs_file_page_unmap(node, mapping, start, true);
		total += count;
	}

	dprintf("vfs: wrote %zu bytes to offset 0x%" PRIx64 " in %p(%" PRId32 ":%" PRId32 ")\n",
	        total, offset, node, (node->mount) ? node->mount->id : -1, node->id);
	ret = 0;
out:
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
 * @param node		Node to resize.
 * @param size		New size of the file.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_file_resize(vfs_node_t *node, file_size_t size) {
	vm_page_t *page;
	int ret;

	if(!node) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Check if the node is a suitable type and if resizing is allowed. */
	if(node->type != VFS_NODE_FILE) {
		mutex_unlock(&node->lock);
		return -ERR_TYPE_INVAL;
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
				vm_page_free(page);
			}
		}
	}

	node->size = (file_size_t)size;
	mutex_unlock(&node->lock);
	return 0;
}

/** Closes a handle to a regular file.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int vfs_file_handle_close(handle_info_t *info) {
	vfs_handle_t *file = info->data;

	if(file->node->mount->type->file_close) {
		file->node->mount->type->file_close(file->node);
	}

	vfs_node_release(file->node);
	return 0;
}

/** File handle operations. */
static handle_type_t vfs_file_handle_type = {
	.id = HANDLE_TYPE_FILE,
	.close = vfs_file_handle_close,
};

#if 0
# pragma mark Directory operations.
#endif

/** Populate a directory's entry cache if it is empty.
 * @param node		Node of directory.
 * @return		0 on success, negative error code on failure. */
static int vfs_dir_cache_entries(vfs_node_t *node) {
	int ret = 0;

	/* If the radix tree is empty, we consider the cache to be empty - even
	 * if the directory is empty, the cache should at least have '.' and
	 * '..' entries. */
	if(radix_tree_empty(&node->dir_entries)) {
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
 * @return		ID of node on success, negative error code on
 *			failure. */
static identifier_t vfs_dir_entry_get(vfs_node_t *node, const char *name) {
	vfs_dir_entry_t *entry;
	int ret;

	assert(node->type == VFS_NODE_DIR);
	assert(node->mount);

	/* Populate the entry cache if it is empty. */
	if((ret = vfs_dir_cache_entries(node)) != 0) {
		return ret;
	}

	/* Look up the entry. */
	entry = radix_tree_lookup(&node->dir_entries, name);
	return (entry == NULL) ? -ERR_NOT_FOUND : entry->id;
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
void vfs_dir_entry_add(vfs_node_t *node, identifier_t id, const char *name) {
	vfs_dir_entry_t *entry;
	size_t len;

	/* Work out the length we need. */
	len = sizeof(vfs_dir_entry_t) + strlen(name) + 1;

	/* Allocate the buffer for it and fill it in. */
	entry = kmalloc(len, MM_SLEEP);
	entry->length = len;
	entry->id = id;
	strcpy(entry->name, name);

	/* Insert into the cache. */
	radix_tree_insert(&node->dir_entries, name, entry);

	/* Increase count. */
	node->size++;
}

/** Remove an entry from a directory's entry cache.
 * @param node		Node to remove entry from.
 * @param name		Name of entry to remove. */
static void vfs_dir_entry_remove(vfs_node_t *node, const char *name) {
	radix_tree_remove(&node->dir_entries, name, kfree);
	node->size--;
}

/** Create a directory in the file system.
 *
 * Creates a new directory in the filesystem.
 *
 * @param path		Path to directory to create.
 * @param nodep		Where to store pointer to node for directory (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_dir_create(const char *path, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, MM_SLEEP);
	node->type = VFS_NODE_DIR;

	/* Call the common creation code. */
	if((ret = vfs_node_create(path, node)) != 0) {
		vfs_node_release(node);
		return ret;
	}

	/* Store a pointer to the node or release it if it is not wanted. */
	if(nodep) {
		*nodep = node;
	} else {
		vfs_node_release(node);
	}
	return 0;
}

/** Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed.
 *
 * @param node		Node to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_PARAM_INVAL
 *			will be returned).
 * @param index		Number of the directory entry to read (if not found,
 *			-ERR_NOT_FOUND will be returned).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_dir_read(vfs_node_t *node, vfs_dir_entry_t *buf, size_t size, offset_t index) {
	vfs_dir_entry_t *entry = NULL;
	vfs_node_t *child;
	offset_t i = 0;
	int ret;

	if(!node || !buf || !size || index < 0) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Ensure that the node is a directory. */
	if(node->type != VFS_NODE_DIR) {
		mutex_unlock(&node->lock);
		return -ERR_TYPE_INVAL;
	}

	/* Cache the directory entries if we do not already have them, and
	 * check that the index is valid. */
	if((ret = vfs_dir_cache_entries(node)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	} else if(index >= (offset_t)node->size) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_FOUND;
	}

	/* Iterate through the tree to find the entry. */
	RADIX_TREE_FOREACH(&node->dir_entries, iter) {
		if(i++ == index) {
			entry = radix_tree_entry(iter, vfs_dir_entry_t);
			break;
		}
	}

	/* We should have it because we checked against size. */
	if(!entry) {
		fatal("Entry %" PRId64 " within size but not found (%p)", index, node);
	}

	/* Check that the buffer is large enough. */
	if(size < entry->length) {
		mutex_unlock(&node->lock);
		return -ERR_NOT_FOUND;
	}

	/* Copy it to the buffer. */
	memcpy(buf, entry, entry->length);

	mutex_unlock(&node->lock);
	mutex_lock(&node->mount->lock, 0);
	mutex_lock(&node->lock, 0);

	/* Fix up the entry. */
	if(node == node->mount->root && strcmp(entry->name, "..") == 0) {
		/* This is the '..' entry, and the node is the root of its
		 * mount. Change the node ID to be the ID of the mountpoint,
		 * if any. */
		if(node->mount->mountpoint) {
			mutex_lock(&node->mount->mountpoint->lock, 0);
			if((buf->id = vfs_dir_entry_get(node->mount->mountpoint, "..")) < 0) {
				mutex_unlock(&node->mount->mountpoint->lock);
				mutex_unlock(&node->mount->lock);
				mutex_unlock(&node->lock);
				return (int)buf->id;
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
				mutex_lock(&child->lock, 0);
				if(child->type == VFS_NODE_DIR && child->mounted) {
					buf->id = child->mounted->root->id;
				}
				mutex_unlock(&child->lock);
			}
		}
	}

	mutex_unlock(&node->mount->lock);
	mutex_unlock(&node->lock);
	return 0;
}

/** Closes a handle to a directory
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int vfs_dir_handle_close(handle_info_t *info) {
	vfs_handle_t *dir = info->data;

	if(dir->node->mount->type->dir_close) {
		dir->node->mount->type->dir_close(dir->node);
	}

	vfs_node_release(dir->node);
	return 0;
}

/** Directory handle operations. */
static handle_type_t vfs_dir_handle_type = {
	.id = HANDLE_TYPE_DIR,
	.close = vfs_dir_handle_close,
};

#if 0
# pragma mark Symbolic link operations.
#endif

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
 *
 * Creates a new symbolic link in the filesystem.
 *
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @param nodep		Where to store pointer to node for link (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_symlink_create(const char *path, const char *target, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	/* Allocate a new node and fill in some details. */
	node = vfs_node_alloc(NULL, MM_SLEEP);
	node->type = VFS_NODE_SYMLINK;
	node->link_dest = kstrdup(target, MM_SLEEP);

	/* Call the common creation code. */
	if((ret = vfs_node_create(path, node)) != 0) {
		/* This will free the link destination. */
		vfs_node_release(node);
		return ret;
	}

	/* Store a pointer to the node or release it if it is not wanted. */
	if(nodep) {
		*nodep = node;
	} else {
		vfs_node_release(node);
	}
	return 0;
}

/** Get the destination of a symbolic link.
 *
 * Reads the destination of a symbolic link into a buffer. A NULL byte will
 * be placed at the end of the buffer, unless the buffer is too small.
 *
 * @param node		Node of symbolic link.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer (destination will be truncated if buffer
 *			is too small).
 *
 * @return		Number of bytes read on success, negative error code
 *			on failure.
 */
int vfs_symlink_read(vfs_node_t *node, char *buf, size_t size) {
	size_t len;
	int ret;

	if(!node || !buf || !size) {
		return -ERR_PARAM_INVAL;
	} else if(node->type != VFS_NODE_SYMLINK) {
		return -ERR_TYPE_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Ensure destination is cached. */
	if((ret = vfs_symlink_cache_dest(node)) != 0) {
		mutex_unlock(&node->lock);
		return ret;
	}

	len = ((len = strlen(node->link_dest) + 1) > size) ? size : len;
	memcpy(buf, node->link_dest, len);
	mutex_unlock(&node->lock);
	return (int)len;
}

#if 0
# pragma mark Mount operations.
#endif

/** Look up a mount by ID.
 * @note		Does not take the mount lock.
 * @param id		ID of mount to look up.
 * @return		Pointer to mount if found, NULL if not. */
static vfs_mount_t *vfs_mount_lookup(identifier_t id) {
	vfs_mount_t *mount;

	LIST_FOREACH(&vfs_mount_list, iter) {
		mount = list_entry(iter, vfs_mount_t, header);

		if(mount->id == (identifier_t)id) {
			return mount;
		}
	}

	return NULL;
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Some filesystem types are read-only by design - when mounting these the
 * VFS_MOUNT_RDONLY flag will automatically be set. It may also be set if the
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
int vfs_mount(const char *dev, const char *path, const char *type, int flags) {
	vfs_mount_t *mount = NULL;
	vfs_node_t *node = NULL;
	int ret;

	if(!path || !type) {
		return -ERR_PARAM_INVAL;
	}

	/* Lock the mount lock across the entire operation, so that only one
	 * mount can take place at a time. */
	mutex_lock(&vfs_mount_lock, 0);

	/* If the root filesystem is not yet mounted, the only place we can
	 * mount is '/'. */
	if(!vfs_root_mount) {
		if(strcmp(path, "/") != 0) {
			ret = -ERR_NOT_FOUND;
			goto fail;
		}
	} else {
		/* Look up the destination directory. */
		if((ret = vfs_node_lookup(path, true, &node)) != 0) {
			goto fail;
		}

		mutex_lock(&node->lock, 0);

		/* Check that the node is a directory, and that it is not being
		 * used as a mount point already. */
		if(node->type != VFS_NODE_DIR) {
			ret = -ERR_TYPE_INVAL;
			goto fail;
		} else if(node->mount->root == node) {
			ret = -ERR_IN_USE;
			goto fail;
		}
	}

	/* Initialize the mount structure. */
	mount = kmalloc(sizeof(vfs_mount_t), MM_SLEEP);
	list_init(&mount->header);
	list_init(&mount->used_nodes);
	list_init(&mount->unused_nodes);
	avl_tree_init(&mount->nodes);
	mutex_init(&mount->lock, "vfs_mount_lock", 0);
	mount->type = NULL;
	mount->root = NULL;
	mount->flags = NULL;
	mount->mountpoint = node;

	/* Allocate a mount ID. */
	if(vfs_next_mount_id == INT32_MAX) {
		ret = -ERR_NO_SPACE;
		goto fail;
	}
	mount->id = vfs_next_mount_id++;

	/* Look up the filesystem type. */
	mount->type = vfs_type_lookup(type);
	if(mount->type == NULL) {
		ret = -ERR_PARAM_INVAL;
		goto fail;
	}

	/* If the type is read-only, set read-only in the mount flags. */
	if(mount->type->flags & VFS_TYPE_RDONLY) {
		mount->flags |= VFS_MOUNT_RDONLY;
	}

	/* Create the root node for the filesystem. */
	mount->root = vfs_node_alloc(mount, MM_SLEEP);
	mount->root->type = VFS_NODE_DIR;

	/* Call the filesystem's mount operation. */
	if(mount->type->mount) {
		ret = mount->type->mount(mount);
		if(ret != 0) {
			goto fail;
		}
	}

	/* Put the root node into the node tree/used list. */
	avl_tree_insert(&mount->nodes, (key_t)mount->root->id, mount->root, NULL);
	list_append(&mount->used_nodes, &mount->root->header);

	/* Make the mount point point to the new mount. */
	if(mount->mountpoint) {
		mount->mountpoint->mounted = mount;
		mutex_unlock(&mount->mountpoint->lock);
	}

	/* Store mount in mounts list and unlock the mount lock. */
	list_append(&vfs_mount_list, &mount->header);
	if(!vfs_root_mount) {
		vfs_root_mount = mount;
	}
	mutex_unlock(&vfs_mount_lock);

	dprintf("vfs: mounted %s on %s (mount: %p:%" PRId32 ", root: %p, device: %s)\n",
	        mount->type->name, path, mount, mount->id, mount->root,
	        (dev) ? dev : "<none>");
	return 0;
fail:
	if(mount) {
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
	return -ERR_NOT_IMPLEMENTED;
}

#if 0
# pragma mark Other functions.
#endif

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
	char *dir, *name;
	int ret;

	/* Split path into directory/name. */
	dir = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);

	dprintf("vfs: unlink(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	if((ret = vfs_node_lookup(dir, true, &parent)) != 0) {
		goto out;
	} else if((ret = vfs_node_lookup(path, false, &node)) != 0) {
		goto out;
	}

	mutex_lock(&parent->lock, 0);
	mutex_lock(&node->lock, 0);

	/* If looking up the node succeeded, the parent must be a directory. */
	assert(parent->type == VFS_NODE_DIR);

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

#if 0
# pragma mark Debugger commands.
#endif

/** Print a list of mounts.
 *
 * Prints out a list of all mounted filesystems.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		Always returns KDBG_OK.
 */
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

		kprintf(LOG_NONE, "%-5" PRId32 " %-5d %-10s %-18p %-18p %-18p\n",
		        mount->id, mount->flags, (mount->type) ? mount->type->name : "invalid",
		        mount->data, mount->root, mount->mountpoint);
	}

	return KDBG_OK;
}

/** Print a list of nodes.
 *
 * Prints out a list of nodes on a mount.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
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
	if(!(mount = vfs_mount_lookup((identifier_t)id))) {
		kprintf(LOG_NONE, "Unknown mount ID %" PRId32 ".\n", id);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID       Flags Count Type Size         Mount\n");
	kprintf(LOG_NONE, "==       ===== ===== ==== ====         =====\n");

	if(argc == 3) {
		list = (strcmp(argv[1], "--unused") == 0) ? &mount->unused_nodes : &mount->used_nodes;

		LIST_FOREACH(list, iter) {
			node = list_entry(iter, vfs_node_t, header);

			kprintf(LOG_NONE, "%-8" PRId32 " %-5d %-5d %-4d %-12" PRIu64 " %p\n",
			        node->id, node->flags, refcount_get(&node->count),
			        node->type, node->size, node->mount);
		}
	} else {
		AVL_TREE_FOREACH(&mount->nodes, iter) {
			node = avl_tree_entry(iter, vfs_node_t);

			kprintf(LOG_NONE, "%-8" PRId32 " %-5d %-5d %-4d %-12" PRIu64 " %p\n",
			        node->id, node->flags, refcount_get(&node->count),
			        node->type, node->size, node->mount);
		}
	}
	return KDBG_FAIL;
}

/** Print information about a node.
 *
 * Prints out information about a node on the filesystem.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_vnode(int argc, char **argv) {
	vfs_dir_entry_t *entry;
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
		if(!(mount = vfs_mount_lookup((identifier_t)val))) {
			kprintf(LOG_NONE, "Unknown mount ID %" PRId32 ".\n", val);
			return KDBG_FAIL;
		}

		/* Get the node ID and search for it. */
		if(kdbg_parse_expression(argv[2], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}
		if(!(node = avl_tree_lookup(&mount->nodes, (key_t)val))) {
			kprintf(LOG_NONE, "Unknown node ID %" PRId32 ".\n", val);
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
	kprintf(LOG_NONE, "Node %p(%" PRIu32 ":%" PRIu32 ")\n", node,
	        (node->mount) ? node->mount->id : -1, node->id);
	kprintf(LOG_NONE, "=================================================\n");

	kprintf(LOG_NONE, "Count:        %d\n", refcount_get(&node->count));
	kprintf(LOG_NONE, "Mount:        %p\n", node->mount);
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
	if(node->type == VFS_NODE_DIR && node->mounted) {
		kprintf(LOG_NONE, "Mounted:      %p(%" PRId32 ")\n", node->mounted,
		        node->mounted->id);
	}

	/* If it is a directory, print out a list of cached entries. If it is
	 * a file, print out a list of cached pages. */
	if(node->type == VFS_NODE_DIR) {
		kprintf(LOG_NONE, "\nCached directory entries:\n");

		RADIX_TREE_FOREACH(&node->dir_entries, iter) {
			entry = radix_tree_entry(iter, vfs_dir_entry_t);

			kprintf(LOG_NONE, "  Entry %p - %" PRId32 "(%s)\n",
			        entry, entry->id, entry->name);
		}
	} else if(node->type == VFS_NODE_FILE) {
		kprintf(LOG_NONE, "\nCached pages:\n");

		AVL_TREE_FOREACH(&node->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			kprintf(LOG_NONE, "  Page 0x%016" PRIpp " - Offset: %-10" PRId64 " Flags: %d\n",
			        page->addr, page->offset, page->flags);
		}
	}

	return KDBG_OK;
}

#if 0
# pragma mark Initialization functions.
#endif

/** Initialization function for the VFS. */
void __init_text vfs_init(void) {
	int ret;

	/* Initialize the node slab cache. */
	vfs_node_cache = slab_cache_create("vfs_node_cache", sizeof(vfs_node_t), 0,
	                                   vfs_node_cache_ctor, vfs_node_cache_dtor,
	                                   NULL, NULL, NULL, 0, MM_FATAL);

	/* Register RamFS and mount it as the root. */
	if((ret = vfs_type_register(&ramfs_fs_type)) != 0) {
		fatal("Could not register RamFS filesystem type (%d)", ret);
	} else if((ret = vfs_mount(NULL, "/", "ramfs", 0)) != 0) {
		fatal("Could not mount RamFS at root (%d)", ret);
	}

	/* Give the kernel process a correct current/root directory. */
	vfs_node_get(vfs_root_mount->root);
	curr_proc->ioctx.root_dir = vfs_root_mount->root;
	vfs_node_get(vfs_root_mount->root);
	curr_proc->ioctx.curr_dir = vfs_root_mount->root;
}

#if 0
# pragma mark System calls.
#endif

/** Create a file in the file system.
 *
 * Creates a new regular file in the filesystem.
 *
 * @param path		Path to file to create.
 * @param nodep		Where to store pointer to node for file (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_create(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = vfs_file_create(kpath, NULL);
	kfree(kpath);
	return ret;
}

/** Open a new file handle.
 *
 * Opens a handle to a file in the filesystem. This handle can be passed to
 * other regular file operations. When it is no longer required, it should be
 * passed to sys_handle_close(). It will automatically be closed if it is still
 * open when the calling process terminates.
 *
 * @param path		Path to file to open.
 * @param flags		Behaviour flags for the handle.
 *
 * @return		Handle ID (positive) on success, negative error code
 *			on failure.
 */
handle_t sys_fs_file_open(const char *path, int flags) {
	vfs_handle_t *data = NULL;
	char *kpath = NULL;
	handle_t handle;
	int ret;

	/* Copy the path across. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return (handle_t)ret;
	}

	/* Allocate a handle data structure. */
	data = kmalloc(sizeof(vfs_handle_t), MM_SLEEP);
	mutex_init(&data->lock, "vfs_file_handle_lock", 0);
	data->node = NULL;
	data->offset = 0;
	data->flags = flags;

	/* Look up the filesystem node and check if it is suitable. */
	if((ret = vfs_node_lookup(kpath, true, &data->node)) != 0) {
		goto fail;
	} else if(data->node->type != VFS_NODE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto fail;
	} else if(flags & FS_FILE_WRITE && VFS_NODE_IS_RDONLY(data->node)) {
		ret = -ERR_READ_ONLY;
		goto fail;
	}

	/* Call the mount's open function, if any. */
	if(data->node->mount->type->file_open) {
		if((ret = data->node->mount->type->file_open(data->node, flags)) != 0) {
			goto fail;
		}
	}

	/* Allocate a handle in the current process. */
	if((handle = handle_create(&curr_proc->handles, &vfs_file_handle_type, data)) < 0) {
		if(data->node->mount->type->file_close) {
			data->node->mount->type->file_close(data->node);
		}
		ret = (int)handle;
		goto fail;
	}

	dprintf("vfs: opened file handle %" PRId32 "(%p) to %s (node: %p, process: %" PRIu32 ")\n",
	        handle, data, kpath, data->node, curr_proc->id);
	kfree(kpath);
	return handle;
fail:
	if(data) {
		if(data->node) {
			vfs_node_release(data->node);
		}
		kfree(data);
	}
	kfree(kpath);
	return (handle_t)ret;
}

/** Read from a file.
 *
 * Reads data from a file into a buffer. If a non-negative offset is supplied,
 * then it will be used as the offset to read from, and the offset of the file
 * handle will not be taken into account or updated. Otherwise, the read will
 * occur from the file handle's current offset, and before returning the offset
 * will be incremented by the number of bytes read.
 *
 * @todo		Nonblocking reads.
 *
 * @param handle	File handle to read from.
 * @param buf		Buffer to read data into. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to read.
 * @param offset	Offset to read from (if non-negative).
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_read(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	handle_info_t *info;
	bool update = false;
	vfs_handle_t *file;
	size_t bytes = 0;
	int ret = 0, err;
	void *kbuf;

	/* Look up the file handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_FILE, &info)) != 0) {
		goto out;
	}
	file = info->data;

	/* Check if the handle is open for reading. */
	if(!(file->flags & FS_FILE_READ)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	}

	/* Check if count is 0 before checking other parameters so an error is
	 * returned if necessary. */
	if(count == 0) {
		goto out;
	}

	/* Work out the offset to read from. */
	if(offset < 0) {
		mutex_lock(&file->lock, 0);
		offset = file->offset;
		mutex_unlock(&file->lock);

		update = true;
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
	ret = vfs_file_read(file->node, kbuf, count, offset, &bytes);
	if(bytes) {
		/* Update file offset. */
		if(update) {
			mutex_lock(&file->lock, 0);
			file->offset += bytes;
			mutex_unlock(&file->lock);
		}

		/* Copy data across. */
		if((err = memcpy_to_user(buf, kbuf, bytes)) != 0) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	if(info) {
		handle_release(info);
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
 * @todo		Nonblocking writes.
 *
 * @param node		Node to write to (must be VFS_NODE_FILE).
 * @param buf		Buffer to write data from. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (if non-negative).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_file_write(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	handle_info_t *info;
	bool update = false;
	vfs_handle_t *file;
	void *kbuf = NULL;
	size_t bytes = 0;
	int ret = 0, err;

	/* Look up the file handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_FILE, &info)) != 0) {
		goto out;
	}
	file = info->data;

	/* Check if the handle is open for writing. */
	if(!(file->flags & FS_FILE_WRITE)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	}

	/* Check if count is 0 before checking other parameters so an error is
	 * returned if necessary. */
	if(count == 0) {
		goto out;
	}

	/* Work out the offset to write to, and set it to the end of the file
	 * if the handle has the FS_FILE_APPEND flag set. */
	if(offset < 0) {
		mutex_lock(&file->lock, 0);
		if(file->flags & FS_FILE_APPEND) {
			file->offset = file->node->size;
		}
		offset = file->offset;
		mutex_unlock(&file->lock);

		update = true;
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
	ret = vfs_file_write(file->node, kbuf, count, offset, &bytes);
	if(bytes && update) {
		mutex_lock(&file->lock, 0);
		file->offset += bytes;
		mutex_unlock(&file->lock);
	}
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	if(info) {
		handle_release(info);
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
	handle_info_t *info;
	vfs_handle_t *file;
	int ret;

	/* Look up the file handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_FILE, &info)) != 0) {
		return ret;
	}
	file = info->data;

	/* Check if the handle is open for writing. */
	if(!(file->flags & FS_FILE_WRITE)) {
		handle_release(info);
		return -ERR_PERM_DENIED;
	}

	ret = vfs_file_resize(file->node, size);
	handle_release(info);
	return ret;
}

/** Create a directory in the file system.
 *
 * Creates a new directory in the filesystem.
 *
 * @param path		Path to directory to create.
 * @param nodep		Where to store pointer to node for directory (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_dir_create(const char *path) {
	char *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	ret = vfs_dir_create(kpath, NULL);
	kfree(kpath);
	return ret;
}

/** Open a new directory handle.
 *
 * Opens a handle to a directory in the filesystem. This handle can be passed
 * to other directory operations. When it is no longer required, it should be
 * passed to sys_handle_close(). It will automatically be closed if it is still
 * open when the calling process terminates.
 *
 * @param path		Path to directory to open.
 * @param flags		Behaviour flags for the handle.
 *
 * @return		Handle ID (positive) on success, negative error code
 *			on failure.
 */
handle_t sys_fs_dir_open(const char *path, int flags) {
	vfs_handle_t *data = NULL;
	char *kpath = NULL;
	handle_t handle;
	int ret;

	/* Copy the path across. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return (handle_t)ret;
	}

	/* Allocate a handle data structure. */
	data = kmalloc(sizeof(vfs_handle_t), MM_SLEEP);
	mutex_init(&data->lock, "vfs_dir_handle_lock", 0);
	data->node = NULL;
	data->offset = 0;
	data->flags = flags;

	/* Look up the filesystem node and check if it is suitable. */
	if((ret = vfs_node_lookup(kpath, true, &data->node)) != 0) {
		goto fail;
	} else if(data->node->type != VFS_NODE_DIR) {
		ret = -ERR_TYPE_INVAL;
		goto fail;
	}

	/* Call the mount's open function, if any. */
	if(data->node->mount->type->dir_open) {
		if((ret = data->node->mount->type->dir_open(data->node, flags)) != 0) {
			goto fail;
		}
	}

	/* Allocate a handle in the current process. */
	if((handle = handle_create(&curr_proc->handles, &vfs_dir_handle_type, data)) < 0) {
		if(data->node->mount->type->dir_close) {
			data->node->mount->type->dir_close(data->node);
		}
		ret = (int)handle;
		goto fail;
	}

	dprintf("vfs: opened dir handle %" PRId32 "(%p) to %s (node: %p, process: %" PRIu32 ")\n",
	        handle, data, kpath, data->node, curr_proc->id);
	kfree(kpath);
	return handle;
fail:
	if(data) {
		if(data->node) {
			vfs_node_release(data->node);
		}
		kfree(data);
	}
	kfree(kpath);
	return (handle_t)ret;
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
 * @todo		Nonblocking reads.
 *
 * @param handle	Handle to directory to read from.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, -ERR_PARAM_INVAL
 *			will be returned).
 * @param index		Number of the directory entry to read, if non-negative.
 *			If not found, -ERR_NOT_FOUND will be returned.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_dir_read(handle_t handle, vfs_dir_entry_t *buf, size_t size, offset_t index) {
	vfs_dir_entry_t *kbuf;
	handle_info_t *info;
	bool update = false;
	vfs_handle_t *dir;
	int ret;

	if(!size) {
		return -ERR_PARAM_INVAL;
	}

	/* Look up the directory handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_DIR, &info)) != 0) {
		return ret;
	}
	dir = info->data;

	/* Work out the index of the entry to read. */
	if(index < 0) {
		mutex_lock(&dir->lock, 0);
		index = dir->offset;
		mutex_unlock(&dir->lock);

		update = true;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(size, 0))) {
		handle_release(info);
		return -ERR_NO_MEMORY;
	}

	/* Perform the actual read. */
	ret = vfs_dir_read(dir->node, kbuf, size, index);
	if(ret == 0) {
		/* Update offset in the handle. */
		if(update) {
			mutex_lock(&dir->lock, 0);
			dir->offset++;
			mutex_unlock(&dir->lock);
		}

		/* Copy data across. */
		ret = memcpy_to_user(buf, kbuf, kbuf->length);
	}

	kfree(kbuf);
	handle_release(info);
	return ret;
}

/** Set the offset of a VFS handle.
 *
 * Modifies the offset of a file or directory handle according to the specified
 * action, and returns the new offset. For directories, the offset is the
 * index of the next directory entry that will be read.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FS_FILE_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_handle_seek(handle_t handle, int action, offset_t offset, offset_t *newp) {
	handle_info_t *info;
	vfs_handle_t *data;
	int ret;

	/* Look up the handle and check the type. */
	if((ret = handle_get(&curr_proc->handles, handle, -1, &info)) != 0) {
		return ret;
	} else if(info->type->id != HANDLE_TYPE_FILE && info->type->id != HANDLE_TYPE_DIR) {
		ret = -ERR_TYPE_INVAL;
		goto out;
	}

	/* Get the data structure and lock it. */
	data = info->data;
	mutex_lock(&data->lock, 0);

	/* Perform the action. */
	switch(action) {
	case FS_HANDLE_SEEK_SET:
		data->offset = offset;
		break;
	case FS_HANDLE_SEEK_ADD:
		data->offset += offset;
		break;
	case FS_HANDLE_SEEK_END:
		mutex_lock(&data->node->lock, 0);

		/* To do this on directories, we must cache the entries to
		 * know the size. */
		if((ret = vfs_dir_cache_entries(data->node)) != 0) {
			mutex_unlock(&data->node->lock);
			mutex_unlock(&data->lock);
			goto out;
		}

		data->offset = data->node->size + offset;
		mutex_unlock(&data->node->lock);
		break;
	}

	/* Write the new offset if necessary. */
	if(newp) {
		ret = memcpy_to_user(newp, &data->offset, sizeof(offset_t));
	}
	mutex_unlock(&data->lock);
out:
	handle_release(info);
	return ret;
}

int sys_fs_handle_info(handle_t handle, vfs_info_t *infop) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Create a symbolic link.
 *
 * Creates a new symbolic link in the filesystem.
 *
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_symlink_create(const char *path, const char *target) {
	char *kpath, *ktarget;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = strndup_from_user(target, PATH_MAX, MM_SLEEP, &ktarget)) != 0) {
		kfree(kpath);
		return ret;
	}

	ret = vfs_symlink_create(kpath, ktarget, NULL);
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
	vfs_node_t *node;
	int ret, err;

	/* Copy the path across. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	}

	/* Look up the filesystem node. No need to check here whether it is
	 * suitable, vfs_symlink_read() does that. */
	if((ret = vfs_node_lookup(kpath, false, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Allocate a buffer to read into. See comment in sys_fs_file_read()
	 * about not using MM_SLEEP. */
	if(!(kbuf = kmalloc(size, 0))) {
		vfs_node_release(node);
		kfree(kpath);
		return -ERR_NO_MEMORY;
	}

	ret = vfs_symlink_read(node, kbuf, size);
	if(ret > 0) {
		if((err = memcpy_to_user(buf, kbuf, size)) != 0) {
			ret = err;
		}
	}

	vfs_node_release(node);
	kfree(kpath);
	return ret;
}

/** Mount a filesystem.
 *
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Some filesystem types are read-only by design - when mounting these the
 * VFS_MOUNT_RDONLY flag will automatically be set. It may also be set if the
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
		//if((ret = strndup_from_user(dev, 0, &kdev)) != 0) {
		//	goto out;
		//}
		return -ERR_NOT_IMPLEMENTED;
	}
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		goto out;
	}
	if((ret = strndup_from_user(type, PATH_MAX, MM_SLEEP, &ktype)) != 0) {
		goto out;
	}

	ret = vfs_mount(kdev, kpath, ktype, flags);
out:
	if(kdev) { kfree(kdev); }
	if(kpath) { kfree(kpath); }
	if(ktype) { kfree(ktype); }
	return ret;
}

int sys_fs_unmount(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_getcwd(char *buf, size_t size) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Set the current working directory.
 *
 * Changes the calling process' current working directory.
 *
 * @param path		Path to change to.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_fs_setcwd(const char *path) {
	vfs_node_t *node;
	char *kpath;
	int ret;

	/* Get the path and look it up. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = vfs_node_lookup(kpath, true, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. If the node is the wrong type, it will be picked
	 * up by io_context_setcwd(). Release the node no matter what, as upon
	 * success it is referenced by io_context_setcwd(). */
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
	} else if((ret = vfs_node_lookup(kpath, true, &node)) != 0) {
		kfree(kpath);
		return ret;
	}

	/* Attempt to set. If the node is the wrong type, it will be picked
	 * up by io_context_setroot(). Release the node no matter what, as upon
	 * success it is referenced by io_context_setroot(). */
	ret = io_context_setroot(&curr_proc->ioctx, node);
	vfs_node_release(node);
	kfree(kpath);
	return ret;
}

int sys_fs_info(const char *path, bool follow, vfs_info_t *infop) {
	return -ERR_NOT_IMPLEMENTED;
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
