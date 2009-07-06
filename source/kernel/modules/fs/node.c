/* Kiwi VFS node functions
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
 * @brief		VFS node functions.
 *
 * This file contains the bulk of the interface that the VFS exposes to other
 * modules. This includes functions for looking up nodes on the filesystem,
 * and reading/modifying those nodes, as well as for creating new filesystem
 * nodes.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/cache.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/pmm.h>
#include <mm/slab.h>

#include <assert.h>

#include "vfs_priv.h"

/*
 * Node cache functions.
 */

/** Filesystem node slab cache. */
static slab_cache_t *vfs_node_cache;

/** VFS node object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_ctor(void *obj, void *data, int kmflag) {
	vfs_node_t *node = (vfs_node_t *)obj;

	list_init(&node->header);
	mutex_init(&node->lock, "vfs_node_lock", 0);
	refcount_set(&node->count, 0);
	radix_tree_init(&node->children);

	return 0;
}

/** VFS node cache reclaim callback.
 * @param data		Cache data (unused). */
static void vfs_node_cache_reclaim(void *data) {
	dprintf("vfs: performing reclaim of unused nodes...\n");
	vfs_mount_reclaim_nodes();
}

/** Allocate a node structure and set one reference on it.
 * @param name		Name to give the node (can be NULL).
 * @param mount		Mount that the node resides on.
 * @param mmflag	Allocation flags.
 * @return		Pointer to node on success, NULL on failure (always
 *			succeeds if MM_SLEEP is specified). */
vfs_node_t *vfs_node_alloc(const char *name, vfs_mount_t *mount, int mmflag) {
	vfs_node_t *node;

	node = slab_cache_alloc(vfs_node_cache, mmflag);
	if(node == NULL) {
		return NULL;
	}

	node->type = VFS_NODE_DIR;
	node->mount = mount;
	node->flags = 0;
	node->cache = NULL;
	node->size = 0;
	node->dirty = false;
	node->parent = NULL;

	/* Set the node name if it is supplied. */
	if(name != NULL) {
		node->name = kstrdup(name, mmflag);
		if(node->name == NULL) {
			slab_cache_free(vfs_node_cache, node);
			return NULL;
		}
	} else {
		node->name = NULL;
	}

	refcount_inc(&node->count);
	return node;
}

/** Free a node structure.
 * @param node		Node to free.
 * @param destroy	Destroy the node even if it is persistent.
 * @return		0 on success, 1 if node persistent, negative error
 *			code on failure (this can happen, for example, if an
 *			error occurs flushing the node data). */
int vfs_node_free(vfs_node_t *node, bool destroy) {
	int ret;

	/* Lock the parent first, so we ensure that the node is not
	 * being searched for. This prevents a deadlock: lock node, lock
	 * lock parent, block because parent is locked while node is
	 * being searched for, search locks node, blocks, deadlock. */
	if(node->parent) {
		mutex_lock(&node->parent->lock, 0);
	}
	mutex_lock(&node->lock, 0);

	assert(refcount_get(&node->count) == 0);
	assert(radix_tree_empty(&node->children));

	/* If the node is required to remain cached, do nothing. */
	if(node->flags & VFS_NODE_PERSISTENT && !destroy) {
		if(node->parent) {
			mutex_unlock(&node->parent->lock);
		}
		mutex_unlock(&node->lock);
		return 1;
	}

	/* Destroy the cache if there is one. Do this first as its the only
	 * step that can fail, so we want to do it before messing around with
	 * anything else. */
	if(node->cache != NULL) {
		ret = cache_destroy(node->cache);
		if(ret != 0) {
			kprintf(LOG_NORMAL,"vfs: warning: failed to destroy node cache for %p(%s): %d\n",
			        node, node->name, ret);
			return ret;
		}
	}

	/* Remove the node from its mount list. Do not lock the mount here
	 * because this function should be called with the mount lock held,
	 * or when it is not attached to anything. */
	list_remove(&node->header);

	/* Detach from parent node and then unlock it. */
	if(node->parent) {
		radix_tree_remove(&node->parent->children, node->name);
		refcount_dec(&node->parent->count);
		mutex_unlock(&node->parent->lock);
	}

	dprintf("vfs: freed node %p(%s) (parent: %p, mount: %p)\n", node, node->name,
		node->parent, node->mount);

	/* Free any name string. */
	if(node->name) {
		kfree(node->name);
		node->name = NULL;
	}

	mutex_unlock(&node->lock);
	slab_cache_free(vfs_node_cache, node);
	return 0;
}

