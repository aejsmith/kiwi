/*
 * Copyright (C) 2009-2011 Alex Smith
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
#include <security/context.h>

#include <assert.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

#if CONFIG_FS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

KBOOT_BOOLEAN_OPTION("force_fsimage", "Force filesystem image usage", false);

/** Data for a file handle. */
typedef struct file_handle {
	mutex_t lock;			/**< Lock to protect offset. */
	offset_t offset;		/**< Current file offset. */
	int flags;			/**< Flags the file was opened with. */
} file_handle_t;

extern void fs_node_get(fs_node_t *node);
extern status_t kern_fs_security(const char *path, bool follow, user_id_t *uidp,
                                 group_id_t *gidp, object_acl_t *aclp);
static status_t dir_lookup(fs_node_t *node, const char *name, node_id_t *idp);
static object_type_t file_object_type;

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
 * @param security	Security attributes for the node. Should be NULL if the
 *			filesystem does not support security attributes, in
 *			which case default attributes will be used.
 * @param ops		Pointer to operations structure for the node.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to node structure allocated. */
fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, file_type_t type,
                         object_security_t *security, fs_node_ops_t *ops,
                         void *data) {
	object_security_t dsecurity;
	object_acl_t acl, sacl;
	fs_node_t *node;

	node = slab_cache_alloc(fs_node_cache, MM_WAIT);
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

	/* If no security attributes are provided, it means that the FS we're
	 * creating the node for does not have security support. Construct a
	 * default ACL that grants access to everyone. */
	if(!security) {
		dsecurity.uid = 0;
		dsecurity.gid = 0;
		dsecurity.acl = &acl;

		object_acl_init(&acl);
		object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0, DEFAULT_FILE_RIGHTS_OWNER);

		security = &dsecurity;
	}

	/* Create the system ACL. This grants processes with the CAP_FS_ADMIN
	 * capability full control over the filesystem. FIXME: Should only
	 * grant execute if a directory or if the standard ACL grants execute
	 * capability to somebody. */
	object_acl_init(&sacl);
	object_acl_add_entry(&sacl, ACL_ENTRY_CAPABILITY, CAP_FS_ADMIN, OBJECT_RIGHT_OWNER);
	//object_acl_add_entry(&sacl, ACL_ENTRY_CAPABILITY, CAP_FS_ADMIN,
	//                     OBJECT_SET_ACL | OBJECT_SET_OWNER | FS_READ | FS_WRITE | FS_EXECUTE);

	/* Initialize the node's object header. Only regular files and
	 * directories can have handles opened to them. */
	switch(type) {
	case FILE_TYPE_REGULAR:
	case FILE_TYPE_DIR:
		object_init(&node->obj, &file_object_type, security, &sacl);
		break;
	default:
		object_init(&node->obj, NULL, security, &sacl);
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
		avl_tree_remove(&node->mount->nodes, &node->tree_link);
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

		assert(node->type == FILE_TYPE_DIR);

		/* Return the root node if the end of the path has been reached. */
		if(!path[0]) {
			*nodep = node;
			return STATUS_SUCCESS;
		}
	} else {
		assert(node->type == FILE_TYPE_DIR);
	}

	/* Loop through each element of the path string. */
	while(true) {
		tok = strsep(&path, "/");

		/* If the node is a symlink and this is not the last element
		 * of the path, or the caller wishes to follow the link, follow
		 * it. */
		if(node->type == FILE_TYPE_SYMLINK && (tok || follow)) {
			/* The previous node should be the link's parent. */
			assert(prev);
			assert(prev->type == FILE_TYPE_DIR);

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
		} else if(node->type == FILE_TYPE_SYMLINK) {
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
		} else if(node->type != FILE_TYPE_DIR) {
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
		if(!(object_rights(&node->obj, NULL) & FILE_RIGHT_EXECUTE)) {
			fs_node_release(node);
			return STATUS_ACCESS_DENIED;
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
				assert(node->mount->mountpoint->type == FILE_TYPE_DIR);

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
		ret = dir_lookup(node, tok, &id);
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
		node = avl_tree_lookup(&mount->nodes, id);
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
			avl_tree_insert(&mount->nodes, &node->tree_link, id, node);
			list_append(&mount->used_nodes, &node->mount_link);
			mutex_unlock(&mount->lock);
		}

		/* Do not release the previous node if the new node is a
		 * symbolic link, as the symbolic link lookup requires it. */
		if(node->type != FILE_TYPE_SYMLINK) {
			fs_node_release(prev);
		}
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
	dup = kstrdup(path, MM_WAIT);

	/* Look up the path string. */
	ret = fs_node_lookup_internal(dup, node, follow, 0, &node);
	if(ret == STATUS_SUCCESS) {
		if(type >= 0 && node->type != (file_type_t)type) {
			if(type == FILE_TYPE_REGULAR) {
				ret = STATUS_NOT_REGULAR;
			} else if(type == FILE_TYPE_DIR) {
				ret = STATUS_NOT_DIR;
			} else if(type == FILE_TYPE_SYMLINK) {
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

/**
 * Decrease the reference count of a node.
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
	node->removed = true;
}

/** Common node creation code.
 * @param path		Path to node to create.
 * @param type		Type to give the new node.
 * @param target	For symbolic links, the target of the link.
 * @param security	Security attributes for the node.
 * @param nodep		Where to store pointer to created node (can be NULL).
 * @return		Status code describing result of the operation. */
static status_t fs_node_create(const char *path, file_type_t type, const char *target,
                               object_security_t *security, fs_node_t **nodep) {
	fs_node_t *parent = NULL, *node = NULL;
	char *dir, *name;
	node_id_t id;
	status_t ret;
	size_t i;

	assert(security);
	assert(security->acl);

	/* Validate the security attributes. */
	ret = object_security_validate(security, NULL);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	for(i = 0; i < security->acl->count; i++) {
		switch(security->acl->entries[i].type) {
		case ACL_ENTRY_CAPABILITY:
		case ACL_ENTRY_SESSION:
			return STATUS_NOT_SUPPORTED;
		}
	}

	/* Replace -1 for UID and GID in the security attributes with the
	 * current UID/GID. Normally this would be done by object_init(),
	 * however we pass this through to the filesystem to write the
	 * security attributes for the node, meaning the values must be
	 * valid. */
	if(security->uid < 0) {
		security->uid = security_current_uid();
	}
	if(security->gid < 0) {
		security->gid = security_current_gid();
	}

	/* Split path into directory/name. */
	dir = kdirname(path, MM_WAIT);
	name = kbasename(path, MM_WAIT);

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
	ret = fs_node_lookup(dir, true, FILE_TYPE_DIR, &parent);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	mutex_lock(&parent->mount->lock);

	/* Check if the name we're creating already exists. This will populate
	 * the entry cache so it will be OK to add the node to it. */
	ret = dir_lookup(parent, name, &id);
	if(ret != STATUS_NOT_FOUND) {
		if(ret == STATUS_SUCCESS) {
			ret = STATUS_ALREADY_EXISTS;
		}
		goto out;
	}

	/* Check that we are on a writable filesystem, that we have write
	 * permission to the directory, and that the FS supports node
	 * creation. */
	if(FS_NODE_IS_RDONLY(parent)) {
		ret = STATUS_READ_ONLY;
		goto out;
	} else if(!(object_rights(&parent->obj, NULL) & FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	} else if(!parent->ops->create) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	/* We can now call into the filesystem to create the node. */
	ret = parent->ops->create(parent, name, type, target, security, &node);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Attach the node to the node tree and used list. */
	avl_tree_insert(&node->mount->nodes, &node->tree_link, node->id, node);
	list_append(&node->mount->used_nodes, &node->mount_link);

	dprintf("fs: created %s (node: %" PRIu16 ":%" PRIu64 ", parent: %" PRIu16 ":%" PRIu64 ")\n",
	        path, node->mount->id, node->id, parent->mount->id, parent->id);
	if(nodep) {
		*nodep = node;
		node = NULL;
	}
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
 * @param infop		Structure to store information in. */
static void fs_node_info(fs_node_t *node, file_info_t *infop) {
	memset(infop, 0, sizeof(file_info_t));
	infop->id = node->id;
	infop->mount = (node->mount) ? node->mount->id : 0;
	infop->type = node->type;
	if(node->ops->info) {
		node->ops->info(node, infop);
	} else {
		infop->links = 1;
		infop->size = 0;
		infop->block_size = PAGE_SIZE;
	}
}

/** Get the name of a node in its parent directory.
 * @param parent	Directory containing node.
 * @param id		ID of node to get name of.
 * @param namep		Where to store pointer to string containing node name.
 * @return		Status code describing result of the operation. */
static status_t fs_node_name(fs_node_t *parent, node_id_t id, char **namep) {
	dir_entry_t *entry;
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
			*namep = kstrdup(entry->name, MM_WAIT);
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

	fs_node_get(node);

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
		len += ((buf) ? strlen(name) + 1 : strlen(name));
		tmp = kmalloc(len + 1, MM_WAIT);
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
	tmp = kmalloc((++len) + 1, MM_WAIT);
	strcpy(tmp, "/");
	if(buf) {
		strcat(tmp, buf);
		kfree(buf);
	}
	buf = tmp;

	*pathp = buf;
	return STATUS_SUCCESS;
fail:
	if(node) {
		fs_node_release(node);
	}
	kfree(buf);
	return ret;
}

/** Create a handle to a node.
 * @param node		Node to create handle to (will be referenced).
 * @param rights	Access rights for the handle. These are not checked
 *			against the ACL, must be done before calling if
 *			necessary.
 * @param flags		Flags for the handle.
 * @param handlep	Where to store pointer to handle.
 * @return		Status code describing result of the operation. */
static status_t file_handle_create(fs_node_t *node, object_rights_t rights, int flags,
                                   object_handle_t **handlep) {
	file_handle_t *data;
	status_t ret;

	/* Prevent opening for writing on a read-only filesystem. */
	if(rights & FILE_RIGHT_WRITE && FS_NODE_IS_RDONLY(node)) {
		return STATUS_READ_ONLY;
	}

	/* Allocate the per-handle data structure. */
	data = kmalloc(sizeof(file_handle_t), MM_WAIT);
	mutex_init(&data->lock, "file_handle_lock", 0);
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

/** Change filesystem security attributes.
 * @param object	Object to check.
 * @param security	New security attributes.
 * @return		Status code describing result of the operation. */
static status_t file_object_set_security(object_t *object, object_security_t *security) {
	fs_node_t *node = (fs_node_t *)object;
	size_t i;

	if(FS_NODE_IS_RDONLY(node)) {
		return STATUS_READ_ONLY;
	} else if(!node->ops->set_security) {
		return STATUS_NOT_SUPPORTED;
	}

	/* The ACL must not contain any session or capability entries. */
	if(security->acl) {
		for(i = 0; i < security->acl->count; i++) {
			switch(security->acl->entries[i].type) {
			case ACL_ENTRY_CAPABILITY:
			case ACL_ENTRY_SESSION:
				return STATUS_NOT_SUPPORTED;
			}
		}
	}

	return node->ops->set_security(node, security);
}

/** Close a handle to a file.
 * @param handle	Handle to close. */
static void file_object_close(object_handle_t *handle) {
	fs_node_release((fs_node_t *)handle->object);
	kfree(handle->data);
}

/** Change file handle options.
 * @param handle	Handle to operate on.
 * @param action	Action to perform.
 * @param arg		Argument to function.
 * @param outp		Where to store return value.
 * @return		Status code describing result of the operation. */
static status_t file_object_control(object_handle_t *handle, int action, int arg, int *outp) {
	file_handle_t *data = handle->data;

	switch(action) {
	case HANDLE_GET_FLAGS:
		*outp = data->flags;
		break;
	case HANDLE_SET_FLAGS:
		data->flags = arg;
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}

	return STATUS_SUCCESS;
}

/** Check if a file can be memory-mapped.
 * @param handle	Handle to file.
 * @param flags		Mapping flags (VM_MAP_*).
 * @return		STATUS_SUCCESS if mappable, other status code if not. */
static status_t file_object_mappable(object_handle_t *handle, int flags) {
	fs_node_t *node = (fs_node_t *)handle->object;

	/* Directories cannot be memory-mapped. */
	if(node->type == FILE_TYPE_DIR) {
		return STATUS_NOT_SUPPORTED;
	}

	/* Check whether the filesystem supports memory-mapping. */
	if(!node->ops->get_cache) {
		return STATUS_NOT_SUPPORTED;
	}

	/* If mapping for reading, check if allowed. */
	if(flags & VM_MAP_READ) {
		if(!object_handle_rights(handle, FILE_RIGHT_READ)) {
			return STATUS_ACCESS_DENIED;
		}
	}

	/* If creating a shared mapping for writing, check for write access.
	 * It is not necessary to check for a read-only filesystem here: a
	 * handle cannot be opened with FILE_RIGHT_WRITE on a read-only FS. */
	if((flags & (VM_MAP_WRITE | VM_MAP_PRIVATE)) == VM_MAP_WRITE) {
		if(!object_handle_rights(handle, FILE_RIGHT_WRITE)) {
			return STATUS_ACCESS_DENIED;
		}
	}

	/* If mapping for execution, check for execute access. */
	if(flags & VM_MAP_EXEC) {
		if(!object_handle_rights(handle, FILE_RIGHT_EXECUTE)) {
			return STATUS_ACCESS_DENIED;
		}
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
	.set_security = file_object_set_security,
	.close = file_object_close,
	.control = file_object_control,
	.mappable = file_object_mappable,
	.get_page = file_object_get_page,
	.release_page = file_object_release_page,
};

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

/**
 * Create a read-only file backed by a chunk of memory.
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
 * @return		Pointer to handle to file (has FILE_RIGHT_READ right).
 */
object_handle_t *file_from_memory(const void *buf, size_t size) {
	object_handle_t *handle;
	memory_file_t *file;
	fs_node_t *node;

	file = kmalloc(sizeof(memory_file_t), MM_WAIT);
	file->data = buf;
	file->size = size;

	node = fs_node_alloc(NULL, 0, FILE_TYPE_REGULAR, NULL, &memory_file_ops, file);
	file_handle_create(node, FILE_RIGHT_READ, 0, &handle);
	fs_node_release(node);
	return handle;
}

/**
 * Open a handle to a file or directory.
 *
 * Opens a handle to a regular file or directory, optionally creating it if it
 * doesn't exist. If the entry does not exist, it will be created as a regular
 * file. To create a directory, use dir_create().
 *
 * @param path		Path to file or directory to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param create	Whether to create the file. If 0, the file will not be
 *			created if it doesn't exist. If FILE_CREATE, it will be
 *			created if it doesn't exist. If FILE_CREATE_ALWAYS, it
 *			will always be created, and an error will be returned
 *			if it already exists.
 * @param security	If creating the file, the security attributes to give
 *			to it. If NULL, default security attributes will be
 *			used. Note that the ACL (if any) will not be copied: the
 *			memory used for it will be taken over and the given ACL
 *			structure will be invalidated.
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_open(const char *path, object_rights_t rights, int flags, int create,
                   object_security_t *security, object_handle_t **handlep) {
	object_security_t dsecurity = { -1, -1, NULL };
	object_acl_t acl;
	fs_node_t *node;
	status_t ret;

	if(create != 0 && create != FILE_CREATE && create != FILE_CREATE_ALWAYS) {
		return STATUS_INVALID_ARG;
	}

	/* Look up the filesystem node. */
	ret = fs_node_lookup(path, true, -1, &node);
	if(ret != STATUS_SUCCESS) {
		/* If requested try to create the node. */
		if(ret == STATUS_NOT_FOUND && create) {
			if(security) {
				dsecurity.uid = security->uid;
				dsecurity.gid = security->gid;
				dsecurity.acl = security->acl;
			}

			/* Create a default ACL if none is given. */
			if(!dsecurity.acl) {
				dsecurity.acl = &acl;
				object_acl_init(&acl);
				object_acl_add_entry(&acl, ACL_ENTRY_USER, -1,
				                     DEFAULT_FILE_RIGHTS_OWNER);
				object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0,
				                     DEFAULT_FILE_RIGHTS_OTHERS);
			}

			ret = fs_node_create(path, FILE_TYPE_REGULAR, NULL, &dsecurity, &node);
			object_acl_destroy(dsecurity.acl);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		} else {
			return ret;
		}
	} else if(create == FILE_CREATE_ALWAYS) {
		fs_node_release(node);
		return STATUS_ALREADY_EXISTS;
	} else if(node->type != FILE_TYPE_REGULAR && node->type != FILE_TYPE_DIR) {
		fs_node_release(node);
		return STATUS_NOT_SUPPORTED;
	} else if(rights && (object_rights(&node->obj, NULL) & rights) != rights) {
		/* This check will only be done if we haven't had to create the
		 * new file. */
		return STATUS_ACCESS_DENIED;
	}

	ret = file_handle_create(node, rights, flags, handlep);
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
static status_t file_read_internal(object_handle_t *handle, void *buf, size_t count,
                                   offset_t offset, bool usehnd, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	file_handle_t *data;
	size_t total = 0;
	fs_node_t *node;

	if(!handle || !buf) {
		ret = STATUS_INVALID_ARG;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = STATUS_INVALID_HANDLE;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	if(node->type != FILE_TYPE_REGULAR) {
		ret = STATUS_NOT_REGULAR;
		goto out;
	} else if(!object_handle_rights(handle, FILE_RIGHT_READ)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	} else if(!node->ops->read) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Pull the offset out of the handle structure if required. */
	if(usehnd) {
		offset = data->offset;
	}

	ret = node->ops->read(node, buf, count, offset, data->flags & FILE_NONBLOCK, &total);
out:
	if(total) {
		dprintf("fs: read %zu bytes from offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
		        total, offset, node, (node->mount) ? node->mount->id : 0, node->id);
		if(usehnd) {
			mutex_lock(&data->lock);
			data->offset += total;
			mutex_unlock(&data->lock);
		}
	}
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur from the file
 * handle's current offset, and before returning the offset will be incremented
 * by the number of bytes read.
 *
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_read(object_handle_t *handle, void *buf, size_t count, size_t *bytesp) {
	return file_read_internal(handle, buf, count, 0, true, bytesp);
}

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
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
status_t file_pread(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	return file_read_internal(handle, buf, count, offset, false, bytesp);
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
static status_t file_write_internal(object_handle_t *handle, const void *buf, size_t count,
                                    offset_t offset, bool usehnd, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	file_handle_t *data;
	file_info_t info;
	size_t total = 0;
	fs_node_t *node;

	if(!handle || !buf) {
		ret = STATUS_INVALID_ARG;
		goto out;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = STATUS_INVALID_HANDLE;
		goto out;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	if(node->type != FILE_TYPE_REGULAR) {
		ret = STATUS_NOT_REGULAR;
		goto out;
	} else if(!object_handle_rights(handle, FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	} else if(!node->ops->write) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	} else if(!count) {
		goto out;
	}

	/* Pull the offset out of the handle, and handle the FILE_APPEND flag. */
	if(usehnd) {
		if(data->flags & FILE_APPEND) {
			mutex_lock(&data->lock);
			fs_node_info(node, &info);
			data->offset = offset = info.size;
			mutex_unlock(&data->lock);
		} else {
			offset = data->offset;
		}
	}

	ret = node->ops->write(node, buf, count, offset, data->flags & FILE_NONBLOCK, &total);
out:
	if(total) {
		dprintf("fs: wrote %zu bytes to offset 0x%" PRIx64 " in %p(%" PRIu16 ":%" PRIu64 ")\n",
		        total, offset, node, (node->mount) ? node->mount->id : 0, node->id);
		if(usehnd) {
			mutex_lock(&data->lock);
			data->offset += total;
			mutex_unlock(&data->lock);
		}
	}
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/**
 * Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the file
 * handle's current offset (if the FILE_APPEND flag is set, the offset will be
 * set to the end of the file and the write will take place there), and before
 * returning the handle's offset will be incremented by the number of bytes
 * written.
 *
 * @param handle	Handle to file to write to. Must have the
 *			FILE_RIGHT_WRITE access right.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FILE_APPEND is
 *			set).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_write(object_handle_t *handle, const void *buf, size_t count, size_t *bytesp) {
	return file_write_internal(handle, buf, count, 0, true, bytesp);
}

/**
 * Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to write to. Must have the
 *			FILE_RIGHT_WRITE access right.
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
status_t file_pwrite(object_handle_t *handle, const void *buf, size_t count, offset_t offset,
                     size_t *bytesp) {
	return file_write_internal(handle, buf, count, offset, false, bytesp);
}

/**
 * Modify the size of a file.
 *
 * Modifies the size of a file in the file system. If the new size is smaller
 * than the previous size of the file, then the extra data is discarded. If
 * it is larger than the previous size, then the extended space will be filled
 * with zero bytes.
 *
 * @param handle	Handle to file to resize. Must have the FILE_RIGHT_WRITE
 *			access right.
 * @param size		New size of the file.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_resize(object_handle_t *handle, offset_t size) {
	fs_node_t *node;

	if(!handle) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	if(node->type != FILE_TYPE_REGULAR) {
		return STATUS_NOT_REGULAR;
	} else if(!object_handle_rights(handle, FILE_RIGHT_WRITE)) {
		return STATUS_ACCESS_DENIED;
	} else if(!node->ops->resize) {
		return STATUS_NOT_SUPPORTED;
	}

	return node->ops->resize(node, size);
}

/**
 * Set the offset of a file handle.
 *
 * Modifies the offset of a file handle according to the specified action, and
 * returns the new offset. For directories, the offset is the index of the next
 * directory entry that will be read.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FILE_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		Status code describing result of the operation.
 */
status_t file_seek(object_handle_t *handle, int action, rel_offset_t offset, offset_t *newp) {
	file_handle_t *data;
	file_info_t info;
	fs_node_t *node;

	if(!handle || (action != FILE_SEEK_SET && action != FILE_SEEK_ADD && action != FILE_SEEK_END)) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	mutex_lock(&data->lock);

	/* Perform the action. */
	switch(action) {
	case FILE_SEEK_SET:
		if(offset < 0) {
			mutex_unlock(&data->lock);
			return STATUS_INVALID_ARG;
		}
		data->offset = (offset_t)offset;
		break;
	case FILE_SEEK_ADD:
		if(((rel_offset_t)data->offset + offset) < 0) {
			mutex_unlock(&data->lock);
			return STATUS_INVALID_ARG;
		}
		data->offset += offset;
		break;
	case FILE_SEEK_END:
		if(node->type == FILE_TYPE_DIR) {
			/* FIXME. */
			mutex_unlock(&data->lock);
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
	mutex_unlock(&data->lock);
	return STATUS_SUCCESS;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information for.
 * @param infop		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t file_info(object_handle_t *handle, file_info_t *infop) {
	fs_node_t *node;

	if(!handle || !infop) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	fs_node_info(node, infop);
	return STATUS_SUCCESS;
}

/** Flush changes to a file to the FS.
 * @param handle	Handle to file to flush.
 * @return		Status code describing result of the operation. */
status_t file_sync(object_handle_t *handle) {
	fs_node_t *node;

	if(!handle) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	if(!FS_NODE_IS_RDONLY(node) && node->ops->flush) {
		return node->ops->flush(node);
	} else {
		return STATUS_SUCCESS;
	}
}

/** Look up an entry in a directory.
 * @param node		Node to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry maps to.
 * @return		Status code describing result of the operation. */
static status_t dir_lookup(fs_node_t *node, const char *name, node_id_t *idp) {
	if(!node->ops->lookup_entry) {
		return STATUS_NOT_SUPPORTED;
	}
	return node->ops->lookup_entry(node, name, idp);
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
 * @param security	Security attributes for the directory. If NULL, default
 *			security attributes will be used. Note that the ACL (if
 *			any) will not be copied: the memory used for it will be
 *			taken over and the given ACL structure will be
 *			invalidated.
 *
 * @return		Status code describing result of the operation.
 */
status_t dir_create(const char *path, object_security_t *security) {
	object_security_t dsecurity = { -1, -1, NULL };
	object_acl_t acl;
	status_t ret;

	if(security) {
		dsecurity.uid = security->uid;
		dsecurity.gid = security->gid;
		if(security->acl) {
			dsecurity.acl = security->acl;
		}
	}

	/* Create a default ACL if none is given. */
	if(!dsecurity.acl) {
		dsecurity.acl = &acl;
		object_acl_init(&acl);
		object_acl_add_entry(&acl, ACL_ENTRY_USER, -1, DEFAULT_DIR_RIGHTS_OWNER);
		object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0, DEFAULT_DIR_RIGHTS_OTHERS);
	}

	ret = fs_node_create(path, FILE_TYPE_DIR, NULL, &dsecurity, NULL);
	object_acl_destroy(dsecurity.acl);
	return ret;
}

/**
 * Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed. The number of the entry read
 * will be the handle's current offset, and upon success the handle's offset
 * will be incremented by 1.
 *
 * @param handle	Handle to directory to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, the function will
 *			return STATUS_TOO_SMALL).
 *
 * @return		Status code describing result of the operation. If the
 *			handle's offset is past the end of the directory,
 *			STATUS_NOT_FOUND will be returned.
 */
status_t dir_read(object_handle_t *handle, dir_entry_t *buf, size_t size) {
	fs_node_t *child, *node;
	file_handle_t *data;
	dir_entry_t *entry;
	status_t ret;

	if(!handle || !buf) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_FILE) {
		return STATUS_INVALID_HANDLE;
	}

	node = (fs_node_t *)handle->object;
	data = handle->data;
	if(node->type != FILE_TYPE_DIR) {
		return STATUS_NOT_DIR;
	} else if(!object_handle_rights(handle, FILE_RIGHT_READ)) {
		return STATUS_ACCESS_DENIED;
	} else if(!node->ops->read_entry) {
		return STATUS_NOT_SUPPORTED;
	}

	/* Ask the filesystem to read the entry. */
	ret = node->ops->read_entry(node, data->offset, &entry);
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
			ret = dir_lookup(node->mount->mountpoint, "..", &buf->id);
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
		child = avl_tree_lookup(&node->mount->nodes, buf->id);
		if(child) {
			if(child != node) {
				/* Mounted pointer is protected by mount lock. */
				if(child->type == FILE_TYPE_DIR && child->mounted) {
					buf->id = child->mounted->root->id;
					buf->mount = child->mounted->id;
				}
			}
		}
	}

	mutex_unlock(&node->mount->lock);

	/* Update offset in the handle. */
	mutex_lock(&data->lock);
	data->offset++;
	mutex_unlock(&data->lock);
	return STATUS_SUCCESS;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
status_t symlink_create(const char *path, const char *target) {
	object_acl_t acl;
	object_security_t security = { -1, -1, &acl };
	status_t ret;

	/* Construct the ACL for the symbolic link. */
	object_acl_init(&acl);
	object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0,
	                     FILE_RIGHT_READ | FILE_RIGHT_WRITE | FILE_RIGHT_EXECUTE);

	ret = fs_node_create(path, FILE_TYPE_SYMLINK, target, &security, NULL);
	object_acl_destroy(security.acl);
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
status_t symlink_read(const char *path, char *buf, size_t size) {
	fs_node_t *node;
	status_t ret;
	char *dest;
	size_t len;

	if(!path || !buf || !size) {
		return STATUS_INVALID_ARG;
	}

	/* Find the link node. */
	ret = fs_node_lookup(path, false, FILE_TYPE_SYMLINK, &node);
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
		dup = kstrdup(str, MM_WAIT);

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
				opts = krealloc(opts, sizeof(fs_mount_option_t) * (count + 1), MM_WAIT);
				opts[count].name = kstrdup(name, MM_WAIT);
				opts[count].value = (value) ? kstrdup(value, MM_WAIT) : NULL;
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

/**
 * Mount a filesystem.
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
	object_rights_t rights;
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
		ret = fs_node_lookup(path, true, FILE_TYPE_DIR, &node);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		/* Check that it is not being used as a mount point already. */
		if(node->mount->root == node) {
			ret = STATUS_IN_USE;
			goto fail;
		}
	}

	/* Initialize the mount structure. */
	mount = kmalloc(sizeof(fs_mount_t), MM_WAIT);
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

		/* Only request write access if not mounting read-only. */
		rights = DEVICE_RIGHT_READ;
		if(!(flags & FS_MOUNT_RDONLY)) {
			rights |= DEVICE_RIGHT_WRITE;
		}

		ret = device_open(device, rights, &mount->device);
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
	avl_tree_insert(&mount->nodes, &mount->root->tree_link, mount->root->id, mount->root);
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
	if(mount->mountpoint) {
		mutex_lock(&mount->mountpoint->mount->lock);
	}
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
	if(mount->root->mount_link.next != &mount->used_nodes || mount->root->mount_link.prev != &mount->used_nodes) {
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

	/* Call unmount operation and release device/type. */
	if(mount->ops->unmount) {
		mount->ops->unmount(mount);
	}
	if(mount->device) {
		object_handle_release(mount->device);
	}
	refcount_dec(&mount->type->count);

	list_remove(&mount->header);
	mutex_unlock(&mount->lock);
	kfree(mount);
	return STATUS_SUCCESS;
fail:
	mutex_unlock(&mount->lock);
	if(mount->mountpoint) {
		mutex_unlock(&mount->mountpoint->mount->lock);
	}
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

	if(!path) {
		return STATUS_INVALID_ARG;
	}

	if(!cap_check(NULL, CAP_FS_MOUNT)) {
		return STATUS_PERM_DENIED;
	}

	/* Serialise mount/unmount operations. */
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
status_t fs_info(const char *path, bool follow, file_info_t *infop) {
	fs_node_t *node;
	status_t ret;

	if(!path || !infop) {
		return STATUS_INVALID_ARG;
	}

	ret = fs_node_lookup(path, follow, -1, &node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	fs_node_info(node, infop);
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
	dir = kdirname(path, MM_WAIT);
	name = kbasename(path, MM_WAIT);

	dprintf("fs: unlink(%s) - dirname is '%s', basename is '%s'\n", path, dir, name);

	/* Look up the parent node and the node to unlink. */
	ret = fs_node_lookup(dir, true, FILE_TYPE_DIR, &parent);
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
	} else if(!(object_rights(&parent->obj, NULL) & FILE_RIGHT_WRITE)) {
		ret = STATUS_ACCESS_DENIED;
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
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_mount(int argc, char **argv, kdb_filter_t *filter) {
	fs_mount_t *mount;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints out a list of all mounted filesystems.");
		return KDB_SUCCESS;
	}

	kdb_printf("%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
	           "ID", "Flags", "Type", "Ops", "Data", "Root", "Mountpoint");
	kdb_printf("%-5s %-5s %-10s %-18s %-18s %-18s %-18s\n",
	           "==", "=====", "====", "===", "====", "====", "==========");

	LIST_FOREACH(&mount_list, iter) {
		mount = list_entry(iter, fs_mount_t, header);
		kdb_printf("%-5" PRIu16 " %-5d %-10s %-18p %-18p %-18p %-18p\n",
		           mount->id, mount->flags, (mount->type) ? mount->type->name : "invalid",
		           mount->ops, mount->data, mount->root, mount->mountpoint);
	}

	return KDB_SUCCESS;
}

/** Print information about a node.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_node(int argc, char **argv, kdb_filter_t *filter) {
	fs_node_t *node = NULL;
	list_t *list = NULL;
	fs_mount_t *mount;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [--unused|--used] <mount ID>\n", argv[0]);
		kdb_printf("       %s <mount ID> <node ID>\n\n", argv[0]);

		kdb_printf("Prints either a list of nodes on a mount, or details of a\n");
		kdb_printf("single filesystem node that's currently in memory.\n");
		return KDB_SUCCESS;
	} else if(argc != 2 && argc != 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Parse the arguments. */
	if(argc == 3) {
		if(argv[1][0] == '-' && argv[1][1] == '-') {
			if(kdb_parse_expression(argv[2], &val, NULL) != KDB_SUCCESS) {
				return KDB_FAILURE;
			} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
				kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
				return KDB_FAILURE;
			}
		} else {
			if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS) {
				return KDB_FAILURE;
			} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
				kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
				return KDB_FAILURE;
			} else if(kdb_parse_expression(argv[2], &val, NULL) != KDB_SUCCESS) {
				return KDB_FAILURE;
			} else if(!(node = avl_tree_lookup(&mount->nodes, val))) {
				kdb_printf("Unknown node ID %" PRIu64 ".\n", val);
				return KDB_FAILURE;
			}
		}
	} else {
		if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS) {
			return KDB_FAILURE;
		} else if(!(mount = fs_mount_lookup((mount_id_t)val))) {
			kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
			return KDB_FAILURE;
		}
	}

	if(node) {
		/* Print out basic node information. */
		kdb_printf("Node %p(%" PRIu16 ":%" PRIu64 ")\n", node,
		           (node->mount) ? node->mount->id : 0, node->id);
		kdb_printf("=================================================\n");

		kdb_printf("Count:   %d\n", refcount_get(&node->count));
		if(node->mount) {
			kdb_printf("Mount:   %p (Locked: %d (%" PRId32 "))\n", node->mount,
			           atomic_get(&node->mount->lock.value),
			           (node->mount->lock.holder) ? node->mount->lock.holder->id : -1);
		} else {
			kdb_printf("Mount:   %p\n", node->mount);
		}
		kdb_printf("Ops:     %p\n", node->ops);
		kdb_printf("Data:    %p\n", node->data);
		kdb_printf("Removed: %d\n", node->removed);
		kdb_printf("Type:    %d\n", node->type);
		if(node->mounted) {
			kdb_printf("Mounted: %p(%" PRIu16 ")\n", node->mounted,
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

		kdb_printf("ID       Count Removed Type Ops                Data               Mount\n");
		kdb_printf("==       ===== ======= ==== ===                ====               =====\n");

		if(list) {
			LIST_FOREACH(list, iter) {
				node = list_entry(iter, fs_node_t, mount_link);
				kdb_printf("%-8" PRIu64 " %-5d %-7d %-4d %-18p %-18p %p\n",
				           node->id, refcount_get(&node->count), node->removed,
				           node->type, node->ops, node->data, node->mount);
			}
		} else {
			AVL_TREE_FOREACH(&mount->nodes, iter) {
				node = avl_tree_entry(iter, fs_node_t);
				kdb_printf("%-8" PRIu64 " %-5d %-7d %-4d %-18p %-18p %p\n",
				           node->id, refcount_get(&node->count), node->removed,
				           node->type, node->ops, node->data, node->mount);
			}
		}
	}

	return KDB_SUCCESS;
}

/** Initialize the filesystem layer. */
__init_text void fs_init(void) {
	fs_node_cache = slab_cache_create("fs_node_cache", sizeof(fs_node_t), 0,
	                                  NULL, NULL, NULL, 0, MM_BOOT);

	/* Register the KDB commands. */
	kdb_register_command("mount", "Print a list of mounted filesystems.", kdb_cmd_mount);
	kdb_register_command("node", "Display information about a filesystem node.", kdb_cmd_node);
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
 * Open a handle to a file or directory.
 *
 * Opens a handle to a regular file or directory, optionally creating it if it
 * doesn't exist. If the entry does not exist, it will be created as a regular
 * file. To create a directory, use kern_dir_create().
 *
 * @param path		Path to file or directory to open.
 * @param rights	Requested access rights for the handle.
 * @param flags		Behaviour flags for the handle.
 * @param create	Whether to create the file. If 0, the file will not be
 *			created if it doesn't exist. If FILE_CREATE, it will be
 *			created if it doesn't exist. If FILE_CREATE_ALWAYS, it
 *			will always be created, and an error will be returned
 *			if it already exists.
 * @param security	If creating the file, the security attributes to give
 *			to it. If NULL, default security attributes will be
 *			used.
 * @param handlep	Where to store created handle.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_file_open(const char *path, object_rights_t rights, int flags, int create,
                        const object_security_t *security, handle_t *handlep) {
	object_security_t ksecurity = { -1, -1, NULL };
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

	if(security) {
		/* Don't bother copying anything provided if we aren't going
		 * to use it. */
		if(create != 0) {
			ret = object_security_from_user(&ksecurity, security, false);
			if(ret != STATUS_SUCCESS) {
				kfree(kpath);
				return ret;
			}
		} else {
			security = NULL;
		}
	}

	ret = file_open(kpath, rights, flags, create, (security) ? &ksecurity : NULL, &handle);
	if(ret != STATUS_SUCCESS) {
		object_security_destroy(&ksecurity);
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(handle, NULL, 0, NULL, handlep);
	object_handle_release(handle);
	object_security_destroy(&ksecurity);
	kfree(kpath);
	return ret;
}

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur from the file
 * handle's current offset, and before returning the offset will be incremented
 * by the number of bytes read.
 *
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read data into.
 * @param count		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	size_t bytes = 0;
	void *kbuf;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to read. */
	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_WAIT for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = file_read(khandle, kbuf, count, &bytes);
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

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. The read will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
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
status_t kern_file_pread(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	size_t bytes = 0;
	void *kbuf;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to read. */
	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_WAIT for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}

	/* Perform the actual read. */
	ret = file_pread(khandle, kbuf, count, offset, &bytes);
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

/**
 * Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the file
 * handle's current offset (if the FILE_APPEND flag is set, the offset will be
 * set to the end of the file and the write will take place there), and before
 * returning the handle's offset will be incremented by the number of bytes
 * written.
 *
 * @param handle	Handle to file to write to. Must have the 
 *			FILE_RIGHT_WRITE access right.
 * @param buf		Buffer to write data from.
 * @param count		Number of bytes to write. The supplied buffer should be
 *			at least this size. If zero, the function will return
 *			after checking all arguments, and the file handle
 *			offset will not be modified (even if FILE_APPEND is
 *			set).
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	void *kbuf = NULL;
	size_t bytes = 0;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to write. */
	if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_WAIT for
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
	ret = file_write(khandle, kbuf, count, &bytes);
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

/**
 * Write to a file.
 *
 * Writes data from a buffer into a file. The write will occur at the specified
 * offset, and the handle's offset will be ignored and not modified.
 *
 * @param handle	Handle to file to write to. Must have the 
 *			FILE_RIGHT_WRITE access right.
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
status_t kern_file_pwrite(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	void *kbuf = NULL;
	size_t bytes = 0;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to write. */
	if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_WAIT for
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
	ret = file_pwrite(khandle, kbuf, count, offset, &bytes);
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

/**
 * Modify the size of a file.
 *
 * Modifies the size of a file in the file system. If the new size is smaller
 * than the previous size of the file, then the extra data is discarded. If
 * it is larger than the previous size, then the extended space will be filled
 * with zero bytes.
 *
 * @param handle	Handle to file to resize. Must have the FILE_RIGHT_WRITE
 *			access right.
 * @param size		New size of the file.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_file_resize(handle_t handle, offset_t size) {
	object_handle_t *khandle;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = file_resize(khandle, size);
	object_handle_release(khandle);
	return ret;
}

/**
 * Set the offset of a file handle.
 *
 * Modifies the offset of a file handle according to the specified action, and
 * returns the new offset. For directories, the offset is the index of the next
 * directory entry that will be read.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FILE_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_file_seek(handle_t handle, int action, rel_offset_t offset, offset_t *newp) {
	object_handle_t *khandle;
	status_t ret;
	offset_t new;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = file_seek(khandle, action, offset, &new);
	if(ret == STATUS_SUCCESS && newp) {
		ret = memcpy_to_user(newp, &new, sizeof(*newp));
	}
	object_handle_release(khandle);
	return ret;
}

/** Get information about a file or directory.
 * @param handle	Handle to file/directory to get information for.
 * @param infop		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t kern_file_info(handle_t handle, file_info_t *infop) {
	object_handle_t *khandle;
	file_info_t kinfo;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = file_info(khandle, &kinfo);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(infop, &kinfo, sizeof(*infop));
	}
	object_handle_release(khandle);
	return ret;
}

/** Flush changes to a file to the FS.
 * @param handle	Handle to file to flush.
 * @return		Status code describing result of the operation. */
status_t kern_file_sync(handle_t handle) {
	object_handle_t *khandle;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = file_sync(khandle);
	object_handle_release(khandle);
	return ret;
}

/** Create a directory in the file system.
 * @param path		Path to directory to create.
 * @param security	Security attributes for the directory. If NULL, default
 *			security attributes will be used.
 * @return		Status code describing result of the operation. */
status_t kern_dir_create(const char *path, const object_security_t *security) {
	object_security_t ksecurity = { -1, -1, NULL };
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	if(security) {
		ret = object_security_from_user(&ksecurity, security, false);
		if(ret != STATUS_SUCCESS) {
			kfree(kpath);
			return ret;
		}
	}

	ret = dir_create(kpath, (security) ? &ksecurity : NULL);
	object_security_destroy(&ksecurity);
	kfree(kpath);
	return ret;
}

/**
 * Read a directory entry.
 *
 * Reads a single directory entry structure from a directory into a buffer. As
 * the structure length is variable, a buffer size argument must be provided
 * to ensure that the buffer isn't overflowed. The number of the entry read
 * will be the handle's current offset, and upon success the handle's offset
 * will be incremented by 1.
 *
 * @param handle	Handle to directory to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, the function will
 *			return STATUS_TOO_SMALL).
 *
 * @return		Status code describing result of the operation. If the
 *			handle's offset is past the end of the directory,
 *			STATUS_NOT_FOUND will be returned.
 */
status_t kern_dir_read(handle_t handle, dir_entry_t *buf, size_t size) {
	object_handle_t *khandle;
	dir_entry_t *kbuf;
	status_t ret;

	if(!size) {
		return STATUS_TOO_SMALL;
	}

	ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_WAIT for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(size, 0);
	if(!kbuf) {
		object_handle_release(khandle);
		return STATUS_NO_MEMORY;
	}

	/* Perform the actual read. */
	ret = dir_read(khandle, kbuf, size);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(buf, kbuf, kbuf->length);
	}

	kfree(kbuf);
	object_handle_release(khandle);
	return ret;
}

/** Create a symbolic link.
 * @param path		Path to symbolic link to create.
 * @param target	Target for the symbolic link (does not have to exist).
 *			If the path is relative, it is relative to the
 *			directory containing the link.
 * @return		Status code describing result of the operation. */
status_t kern_symlink_create(const char *path, const char *target) {
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

	ret = symlink_create(kpath, ktarget);
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
status_t kern_symlink_read(const char *path, char *buf, size_t size) {
	char *kpath, *kbuf;
	status_t ret;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Allocate a buffer to read into. See comment in sys_fs_file_read()
	 * about not using MM_WAIT. */
	kbuf = kmalloc(size, 0);
	if(!kbuf) {
		kfree(kpath);
		return STATUS_NO_MEMORY;
	}

	ret = symlink_read(kpath, kbuf, size);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(buf, kbuf, size);
	}

	kfree(kpath);
	kfree(kbuf);
	return ret;
}

/**
 * Mount a filesystem.
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
status_t kern_fs_mount(const char *dev, const char *path, const char *type, const char *opts) {
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

/** Get information on mounted filesystems.
 * @param infop		Array of mount information structures to fill in. If
 *			NULL, the function will only return the number of
 *			mounted filesystems.
 * @param countp	If infop is not NULL, this should point to a value
 *			containing the size of the provided array. Upon
 *			successful completion, the value will be updated to
 *			be the number of structures filled in. If infop is NULL,
 *			the number of mounted filesystems will be stored here.
 * @return		Status code describing result of the operation. */
status_t kern_fs_mount_info(mount_info_t *infop, size_t *countp) {
	mount_info_t *info = NULL;
	size_t i = 0, count = 0;
	fs_mount_t *mount;
	status_t ret;
	char *path;

	if(!cap_check(NULL, CAP_FS_MOUNT)) {
		return STATUS_PERM_DENIED;
	}

	if(infop) {
		ret = memcpy_from_user(&count, countp, sizeof(count));
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(!count) {
			return STATUS_SUCCESS;
		}

		info = kmalloc(sizeof(*info), MM_WAIT);
	}

	mutex_lock(&mounts_lock);

	LIST_FOREACH(&mount_list, iter) {
		if(infop) {
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

			ret = memcpy_to_user(&infop[i], info, sizeof(*info));
			if(ret != STATUS_SUCCESS) {
				kfree(info);
				mutex_unlock(&mounts_lock);
				return ret;
			}

			if(++i >= count) {
				break;
			}
		} else {
			i++;
		}
	}

	mutex_unlock(&mounts_lock);
	if(infop) {
		kfree(info);
	}
	return memcpy_to_user(countp, &i, sizeof(i));
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
status_t kern_fs_sync(void) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to store in.
 * @param size		Size of buffer.
 * @return		Status code describing result of the operation. */
status_t kern_fs_getcwd(char *buf, size_t size) {
	status_t ret;
	size_t len;
	char *path;

	if(!buf || !size) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&curr_proc->ioctx.lock);

	ret = fs_node_path(curr_proc->ioctx.curr_dir, curr_proc->ioctx.root_dir, &path);
	if(ret != STATUS_SUCCESS) {
		rwlock_unlock(&curr_proc->ioctx.lock);
		return ret;
	}

	rwlock_unlock(&curr_proc->ioctx.lock);

	len = strlen(path);
	if(len >= size) {
		ret = STATUS_TOO_SMALL;
	} else {
		ret = memcpy_to_user(buf, path, len + 1);
	}
	kfree(path);
	return ret;
}

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		Status code describing result of the operation. */
status_t kern_fs_setcwd(const char *path) {
	fs_node_t *node;
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_node_lookup(kpath, true, FILE_TYPE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->obj, NULL) & FILE_RIGHT_EXECUTE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_ACCESS_DENIED;
	}

	/* Attempt to set. Release the node no matter what, as upon success it
	 * is referenced by io_context_setcwd(). */
	ret = io_context_setcwd(&curr_proc->ioctx, node);
	fs_node_release(node);
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
status_t kern_fs_setroot(const char *path) {
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

	ret = fs_node_lookup(kpath, true, FILE_TYPE_DIR, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	/* Must have execute permission to use as working directory. */
	if(!(object_rights(&node->obj, NULL) & FILE_RIGHT_EXECUTE)) {
		fs_node_release(node);
		kfree(kpath);
		return STATUS_ACCESS_DENIED;
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
 * @param infop		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t kern_fs_info(const char *path, bool follow, file_info_t *infop) {
	file_info_t kinfo;
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_info(kpath, follow, &kinfo);
	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(infop, &kinfo, sizeof(*infop));
	}
	kfree(kpath);
	return ret;
}

/** Obtain security attributes for a filesystem entry.
 * @note		This call is used internally by libkernel, and not
 *			exported from it, as it provides a wrapper around it
 *			that handles ACL memory allocation automatically, and
 *			puts everything into an object_security_t structure.
 * @param path		Path to entry to get security attributes for.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param uidp		Where to store owning user ID.
 * @param gidp		Where to store owning group ID.
 * @param aclp		Where to store ACL. The structure referred to by this
 *			pointer must be initialized prior to calling the
 *			function. If the entries pointer in the structure is
 *			NULL, then the function will store the number of
 *			entries in the ACL in the count entry and do nothing
 *			else. Otherwise, at most the number of entries specified
 *			by the count entry will be copied to the entries
 *			array, and the count will be updated to give the actual
 *			number of entries in the ACL.
 * @return		Status code describing result of the operation. */
status_t kern_fs_security(const char *path, bool follow, user_id_t *uidp, group_id_t *gidp,
                          object_acl_t *aclp) {
	object_acl_t kacl;
	fs_node_t *node;
	status_t ret;
	size_t count;
	char *kpath;

	if(!uidp && !gidp && !aclp) {
		return STATUS_INVALID_ARG;
	}

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	if(aclp) {
		ret = memcpy_from_user(&kacl, aclp, sizeof(*aclp));
		if(ret != STATUS_SUCCESS) {
			kfree(kpath);
			return ret;
		}
	}

	ret = fs_node_lookup(kpath, follow, -1, &node);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	rwlock_read_lock(&node->obj.lock);

	if(uidp) {
		ret = memcpy_to_user(uidp, &node->obj.uid, sizeof(*uidp));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
	if(gidp) {
		ret = memcpy_to_user(gidp, &node->obj.gid, sizeof(*gidp));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
	if(aclp) {
		/* If entries pointer is NULL, the caller wants us to give the
		 * number of entries in the ACL. Otherwise, copy at most the
		 * number of entries specified. */
		if(kacl.entries) {
			count = MIN(kacl.count, node->obj.uacl.count);
			if(count) {
				ret = memcpy_to_user(kacl.entries,
				                     node->obj.uacl.entries,
				                     sizeof(*kacl.entries) * count);
				if(ret != STATUS_SUCCESS) {
					goto out;
				}
			}
		}

		/* Copy back the number of ACL entries. */
		ret = memcpy_to_user(&aclp->count, &node->obj.uacl.count, sizeof(aclp->count));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	ret = STATUS_SUCCESS;
out:
	rwlock_unlock(&node->obj.lock);
	fs_node_release(node);
	kfree(kpath);
	return ret;
}

/**
 * Set security attributes for a filesystem entry.
 *
 * Sets the security attributes (owning user/group and ACL) of a filesystem
 * entry. The calling process must either be the owner of the entry, or have
 * the CAP_FS_ADMIN capability.
 *
 * A process without the CAP_CHANGE_OWNER capability cannot set an owning user
 * ID different to its user ID, or set the owning group ID to that of a group
 * it does not belong to.
 *
 * @param path		Path to entry to set security attributes of.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param security	Security attributes to set. If the user ID is -1, it
 *			will not be changed. If the group ID is -1, it will not
 *			be changed. If the ACL pointer is NULL, the ACL will
 *			not be changed.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_fs_set_security(const char *path, bool follow, const object_security_t *security) {
	object_security_t ksecurity;
	fs_node_t *node;
	status_t ret;
	char *kpath;

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = object_security_from_user(&ksecurity, security, false);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = fs_node_lookup(kpath, follow, -1, &node);
	if(ret != STATUS_SUCCESS) {
		object_security_destroy(&ksecurity);
		kfree(kpath);
		return ret;
	}

	ret = object_set_security(&node->obj, &ksecurity);
	fs_node_release(node);
	object_security_destroy(&ksecurity);
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

	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = fs_unlink(kpath);
	kfree(kpath);
	return ret;
}

status_t kern_fs_rename(const char *source, const char *dest) {
	return STATUS_NOT_IMPLEMENTED;
}
