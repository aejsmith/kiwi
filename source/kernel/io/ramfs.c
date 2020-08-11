/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               RAM-based temporary filesystem.
 */

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

/** Node information structure. */
typedef struct ramfs_node {
    union {
        vm_cache_t *cache;          /**< Data cache. */
        char *target;               /**< Symbolic link destination. */
    };

    atomic_t links;                 /**< Link count. */
    nstime_t created;               /**< Time of creation. */
    nstime_t accessed;              /**< Time of last access. */
    nstime_t modified;              /**< Time last modified. */
} ramfs_node_t;

/** Mount information structure. */
typedef struct ramfs_mount {
    atomic_t next_id;               /**< Next node ID. */
} ramfs_mount_t;

/** Root node ID. */
#define RAMFS_ROOT_NODE 0

/** Free a ramfs node.
 * @param node          Node to free. */
static void ramfs_node_free(fs_node_t *node) {
    ramfs_node_t *data = node->private;

    /* Destroy the data caches. */
    switch (node->file.type) {
    case FILE_TYPE_REGULAR:
        vm_cache_destroy(data->cache, true);
        break;
    case FILE_TYPE_SYMLINK:
        kfree(data->target);
        break;
    default:
        break;
    }

    kfree(data);
}

/** Create a new ramfs node.
 * @param parent        Directory to create in.
 * @param entry         Entry structure for the new entry.
 * @param _node         Node structure.
 * @param target        For symbolic links, the target of the link.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_create(
    fs_node_t *_parent, fs_dentry_t *entry, fs_node_t *_node,
    const char *target)
{
    ramfs_mount_t *mount = _parent->mount->private;
    ramfs_node_t *parent = _parent->private;
    ramfs_node_t *node;

    assert(_parent->file.type == FILE_TYPE_DIR);

    node = kmalloc(sizeof(*node), MM_KERNEL);
    atomic_set(&node->links, 1);
    node->created = node->accessed = node->modified = unix_time();

    /* Allocate a unique ID for the node. */
    _node->id = entry->id = atomic_inc(&mount->next_id);
    _node->ops = _parent->ops;
    _node->private = node;

    switch (_node->file.type) {
    case FILE_TYPE_REGULAR:
        node->cache = vm_cache_create(0, NULL, NULL);
        break;
    case FILE_TYPE_SYMLINK:
        node->target = kstrdup(target, MM_KERNEL);
        break;
    case FILE_TYPE_DIR:
        /* Our link count should include the '.' entry to ourself, and the
         * parent's should include one for our '..' entry. */
        atomic_inc(&node->links);
        atomic_inc(&parent->links);
        break;
    default:
        kfree(node);
        return STATUS_NOT_SUPPORTED;
    }

    /* We exist entirely in the cache, so we should not free our unused nodes. */
    fs_node_set_flag(_node, FS_NODE_KEEP);
    entry->flags |= FS_NODE_KEEP;

    return STATUS_SUCCESS;
}

/** Create a hard link.
 * @param _parent       Directory to create link in.
 * @param entry         Entry structure for the new entry.
 * @param _node         Existing node to link to.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_link(fs_node_t *_parent, fs_dentry_t *entry, fs_node_t *_node) {
    ramfs_node_t *node = _node->private;

    if (atomic_inc(&node->links) == 0)
        fs_node_clear_flag(_node, FS_NODE_REMOVED);

    return STATUS_SUCCESS;
}

/** Unlink a ramfs node.
 * @param _parent       Directory containing the entry to remove.
 * @param entry         Entry in the directory being removed.
 * @param _node         Node that the entry refers to.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_unlink(fs_node_t *_parent, fs_dentry_t *entry, fs_node_t *_node) {
    ramfs_node_t *parent = _parent->private;
    ramfs_node_t *node = _node->private;
    int32_t val;

    /* For directories, the FS layer checks whether its cache is empty before
     * calling into this function to save a call out to the FS when it already
     * knows that the directory is not empty. Therefore, we don't need to do a
     * check here. */
    val = atomic_dec(&node->links);

    if (_node->file.type == FILE_TYPE_DIR) {
        /* Drop an extra link on ourself for the '.' entry, and one on the
         * parent for the '..' entry. */
        atomic_dec(&parent->links);
        val = atomic_dec(&node->links);
    }

    if (val == 1)
        fs_node_set_flag(_node, FS_NODE_REMOVED);

    return STATUS_SUCCESS;
}

/** Get information about a ramfs node.
 * @param _node         Node to get information on.
 * @param info          Information structure to fill in. */
static void ramfs_node_info(fs_node_t *_node, file_info_t *info) {
    ramfs_node_t *node = _node->private;

    info->links = atomic_get(&node->links);
    info->block_size = PAGE_SIZE;
    info->created = node->created;
    info->accessed = node->accessed;
    info->modified = node->modified;

    switch (_node->file.type) {
    case FILE_TYPE_REGULAR:
        info->size = node->cache->size;
        break;
    case FILE_TYPE_SYMLINK:
        info->size = strlen(node->target);
        break;
    default:
        info->size = 0;
        break;
    }
}

/** Resize a ramfs file.
 * @param _node         Node to resize.
 * @param size          New size of the node.
 * @return              Always returns STATUS_SUCCESS. */
static status_t ramfs_node_resize(fs_node_t *_node, offset_t size) {
    ramfs_node_t *node = _node->private;

    assert(_node->file.type == FILE_TYPE_REGULAR);

    vm_cache_resize(node->cache, size);
    node->modified = unix_time();
    return STATUS_SUCCESS;
}