/** Initialize the filesystem node cache. */
void vfs_node_cache_init(void) {
	vfs_node_cache = slab_cache_create("vfs_node_cache", sizeof(vfs_node_t),
	                                   0, vfs_node_ctor, NULL, vfs_node_cache_reclaim,
	                                   NULL, NULL, 0, MM_SLEEP);
}

/*
 * Page cache operations.
 */

/** Get a missing page from a cache. Node should be locked.
 * @todo		Nonblocking reads. Needs a change to the cache layer.
 * @param cache		Cache to get page from.
 * @param offset	Offset of page in data source.
 * @param addrp		Where to store address of page obtained.
 * @return		0 on success, negative error code on failure. */
static int vfs_cache_get_page(cache_t *cache, offset_t offset, phys_ptr_t *addrp) {
	vfs_node_t *node = cache->data;
	phys_ptr_t page;
	void *mapping;
	int ret;

	/* First try to allocate a page to use. */
	if(node->mount && node->mount->type->page_get) {
		ret = node->mount->type->page_get(node, offset, MM_SLEEP, &page);
		if(ret != 0) {
			return ret;
		}
	} else {
		page = pmm_alloc(1, MM_SLEEP | PM_ZERO);
	}

	/* Now try to fill it in, if an operation is provided to do so. */
	if(node->mount && node->mount->type->page_read) {
		mapping = page_phys_map(page, PAGE_SIZE, MM_SLEEP);
		ret = node->mount->type->page_read(node, mapping, offset, false);

		/* Unmap immediately before handling failure. */
		page_phys_unmap(mapping, PAGE_SIZE);

		if(ret != 0) {
			return ret;
		}
	}

	*addrp = page;
	return 0;
}

/** Flush changes to a page to the filesystem.
 * @param cache		Cache that the page is in.
 * @param page		Address of page to flush.
 * @param offset	Offset of page in data source.
 * @return		0 on success, 1 if page has no source to flush to,
 *			negative error code on failure. */
static int vfs_cache_flush_page(cache_t *cache, phys_ptr_t page, offset_t offset) {
	vfs_node_t *node = cache->data;
	void *mapping;
	int ret;

	if(node->mount && node->mount->type->page_flush) {
		mapping = page_phys_map(page, PAGE_SIZE, MM_SLEEP);
		ret = node->mount->type->page_flush(node, mapping, offset, false);
		page_phys_unmap(mapping, PAGE_SIZE);

		return ret;
	} else {
		return 1;
	}
}

/** Free a page from a VFS cache (page will have been flushed).
 * @param cache		Cache that the page is in.
 * @param page		Address of page to free.
 * @param offset	Offset of page in data source. */
static void vfs_cache_free_page(cache_t *cache, phys_ptr_t page, offset_t offset) {
	vfs_node_t *node = cache->data;

	if(node->mount && node->mount->type->page_free) {
		node->mount->type->page_free(node, page);
	} else {
		pmm_free(page, 1);
	}
}

/** VFS page cache operations. */
static cache_ops_t vfs_cache_ops = {
	.get_page = vfs_cache_get_page,
	.flush_page = vfs_cache_flush_page,
	.free_page = vfs_cache_free_page,
};

/** Get and map a page from a node's page cache.
 * @param node		Node to get page from.
 * @param offset	Offset of page to get.
 * @param addrp		Where to store address of mapping.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_page_get(vfs_node_t *node, offset_t offset, void **addrp) {
	phys_ptr_t page;
	int ret;

	ret = cache_get(node->cache, offset, &page);
	if(ret != 0) {
		return ret;
	}

	*addrp = page_phys_map(page, PAGE_SIZE, MM_SLEEP);
	return 0;
}

/** Unmap and release a page from a node's page cache.
 * @param node		Node to release page in.
 * @param addr		Address of mapping.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied. */
static void vfs_node_page_release(vfs_node_t *node, void *addr, offset_t offset, bool dirty) {
	page_phys_unmap(addr, PAGE_SIZE);
	cache_release(node->cache, offset, dirty);
}

/*
 * Public interface.
 */

/** Find a child of a node.
 * @param parent	Node to get child of (should be locked).
 * @param name		Name of child node to look for.
 * @param childp	Where to store pointer to child node.
 * @return		0 on success, negative error code if not found or on
 *			other errors. */
static int vfs_node_child_find(vfs_node_t *parent, const char *name, vfs_node_t **childp) {
	vfs_node_t *node;
	int ret;

	/* Check if we have the node cached. */
	node = radix_tree_lookup(&parent->children, name);
	if(node) {
		mutex_lock(&node->lock, 0);

		/* Increase reference count and remove from unused node list
		 * if the count has gone up from zero. */
		if(refcount_inc(&node->count) == 1) {
			mutex_lock(&node->mount->lock, 0);
			list_remove(&node->header);
			mutex_unlock(&node->mount->lock);
		}

		*childp = node;
		return 0;
	}

	/* Node isn't cached, we must go through the filesystem backend to
	 * get the node. If the type does not provide a lookup operation,
	 * then we have nothing more to do. */
	if(!parent->mount->type->node_find) {
		return -ERR_OBJ_NOT_FOUND;
	}

	/* Allocate a new node structure. */
	node = vfs_node_alloc(name, parent->mount, MM_SLEEP);

	/* Get the filesystem backend to fill in the node. */
	ret = parent->mount->type->node_find(parent, node);
	if(ret != 0) {
		refcount_dec(&node->count);
		vfs_node_free(node, true);
		return ret;
	}

	/* Create a cache for the node if necessary. */
	if(node->type == VFS_NODE_REGULAR) {
		node->cache = cache_create(&vfs_cache_ops, node);
	}

	mutex_lock(&node->lock, 0);

	/* Attach the node to the parent. Parent is locked by the caller. */
	node->parent = parent;
	refcount_inc(&parent->count);
	radix_tree_insert(&parent->children, node->name, node);

	*childp = node;
	return 0;
}

/** Internal part of node lookup.
 * @param from		Node to begin lookup at.
 * @param path		Path to look up, relative to starting node. This string
 *			will be modified.
 * @param nodep		Where to store address of node looked up.
 * @return		0 on success, negative error code on failure. */