/** Read the destination of a ramfs symbolic link.
 * @param _node         Node to read from.
 * @param _target       Where to store pointer to string containing link target.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_read_symlink(fs_node_t *_node, char **_target) {
    ramfs_node_t *node = _node->private;

    assert(_node->file.type == FILE_TYPE_SYMLINK);

    *_target = kstrdup(node->target, MM_KERNEL);
    return STATUS_SUCCESS;
}

/** Perform I/O on a ramfs file.
 * @param handle        File handle structure.
 * @param request       I/O request.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_io(file_handle_t *handle, io_request_t *request) {
    ramfs_node_t *node = handle->node->private;
    offset_t end;
    status_t ret;

    assert(handle->file->type == FILE_TYPE_REGULAR);

    if (request->op == IO_OP_WRITE) {
        end = request->offset + request->total;
        if (end > node->cache->size)
            vm_cache_resize(node->cache, end);
    }

    ret = vm_cache_io(node->cache, request);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (request->op == IO_OP_WRITE && request->transferred)
        node->modified = unix_time();

    return STATUS_SUCCESS;
}

/** Get the data cache for a ramfs file.
 * @param handle        File handle structure.
 * @return              Pointer to node's VM cache. */
static vm_cache_t *ramfs_node_get_cache(file_handle_t *handle) {
    ramfs_node_t *node = handle->node->private;

    assert(handle->file->type == FILE_TYPE_REGULAR);
    return node->cache;
}

/** Read a ramfs directory entry.
 * @param handle        File handle structure.
 * @param _entry        Where to store pointer to directory entry structure.
 * @return              Status code describing result of the operation. */
static status_t ramfs_node_read_dir(file_handle_t *handle, dir_entry_t **_entry) {
    dir_entry_t *entry;
    const char *name;
    node_id_t id;
    size_t len;
    offset_t i;
    fs_dentry_t *child;

    assert(handle->file->type == FILE_TYPE_DIR);

    mutex_lock(&handle->entry->lock);

    /* Our entire directory structure is stored in the directory cache. To read
     * the entries in a ramfs directory, we iterate over the child entries for
     * the entry used to open the directory handle (with special cases for the
     * "." and ".." entries, as these do not exist in the directory cache). */
    if (handle->offset == 0) {
        name = ".";
        id = handle->entry->id;
    } else if (handle->offset == 1) {
        name = "..";
        id = (handle->entry->parent) ? handle->entry->parent->id : handle->entry->id;
    } else {
        name = NULL;

        i = 2;
        radix_tree_foreach(&handle->entry->entries, iter) {
            child = radix_tree_entry(iter, fs_dentry_t);

            if (i++ == handle->offset) {
                name = child->name;
                id = child->id;
                break;
            }
        }

        if (!name) {
            mutex_unlock(&handle->entry->lock);
            return STATUS_NOT_FOUND;
        }
    }

    len = strlen(name) + 1;
    entry = kmalloc(sizeof(*entry) + len, MM_KERNEL);
    entry->length = sizeof(*entry) + len;
    entry->id = id;
    memcpy(entry->name, name, len);

    *_entry = entry;
    mutex_unlock(&handle->entry->lock);
    handle->offset++;
    return STATUS_SUCCESS;
}

/** Node operations structure. */
static fs_node_ops_t ramfs_node_ops = {
    .free = ramfs_node_free,
    .create = ramfs_node_create,
    .link = ramfs_node_link,
    .unlink = ramfs_node_unlink,
    .info = ramfs_node_info,
    .resize = ramfs_node_resize,
    .read_symlink = ramfs_node_read_symlink,
    .io = ramfs_node_io,
    .get_cache = ramfs_node_get_cache,
    .read_dir = ramfs_node_read_dir,
};

/** Unmount a ramfs.
 * @param mount         Mount that's being unmounted. */
static void ramfs_unmount(fs_mount_t *mount) {
    kfree(mount->private);
}

/** Read a node from a ramfs.
 * @param mount         Mount to obtain node from.
 * @param _node         Node structure to fill in with node details.
 * @return              Status code describing result of the operation. */
static status_t ramfs_read_node(fs_mount_t *mount, fs_node_t *_node) {
    ramfs_node_t *node;

    /* This is a special case to get the root node, we will be called
     * immediately after ramfs_mount(). All other nodes are created by
     * ramfs_node_create() and will exist in the node cache until they are
     * removed, so this function should never be called. */
    if (unlikely(_node->id != RAMFS_ROOT_NODE))
        fatal("Should not be here (%" PRIu16 ":%" PRIu64 ")", mount, _node->id);

    node = kmalloc(sizeof(*node), MM_KERNEL);
    node->links = 1;
    node->created = node->accessed = node->modified = unix_time();

    _node->file.type = FILE_TYPE_DIR;
    _node->ops = &ramfs_node_ops;
    _node->private = node;
    fs_node_set_flag(_node, FS_NODE_KEEP);

    return STATUS_SUCCESS;
}

/** Mount operations structure. */
static fs_mount_ops_t ramfs_mount_ops = {
    .unmount = ramfs_unmount,
    .read_node = ramfs_read_node,
};

/** Mount a ramfs filesystem.
 * @param _mount        Mount structure for the FS.
 * @param opts          Array of mount options.
 * @param count         Number of options.
 * @return              Status code describing result of the operation. */
static status_t ramfs_mount(fs_mount_t *_mount, fs_mount_option_t *opts, size_t count) {
    ramfs_mount_t *mount;

    mount = kmalloc(sizeof(*mount), MM_KERNEL);
    atomic_set(&mount->next_id, 1);

    _mount->ops = &ramfs_mount_ops;
    _mount->private = mount;
    _mount->root->id = RAMFS_ROOT_NODE;
    _mount->root->flags |= FS_DENTRY_KEEP;
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
    if (ret != STATUS_SUCCESS)
        fatal("Could not register ramfs filesystem type (%d)", ret);
}

INITCALL(ramfs_init);