static int vfs_node_lookup_internal(vfs_node_t *from, char *path, vfs_node_t **nodep) {
	vfs_node_t *node = from, *parent;
	char *tok;
	int ret;

	/* Loop through the path, finding each element until we reach the
	 * end of the string. */
	while(true) {
		tok = strsep(&path, "/");

		if(node->type == VFS_NODE_SYMLINK) {
			ret = -ERR_NOT_IMPLEMENTED;
			goto fail;
		}

		if(tok == NULL) {
			/* The last token was the last token of the path,
			 * return the node we're currently on. */
			mutex_unlock(&node->lock);
			*nodep = node;
			return 0;
		} else if(node->type != VFS_NODE_DIR) {
			/* The previous token was not a directory: this means
			 * the path is trying to treat a non-directory as a
			 * directory. Reject this. */
			ret = -ERR_OBJ_TYPE_INVAL;
			goto fail;
		} else if(tok[0] == '.' && tok[1] == '.' && tok[2] == 0) {
			/* Move up to the parent node, if any. If the parent
			 * pointer is NULL, we are at the top of a mount,
			 * so see if there is a mountpoint we can move to. */
			if(node->parent || (node->mount->mountpoint && node->mount->mountpoint->parent)) {
				parent = (node->parent) ? node->parent : node->mount->mountpoint->parent;

				/* Do not need to check on unused lists because
				 * the parent is guaranteed not to be on any
				 * when it has children. */
				refcount_inc(&parent->count);

				/* Release the node we are currently on. */
				mutex_unlock(&node->lock);
				vfs_node_release(node);

				/* Move up and take the lock, do not take the
				 * parent lock first for the reason specified
				 * in vfs_node_free(). */
				node = parent;
				mutex_lock(&node->lock, 0);
			}
		} else if((tok[0] == '.' && tok[1] == 0) || tok[0] == 0) {
			/* A dot character or a zero-length token mean the
			 * current directory, do nothing. */
		} else {
			parent = node;

			/* Attempt to get a child out of the directory. */
			ret = vfs_node_child_find(parent, tok, &node);
			if(ret != 0) {
				goto fail;
			}

			/* No need to go into vfs_node_release() here because
			 * vfs_node_find_child() succeeded, meaning parent will
			 * not need to return to an unused list. */
			refcount_dec(&parent->count);
			mutex_unlock(&parent->lock);
		}
	}
fail:
	mutex_unlock(&node->lock);
	vfs_node_release(node);
	return ret;
}

/** Look up a filesystem node.
 *
 * Looks up a node within the filesystem. The lookup will be done relative
 * to the provided starting node. If it is specified as NULL, the root node
 * will be used. If not, there must be sufficient references on the supplied
 * node to ensure that it does not get freed before this function references
 * it.
 *
 * @note		The node returned will have a reference on, so
 *			vfs_node_release() must be called when it is no
 *			longer required.
 *
 * @param from		Node to begin lookup at (NULL means root of FS).
 * @param path		Path to look up - components are seperated with a /
 *			character.
 * @param nodep		Where to store address of node looked up.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_lookup(vfs_node_t *from, const char *path, vfs_node_t **nodep) {
	vfs_node_t *node;
	char *dup;
	int ret;

	if(!path || path[0] == '/' || !nodep) {
		return -ERR_PARAM_INVAL;
	}

	/* Work out where we're starting the lookup from. */
	if(from != NULL) {
		mutex_lock(&from->lock, 0);

		if(from->type != VFS_NODE_DIR) {
			mutex_unlock(&from->lock);
			return -ERR_OBJ_TYPE_INVAL;
		}

		/* Increase the reference count to ensure that the node does
		 * not get freed. */
		vfs_node_get(from);
	} else {
		mutex_lock(&vfs_root_mount->root->lock, 0);
		vfs_node_get(vfs_root_mount->root);
		from = vfs_root_mount->root;
	}

	node = from;

	/* Create a duplicate of the path string as we modify it internally. */
	dup = kstrdup(path, MM_SLEEP);

	/* Perform the actual lookup, free duplicated string and return. */
	ret = vfs_node_lookup_internal(from, dup, nodep);
	kfree(dup);
	return ret;
}
MODULE_EXPORT(vfs_node_lookup);

/** Place a reference on a node.
 *
 * Increases the reference count of a node to signal that it is being used and
 * should not be freed. Each call to this should be matched with a call to
 * vfs_node_release() to remove the reference.
 *
 * @param node		Node to reference.
 */
void vfs_node_get(vfs_node_t *node) {
	/* This should not be called if the reference count is 0. */
	assert(refcount_get(&node->count) != 0);

	refcount_inc(&node->count);
}
MODULE_EXPORT(vfs_node_get);

/** Remove a reference from a node.
 *
 * Decreases the reference count of a filesystem node structure. This should
 * be called when a node obtained via vfs_node_lookup() is no longer needed,
 * or when a reference added by vfs_node_get() is no longer required.
 *
 * @param node		Node to release.
 */
void vfs_node_release(vfs_node_t *node) {
	if(refcount_dec(&node->count) > 0) {
		return;
	}

	dprintf("vfs: node %p(%s) is now unused, released\n", node, (node->name) ? node->name : "");

	if(node->mount) {
		/* Add the node to the appropriate unused list. */
		mutex_lock(&node->mount->lock, 0);
		list_append((node->dirty) ? &node->mount->dirty_nodes : &node->mount->unused_nodes, &node->header);
		mutex_unlock(&node->mount->lock);
	} else {
		/* Node is not attached anywhere, free it up. */
		assert(!node->parent);
		vfs_node_free(node, true);
	}
}
MODULE_EXPORT(vfs_node_release);

/** Create a new node on the filesystem.
 *
 * Creates a new node on the filesystem of the specified type. Currently, can
 * only create regular nodes and directories.
 *
 * @param parent	Parent node to create under (must be directory).
 * @param name		Name to give new node.
 * @param type		Type of node.
 * @param nodep		Where to store pointer to node structure. If this is
 *			NULL, then the node will just be created. Otherwise,
 *			it will be stored here with a reference on it.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_create(vfs_node_t *parent, const char *name, vfs_node_type_t type, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	if(!parent || !name) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&parent->lock, 0);

	/* Parent must be a directory (obviously). Also, reject the call if
	 * the filesystem type does not allow creation of new nodes. */
	if(parent->type != VFS_NODE_DIR) {
		mutex_unlock(&parent->lock);
		return -ERR_OBJ_TYPE_INVAL;
	} else if(!parent->mount->type->node_create) {
		mutex_unlock(&parent->lock);
		return -ERR_NOT_SUPPORTED;
	}

	/* Now find out if a node with the given name already exists. */
	ret = vfs_node_child_find(parent, name, &node);
	if(ret != -ERR_OBJ_NOT_FOUND) {
		if(ret == 0) {
			/* Node was found. Must free it up again. */
			mutex_unlock(&node->lock);
			vfs_node_release(node);
			return -ERR_OBJ_EXISTS;
		} else {
			return ret;
		}
	}

	/* Node doesn't exist, we can proceed. Create a new node structure
	 * to track the new node. */
	node = vfs_node_alloc(name, parent->mount, MM_SLEEP);
	node->type = type;

	/* Get the filesystem backend to create the node. */
	ret = parent->mount->type->node_create(parent, node);
	if(ret != 0) {
		refcount_dec(&node->count);
		vfs_node_free(node, true);
		return ret;
	}

	/* Create a cache for the node if necessary. */
	if(node->type == VFS_NODE_REGULAR) {
		node->cache = cache_create(&vfs_cache_ops, node);
	}

	mutex_lock(&node->lock, 0);

	/* Attach the node to the parent. */
	node->parent = parent;
	refcount_inc(&parent->count);
	radix_tree_insert(&parent->children, node->name, node);

	mutex_unlock(&parent->lock);

	dprintf("vfs: created node %p(%s) under %p(%s) (type: %d)\n",
	        node, node->name, parent, (parent->name) ? parent->name : "", type);

	/* Store pointer to child if required. */
	if(nodep) {
		*nodep = node;
	} else {
		vfs_node_release(node);
	}
	mutex_unlock(&node->lock);
	return 0;
}
MODULE_EXPORT(vfs_node_create);

/** Read from a filesystem node.
 *
 * Reads data from a filesystem node into a buffer.
 *
 * @param node		Node to read from.
 * @param buffer	Buffer to read data into. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to read.
 * @param offset	Offset within the file to read from.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even if the call fails, as it can fail
 *			when part of the data has been read.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_read(vfs_node_t *node, void *buffer, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	int ret;

	if(!node || !buffer) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Check if the node is a suitable type. */
	if(node->type != VFS_NODE_REGULAR) {
		ret = -ERR_OBJ_TYPE_INVAL;
		goto out;
	}

	/* Ensure that we do not go pass the end of the node. */
	if(offset > (offset_t)node->size) {
		ret = 0;
		goto out;
	} else if((offset + (offset_t)count) > (offset_t)node->size) {
		count = (size_t)((offset_t)node->size - offset);
	}

	/* It is not an error to pass a zero count, just return silently if
	 * this happens, however do it after all the other checks so we do
	 * return errors where appropriate. */
	if(count == 0) {
		ret = 0;
		goto out;
	}

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(buffer, mapping + (offset % PAGE_SIZE), size);
		vfs_node_page_release(node, mapping, start, false);
		total += size; buffer += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buffer += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		memcpy(buffer, mapping, PAGE_SIZE);
		vfs_node_page_release(node, mapping, start, false);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		memcpy(buffer, mapping, count);
		vfs_node_page_release(node, mapping, start, false);
		total += count;
	}

	dprintf("vfs: read %zu bytes from offset 0x%" PRIx64 " in %p(%s)\n",
	        total, offset, node, node->name);
	ret = 0;
out:
	mutex_unlock(&node->lock);
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}
MODULE_EXPORT(vfs_node_read);

/** Write to a filesystem node.
 *
 * Writes data from a buffer into a filesystem node.
 *
 * @param node		Node to write to.
 * @param buffer	Buffer to write data from. Must be at least count
 *			bytes long.
 * @param count		Number of bytes to write.
 * @param offset	Offset within the file to write to.
 * @param bytesp	Where to store number of bytes written (optional).
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_write(vfs_node_t *node, const void *buffer, size_t count, offset_t offset, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	int ret;

	if(!node || !buffer) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&node->lock, 0);

	/* Check if the node is a suitable type, and if it's on a writeable
	 * filesystem. */
	if(node->type != VFS_NODE_REGULAR) {
		ret = -ERR_OBJ_TYPE_INVAL;
		goto out;
	} else if(node->mount && node->mount->flags & VFS_MOUNT_RDONLY) {
		ret = -ERR_OBJ_READ_ONLY;
		goto out;
	}

	/* Attempt to resize the node if necessary. */
	if((offset + (offset_t)count) > (offset_t)node->size) {
		/* If the resize operation is not provided, we can only write
		 * within the space that we have. */
		if(!node->mount || !node->mount->type->node_resize) {
			if(offset > (offset_t)node->size) {
				ret = 0;
				goto out;
			} else {
				count = (size_t)((offset_t)node->size - offset);
			}
		} else {
			ret = node->mount->type->node_resize(node, (file_size_t)(offset + count));
			if(ret != 0) {
				goto out;
			}

			node->size = (file_size_t)(offset + count);
		}
	}

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(mapping + (offset % PAGE_SIZE), buffer, size);
		vfs_node_page_release(node, mapping, start, true);
		total += size; buffer += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buffer += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		memcpy(mapping, buffer, PAGE_SIZE);
		vfs_node_page_release(node, mapping, start, true);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		ret = vfs_node_page_get(node, start, &mapping);
		if(ret != 0) {
			goto out;
		}

		memcpy(mapping, buffer, count);
		vfs_node_page_release(node, mapping, start, true);
		total += count;
	}

	dprintf("vfs: wrote %zu bytes to offset 0x%" PRIx64 " in %p(%s)\n",
	        total, offset, node, node->name);
	ret = 0;
out:
	mutex_unlock(&node->lock);
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}
MODULE_EXPORT(vfs_node_write);

/*
 * Special node types.
 */

/** Create a special node backed by a chunk of memory.
 *
 * Creates a special VFS node structure that is backed by the specified chunk
 * of memory. This is useful to pass data stored in memory to code that expects
 * to be operating on filesystem nodes, such as the program loader module.
 *
 * When the node is created, the data in the given memory area is duplicated
 * into the node's data cache, so updates to the memory area after this
 * function has be called will not show on reads from the node. Similarly,
 * writes to the node will not be written back to the memory area.
 *
 * The node is not attached anywhere in the filesystem, and therefore once its
 * reference count reaches 0, it will be immediately destroyed.
 *
 * @param name		Name to give node.
 * @param memory	Pointer to memory area to use.
 * @param size		Size of memory area.
 * @param nodep		Where to store pointer to created node.
 *
 * @return		0 on success, negative error code on failure.
 */ 
int vfs_node_create_from_memory(const char *name, const void *memory, size_t size, vfs_node_t **nodep) {
	vfs_node_t *node;
	int ret;

	if(!memory || !size || !nodep) {
		return -ERR_PARAM_INVAL;
	}

	node = vfs_node_alloc(name, NULL, MM_SLEEP);
	node->type = VFS_NODE_REGULAR;
	node->size = size;
	node->cache = cache_create(&vfs_cache_ops, node);

	/* Write the data into the node. */
	ret = vfs_node_write(node, memory, size, 0, NULL);
	if(ret != 0) {
		vfs_node_release(node);
		return ret;
	}

	*nodep = node;
	return 0;
}
MODULE_EXPORT(vfs_node_create_from_memory);

/*
 * Address space backends.
 */

/** Get a missing page from a private VFS cache.
 * @param cache		Cache to get page from.
 * @param offset	Offset of page in data source.
 * @param addrp		Where to store address of page obtained.
 * @return		0 on success, negative error code on failure. */
static int vfs_aspace_private_cache_get_page(cache_t *cache, offset_t offset, phys_ptr_t *addrp) {
	vfs_node_t *node = cache->data;
	void *source, *dest;
	phys_ptr_t page;
	int ret;

	/* Get the source page from the node's cache. */
	ret = vfs_node_page_get(node, offset, &source);
	if(ret != 0) {
		return ret;
	}

	/* Allocate a page, map it in and copy the data across. */
	page = pmm_alloc(1, MM_SLEEP);
	dest = page_phys_map(page, PAGE_SIZE, MM_SLEEP);
	memcpy(dest, source, PAGE_SIZE);
	page_phys_unmap(dest, PAGE_SIZE);
	vfs_node_page_release(node, source, offset, false);

	*addrp = page;
	return 0;
}

/** Free a page from a private VFS cache.
 * @param cache		Cache that the page is in.
 * @param page		Address of page to free.
 * @param offset	Offset of page in data source. */
static void vfs_aspace_private_cache_free_page(cache_t *cache, phys_ptr_t page, offset_t offset) {
	pmm_free(page, 1);
}

/** Clean up any data associated with a private VFS cache.
 * @param cache		Cache being destroyed. */
static void vfs_aspace_private_cache_destroy(cache_t *cache) {
	vfs_node_release(cache->data);
}

/** VFS private page cache operations. */
static cache_ops_t vfs_aspace_private_cache_ops = {
	.get_page = vfs_aspace_private_cache_get_page,
	.free_page = vfs_aspace_private_cache_free_page,
	.destroy = vfs_aspace_private_cache_destroy,
};

/** Get a page from a private VFS source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @param addrp		Where to store address of page.
 * @return		0 on success, negative error code on failure. */
static int vfs_aspace_private_get(aspace_source_t *source, offset_t offset, phys_ptr_t *addrp) {
	return cache_get(source->data, offset, addrp);
}

/** Release a page in a private VFS source.
 * @param source	Source to release page in.
 * @param offset	Offset into the source.
 * @return		Pointer to page allocated, or NULL on failure. */
static void vfs_aspace_private_release(aspace_source_t *source, offset_t offset) {
	cache_release(source->data, offset, true);
}

/** Destroy data in a private VFS source.
 * @param source	Source to destroy. */
static void vfs_aspace_private_destroy(aspace_source_t *source) {
	if(cache_destroy(source->data) != 0) {
		/* Shouldn't happen as we don't do any page flushing. */
		fatal("Failed to destroy private VFS cache");
	}
}

/** VFS private address space backend structure. */
static aspace_backend_t vfs_aspace_private_backend = {
	.get = vfs_aspace_private_get,
	.release = vfs_aspace_private_release,
	.destroy = vfs_aspace_private_destroy,
};

/** Check whether a source can be mapped using the given parameters.
 * @param source	Source being mapped.
 * @param offset	Offset of the mapping in the source.
 * @param size		Size of the mapping.
 * @param flags		Flags the mapping is being created with.
 * @return		0 if mapping allowed, negative error code explaining
 *			why it is not allowed if not. */
static int vfs_aspace_shared_map(aspace_source_t *source, offset_t offset, size_t size, int flags) {
	vfs_node_t *node = source->data;

	if(flags & AS_REGION_WRITE && node->mount->flags & VFS_MOUNT_RDONLY) {
		return -ERR_OBJ_READ_ONLY;
	}

	return 0;
}

/** Get a page from a shared VFS source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @param addrp		Where to store address of page.
 * @return		0 on success, negative error code on failure. */
static int vfs_aspace_shared_get(aspace_source_t *source, offset_t offset, phys_ptr_t *addrp) {
	vfs_node_t *node = source->data;

	assert(node->cache);
	return cache_get(node->cache, offset, addrp);
}

/** Release a page in a shared VFS source.
 * @param source	Source to release page in.
 * @param offset	Offset into the source.
 * @return		Pointer to page allocated, or NULL on failure. */
static void vfs_aspace_shared_release(aspace_source_t *source, offset_t offset) {
	vfs_node_t *node = source->data;

	assert(node->cache);
	cache_release(node->cache, offset, true);
}

/** Destroy data in a shared VFS source.
 * @param source	Source to destroy. */
static void vfs_aspace_shared_destroy(aspace_source_t *source) {
	vfs_node_release(source->data);
}

/** VFS shared address space backend structure. */
static aspace_backend_t vfs_aspace_shared_backend = {
	.map = vfs_aspace_shared_map,
	.get = vfs_aspace_shared_get,
	.release = vfs_aspace_shared_release,
	.destroy = vfs_aspace_shared_destroy,
};

/** Create an address space source for a VFS node.
 *
 * Creates an address space source for a VFS node. If the AS_SOURCE_PRIVATE
 * flag is specified, modifications made to pages from the source will not be
 * propagated to other sources for the node, or to the file itself. Otherwise,
 * modifications made to pages from the source will be propagated back to the
 * file and other non-private sources.
 *
 * @param node		Node to create source for.
 * @param flags		Behaviour flags for the source.
 * @param sourcep	Where to store pointer to source.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_node_aspace_create(vfs_node_t *node, int flags, aspace_source_t **sourcep) {
	aspace_source_t *source;

	if(!node || !sourcep) {
		return -ERR_PARAM_INVAL;
	} else if(node->type != VFS_NODE_REGULAR) {
		return -ERR_OBJ_TYPE_INVAL;
	}

	/* Node should have a cache if it is a regular file. */
	assert(node->cache);

	/* Reference the node to ensure it does not get freed while the
	 * source is in existence. */
	vfs_node_get(node);

	source = aspace_source_alloc(node->name, flags, MM_SLEEP);
	if(flags & AS_SOURCE_PRIVATE) {
		source->backend = &vfs_aspace_private_backend;
		source->data = cache_create(&vfs_aspace_private_cache_ops, node);
	} else {
		source->backend = &vfs_aspace_shared_backend;
		source->data = node;
	}

	*sourcep = source;
	return 0;
}
MODULE_EXPORT(vfs_node_aspace_create);
