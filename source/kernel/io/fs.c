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
 * @brief               Filesystem layer.
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
 *
 * TODO:
 *  - Locking could possibly be improved. There may end up being quite a bit of
 *    contention on various locks. Might be able to convert dentry locks to
 *    rwlocks.
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
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Filesystem lookup behaviour flags. */
#define FS_LOOKUP_FOLLOW    (1<<0)      /**< If final path component is a symlink, follow it. */
#define FS_LOOKUP_LOCK      (1<<1)      /**< Return a locked entry. */

static file_ops_t fs_file_ops;

/** List of registered FS types (protected by fs_mount_lock). */
static LIST_DEFINE(fs_types);

/** List of all mounts. */
static mount_id_t next_mount_id = 1;
static LIST_DEFINE(fs_mount_list);
static MUTEX_DEFINE(fs_mount_lock, 0);

/** Caches of filesystem structures. */
static slab_cache_t *fs_node_cache;
static slab_cache_t *fs_dentry_cache;

/** Unused directory entries. */
static LIST_DEFINE(unused_entries);
static SPINLOCK_DEFINE(unused_entries_lock);
static size_t unused_entry_count;

/** Unused nodes. */
static LIST_DEFINE(unused_nodes);
static SPINLOCK_DEFINE(unused_nodes_lock);
static size_t unused_node_count;

/** Mount at the root of the filesystem. */
fs_mount_t *root_mount;

/** Look up a filesystem type (fs_mount_lock must be held). */
static fs_type_t *fs_type_lookup(const char *name) {
    list_foreach(&fs_types, iter) {
        fs_type_t *type = list_entry(iter, fs_type_t, header);

        if (strcmp(type->name, name) == 0)
            return type;
    }

    return NULL;
}

/** Register a new filesystem type.
 * @param type          Pointer to type structure to register.
 * @return              Status code describing result of the operation. */
status_t fs_type_register(fs_type_t *type) {
    /* Check whether the structure is valid. */
    if (!type || !type->name || !type->description || !type->mount)
        return STATUS_INVALID_ARG;

    mutex_lock(&fs_mount_lock);

    /* Check if this type already exists. */
    if (fs_type_lookup(type->name)) {
        mutex_unlock(&fs_mount_lock);
        return STATUS_ALREADY_EXISTS;
    }

    refcount_set(&type->count, 0);
    list_init(&type->header);
    list_append(&fs_types, &type->header);

    kprintf(LOG_NOTICE, "fs: registered filesystem type %s (%s)\n", type->name, type->description);
    mutex_unlock(&fs_mount_lock);
    return STATUS_SUCCESS;
}

/**
 * Removes a previously registered filesystem type. Will not succeed if the
 * filesystem type is in use by any mounts.
 *
 * @param type          Type to remove.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_type_unregister(fs_type_t *type) {
    mutex_lock(&fs_mount_lock);

    /* Check that the type is actually there. */
    if (fs_type_lookup(type->name) != type) {
        mutex_unlock(&fs_mount_lock);
        return STATUS_NOT_FOUND;
    } else if (refcount_get(&type->count) > 0) {
        mutex_unlock(&fs_mount_lock);
        return STATUS_IN_USE;
    }

    list_remove(&type->header);
    mutex_unlock(&fs_mount_lock);
    return STATUS_SUCCESS;
}

/** Look up a mount by ID (fs_mount_lock must be held). */
static fs_mount_t *fs_mount_lookup(mount_id_t id) {
    list_foreach(&fs_mount_list, iter) {
        fs_mount_t *mount = list_entry(iter, fs_mount_t, header);
        if (mount->id == id)
            return mount;
    }

    return NULL;
}

/**
 * Node functions.
 */

/** Allocate a node structure (reference count will be set to 1). */
static fs_node_t *fs_node_alloc(fs_mount_t *mount) {
    fs_node_t *node = slab_cache_alloc(fs_node_cache, MM_KERNEL);

    refcount_set(&node->count, 1);
    list_init(&node->unused_link);

    node->file.ops = &fs_file_ops;
    node->flags    = 0;
    node->mount    = mount;

    return node;
}

/**
 * Frees an unused node structure. The node's mount must be locked. If the node
 * is not marked as removed, the node's flush operation will be called, and the
 * node will not be freed if this fails. Removed nodes will always be freed
 * without error.
 *
 * @param node          Node to free.
 *
 * @return              Status code describing result of the operation. Cannot
 *                      fail if node has FS_NODE_REMOVED set.
 */
static status_t fs_node_free(fs_node_t *node) {
    fs_mount_t *mount = node->mount;
    status_t ret;

    assert(refcount_get(&node->count) == 0);
    assert(mutex_held(&mount->lock));

    if (!fs_node_is_read_only(node) && !(node->flags & FS_NODE_REMOVED)) {
        if (node->ops->flush) {
            ret = node->ops->flush(node);
            if (ret != STATUS_SUCCESS)
                return ret;
        }
    }

    /* May still be on the unused list if freeing via fs_unmount(). */
    if (!list_empty(&node->unused_link)) {
        spinlock_lock(&unused_nodes_lock);
        unused_node_count--;
        list_remove(&node->unused_link);
        spinlock_unlock(&unused_nodes_lock);
    }

    if (node->ops->free)
        node->ops->free(node);

    avl_tree_remove(&mount->nodes, &node->tree_link);

    dprintf("fs: freed node %" PRIu16 ":%" PRIu64 " (%p)\n", mount->id, node->id, node);
    slab_cache_free(fs_node_cache, node);
    return STATUS_SUCCESS;
}

/** Releases a node.
 * @param node          Node to release. */
static void fs_node_release(fs_node_t *node) {
    fs_mount_t *mount = node->mount;

    if (refcount_dec(&node->count) > 0)
        return;

    /* Recheck after locking in case somebody has taken the node. */
    mutex_lock(&mount->lock);
    if (refcount_get(&node->count) > 0) {
        mutex_unlock(&mount->lock);
        return;
    }

    if (node->flags & FS_NODE_REMOVED) {
        /* Free the node straight away if it is removed. */
        fs_node_free(node);
    } else if (!(node->flags & FS_NODE_KEEP)) {
        /* Move to the unused list so that it can be reclaimed. */
        spinlock_lock(&unused_nodes_lock);
        assert(list_empty(&node->unused_link));
        unused_node_count++;
        list_append(&unused_nodes, &node->unused_link);
        spinlock_unlock(&unused_nodes_lock);

        dprintf(
            "fs: transferred node %" PRIu16 ":%" PRIu64 " (%p) to unused list\n",
            mount->id, node->id, node);
    }

    mutex_unlock(&mount->lock);
}

/** Gets information about a node.
 * @param node          Node to get information for.
 * @param info          Structure to store information in. */
static void fs_node_info(fs_node_t *node, file_info_t *info) {
    memset(info, 0, sizeof(*info));

    assert(node->ops->info);
    node->ops->info(node, info);

    info->id    = node->id;
    info->mount = node->mount->id;
    info->type  = node->file.type;
}

/**
 * Directory cache functions.
 */

static void fs_dentry_ctor(void *obj, void *data) {
    fs_dentry_t *entry = obj;

    mutex_init(&entry->lock, "fs_dentry_lock", 0);
    radix_tree_init(&entry->entries);
    list_init(&entry->mount_link);
    list_init(&entry->unused_link);
}

/** Allocate a new directory entry structure (reference count will be set to 0). */
static fs_dentry_t *fs_dentry_alloc(const char *name, fs_mount_t *mount, fs_dentry_t *parent) {
    fs_dentry_t *entry = slab_cache_alloc(fs_dentry_cache, MM_KERNEL);

    refcount_set(&entry->count, 0);

    entry->flags   = 0;
    entry->name    = kstrdup(name, MM_KERNEL);
    entry->mount   = mount;
    entry->node    = NULL;
    entry->parent  = parent;
    entry->mounted = NULL;

    return entry;
}

/** Free a directory entry structure. */
static void fs_dentry_free(fs_dentry_t *entry) {
    radix_tree_clear(&entry->entries, NULL);
    kfree(entry->name);
    slab_cache_free(fs_dentry_cache, entry);
}

/** Increase the reference count of a directory entry.
 * @note                Should not be used on unused entries.
 * @param entry         Entry to increase reference count of. */
void fs_dentry_retain(fs_dentry_t *entry) {
    if (unlikely(refcount_inc(&entry->count) == 1))
        fatal("Retaining unused directory entry %p ('%s')\n", entry, entry->name);
}

/** Decrease the reference count of a locked directory entry.
 * @param entry         Entry to decrease reference count of. Will be unlocked
 *                      upon return. */
static void fs_dentry_release_locked(fs_dentry_t *entry) {
    if (refcount_dec(&entry->count) > 0) {
        mutex_unlock(&entry->lock);
        return;
    }

    assert(entry->node);
    assert(!entry->mounted);

    fs_node_release(entry->node);
    entry->node = NULL;

    /* If the parent is NULL, that means the entry has been unlinked, therefore
     * we should free it immediately. */
    if (!entry->parent) {
        dprintf(
            "fs: freed entry '%s' (%p) on mount %" PRIu16 "\n",
            entry->name, entry, entry->mount->id);

        mutex_unlock(&entry->lock);
        fs_dentry_free(entry);
        return;
    }

    /* Add to the mount unused list. This is done regardless of the keep flag
     * as the purpose this list serves is to aid in cleanup when unmounting,
     * and when doing so we want to free all entries. */
    mutex_lock(&entry->mount->lock);
    list_append(&entry->mount->unused_entries, &entry->mount_link);
    mutex_unlock(&entry->mount->lock);

    if (!(entry->flags & FS_DENTRY_KEEP)) {
        /* Move to the global unused list so it can be reclaimed. */
        spinlock_lock(&unused_entries_lock);
        assert(list_empty(&entry->unused_link));
        unused_entry_count++;
        list_append(&unused_entries, &entry->unused_link);
        spinlock_unlock(&unused_entries_lock);
    }

    mutex_unlock(&entry->lock);
}

/** Decrease the reference count of a directory entry.
 * @param entry         Entry to decrease reference count of. */
void fs_dentry_release(fs_dentry_t *entry) {
    mutex_lock(&entry->lock);
    fs_dentry_release_locked(entry);
}

/** Instantiate a directory entry.
 * @param entry         Entry to instantiate. Will be locked upon return if
 *                      successful.
 * @return              Status code describing result of the operation. */
static status_t fs_dentry_instantiate(fs_dentry_t *entry) {
    status_t ret;

    mutex_lock(&entry->lock);

    if (refcount_inc(&entry->count) != 1) {
        assert(entry->node);
        return STATUS_SUCCESS;
    }

    fs_mount_t *mount = entry->mount;
    mutex_lock(&mount->lock);

    /* Check if the node is cached in the mount. */
    fs_node_t *node = avl_tree_lookup(&mount->nodes, entry->id, fs_node_t, tree_link);
    if (node) {
        if (refcount_inc(&node->count) == 1) {
            if (!(node->flags & FS_NODE_KEEP)) {
                spinlock_lock(&unused_nodes_lock);
                assert(!list_empty(&node->unused_link));
                unused_node_count--;
                list_remove(&node->unused_link);
                spinlock_unlock(&unused_nodes_lock);
            }
        }
    } else {
        /* Node is not cached, we must read it from the filesystem. */
        assert(mount->ops->read_node);

        node = fs_node_alloc(entry->mount);
        node->id = entry->id;

        ret = mount->ops->read_node(mount, node);
        if (ret != STATUS_SUCCESS) {
            slab_cache_free(fs_node_cache, node);
            refcount_dec(&entry->count);

            /* This may have been a newly created entry from fs_dentry_lookup().
             * In this case we must put the entry onto the unused list as it
             * will not have been put there to begin with. */
            list_append(&mount->unused_entries, &entry->mount_link);
            if (!(entry->flags & FS_DENTRY_KEEP)) {
                spinlock_lock(&unused_entries_lock);

                if (list_empty(&entry->unused_link))
                    unused_entry_count++;

                list_append(&unused_entries, &entry->unused_link);

                spinlock_unlock(&unused_entries_lock);
            }

            mutex_unlock(&mount->lock);
            mutex_unlock(&entry->lock);
            return ret;
        }

        /* Attach the node to the node tree. */
        avl_tree_insert(&mount->nodes, node->id, &node->tree_link);
    }

    list_append(&mount->used_entries, &entry->mount_link);

    if (!(entry->flags & FS_DENTRY_KEEP)) {
        spinlock_lock(&unused_entries_lock);
        assert(!list_empty(&entry->unused_link));
        unused_entry_count--;
        list_remove(&entry->unused_link);
        spinlock_unlock(&unused_entries_lock);
    }

    mutex_unlock(&mount->lock);
    entry->node = node;
    return STATUS_SUCCESS;
}

/**
 * Looks up a child entry in a directory, looking it up on the filesystem if it
 * cannot be found. This function does not handle '.' and '..' entries, an
 * assertion exists to check that these are not passed. Symbolic links are not
 * followed.
 *
 * @param parent        Entry to look up in (must be instantiated and locked).
 * @param name          Name of the entry to look up.
 * @param _entry        Where to store pointer to entry structure.
 *                      Will not be instantiated, call fs_dentry_instantiate()
 *                      after successful return.
 *
 * @return              Status code describing the result of the operation.
 */
static status_t fs_dentry_lookup(fs_dentry_t *parent, const char *name, fs_dentry_t **_entry) {
    assert(mutex_held(&parent->lock));
    assert(parent->node);
    assert(strcmp(name, ".") != 0);
    assert(strcmp(name, "..") != 0);

    fs_dentry_t *entry = radix_tree_lookup(&parent->entries, name);
    if (!entry) {
        if (!parent->node->ops->lookup)
            return STATUS_NOT_FOUND;

        entry = fs_dentry_alloc(name, parent->mount, parent);

        status_t ret = parent->node->ops->lookup(parent->node, entry);
        if (ret != STATUS_SUCCESS) {
            fs_dentry_free(entry);
            return ret;
        }

        radix_tree_insert(&parent->entries, name, entry);
    }

    *_entry = entry;
    return STATUS_SUCCESS;
}

/** Look up an entry in the filesystem.
 * @param path          Path string to look up (will be modified).
 * @param entry         Instantiated entry to begin lookup at (NULL for current
 *                      working directory). Will be released upon return.
 * @param flags         Lookup behaviour flags.
 * @param nest          Symbolic link nesting count.
 * @param _entry        Where to store pointer to entry found (referenced,
 *                      and locked).
 * @return              Status code describing result of the operation. */
static status_t fs_lookup_internal(
    char *path, fs_dentry_t *entry, unsigned flags, unsigned nest,
    fs_dentry_t **_entry)
{
    status_t ret;

    if (path[0] == '/') {
        /* Drop the entry we were provided, if any. */
        if (entry)
            fs_dentry_release(entry);

        /* Strip off all '/' characters at the start of the path. */
        while (path[0] == '/')
            path++;

        /* Start from the root directory of the current process. */
        assert(curr_proc->io.root_dir);
        entry = curr_proc->io.root_dir;
        fs_dentry_retain(entry);

        if (path[0] || flags & FS_LOOKUP_LOCK)
            mutex_lock(&entry->lock);

        /* Return the root if we've reached the end of the path. */
        if (!path[0]) {
            *_entry = entry;
            return STATUS_SUCCESS;
        }
    } else {
        if (!entry) {
            /* Start from the current working directory. */
            assert(curr_proc->io.curr_dir);
            entry = curr_proc->io.curr_dir;
            fs_dentry_retain(entry);
        }

        mutex_lock(&entry->lock);
    }

    /* Loop through each element of the path string. The starting entry should
     * already be instantiated. */
    fs_dentry_t *prev = NULL;
    while (true) {
        assert(entry->node);
        fs_node_t *node = entry->node;

        char *tok = strsep(&path, "/");

        /* If the current entry is a symlink and this is not the last element
         * of the path, or the caller wishes to follow the link, follow it. */
        bool follow = tok || flags & FS_LOOKUP_FOLLOW;
        if (node->file.type == FILE_TYPE_SYMLINK && follow) {
            /* The previous entry should be the link's parent. */
            assert(prev);
            assert(prev == entry->parent);

            if (++nest > FS_NESTED_LINK_MAX) {
                ret = STATUS_SYMLINK_LIMIT;
                goto err_release_prev;
            }

            assert(node->ops->read_symlink);

            char *link;
            ret = node->ops->read_symlink(node, &link);
            if (ret != STATUS_SUCCESS)
                goto err_release_prev;

            dprintf(
                "fs: following symbolic link '%s' (%" PRIu16 ":%" PRIu64
                ") in '%s' (%" PRIu64 ":%" PRIu16 ") to '%s' (nest: %u)\n",
                entry->name, entry->mount->id, node->id, prev->name,
                prev->mount->id, prev->node->id, nest);

            /* Don't need this entry any more. The previous iteration of the
             * loop left a reference on the previous entry. */
            fs_dentry_release_locked(entry);

            /* Recurse to find the link destination. The check above ensures we
             * do not infinitely recurse. TODO: although we have a limit on
             * this, perhaps it would be better to avoid recursion altogether. */
            ret = fs_lookup_internal(link, prev, FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK, nest, &entry);
            if (ret != STATUS_SUCCESS) {
                kfree(link);
                return ret;
            }

            /* Entry is locked and instantiated upon return. */
            assert(entry->node);
            node = entry->node;

            dprintf(
                "fs: followed '%s' to '%s' (%" PRIu16 ":%" PRIu64 ")\n",
                link, entry->name, entry->mount->id, node->id);

            kfree(link);
        } else if (node->file.type == FILE_TYPE_SYMLINK) {
            /* Release the previous entry. */
            fs_dentry_release(prev);
        }

        if (!tok) {
            /* The last token was the last element of the path string, return
             * the entry we're currently on. */
            if (!(flags & FS_LOOKUP_LOCK))
                mutex_unlock(&entry->lock);

            *_entry = entry;
            return STATUS_SUCCESS;
        } else if (node->file.type != FILE_TYPE_DIR) {
            /* The previous token was not a directory: this means the path
             * string is trying to treat a non-directory as a directory. Reject
             * this. */
            ret = STATUS_NOT_DIR;
            goto err_release;
        } else if (!tok[0] || (tok[0] == '.' && !tok[1])) {
            /* Zero-length path component or current directory, do nothing. */
            continue;
        }

        /* We're trying to descend into the directory, check for execute
         * permission. */
        if (!file_access(&node->file, FILE_ACCESS_EXECUTE)) {
            ret = STATUS_ACCESS_DENIED;
            goto err_release;
        }

        prev = entry;

        if (tok[0] == '.' && tok[1] == '.' && !tok[2]) {
            /* Do not allow the lookup to ascend past the process' root
             * directory. */
            if (entry == curr_proc->io.root_dir)
                continue;

            assert(entry != root_mount->root);

            if (entry == entry->mount->root) {
                /* We're at the root of the mount. The entry parent pointer is
                 * NULL in this case. Move over onto the mountpoint's parent. */
                entry = entry->mount->mountpoint->parent;
            } else {
                entry = entry->parent;
            }
        } else {
            /* Try to find the entry in the child. */
            ret = fs_dentry_lookup(entry, tok, &entry);
            if (ret != STATUS_SUCCESS)
                goto err_release;

            if (entry->mounted)
                entry = entry->mounted->root;
        }

        mutex_unlock(&prev->lock);

        ret = fs_dentry_instantiate(entry);
        if (ret != STATUS_SUCCESS) {
            fs_dentry_release(prev);
            return ret;
        }

        /* Do not release the previous entry if the new node is a symbolic link,
         * as the symbolic link lookup requires it. */
        if (entry->node->file.type != FILE_TYPE_SYMLINK)
            fs_dentry_release(prev);
    }

err_release_prev:
    fs_dentry_release(prev);

err_release:
    fs_dentry_release_locked(entry);
    return ret;
}

/**
 * Looks up an entry in the filesystem. If the path is a relative path (one
 * that does not begin with a '/' character), then it will be looked up
 * relative to the current directory in the current process' I/O context.
 * Otherwise, the starting '/' character will be taken off and the path will be
 * looked up relative to the current I/O context's root.
 *
 * @param path          Path string to look up.
 * @param flags         Lookup behaviour flags.
 * @param _entry        Where to store pointer to entry found (instantiated).
 *
 * @return              Status code describing result of the operation.
 */
static status_t fs_lookup(const char *path, unsigned flags, fs_dentry_t **_entry) {
    assert(path);
    assert(_entry);

    if (!path[0])
        return STATUS_INVALID_ARG;

    /* Take the I/O context lock for reading across the entire lookup to prevent
     * other threads from changing the root directory of the process while the
     * lookup is being performed. */
    rwlock_read_lock(&curr_proc->io.lock);

    /* Duplicate path so that fs_lookup_internal() can modify it. */
    char *dup = kstrdup(path, MM_KERNEL);

    /* Look up the path string. */
    status_t ret = fs_lookup_internal(dup, NULL, flags, 0, _entry);
    kfree(dup);
    rwlock_unlock(&curr_proc->io.lock);
    return ret;
}

/** Get the path to a directory entry. */
static status_t fs_dentry_path(fs_dentry_t *entry, char **_path) {
    rwlock_read_lock(&curr_proc->io.lock);

    /* Loop through until we reach the root. */
    char *buf = NULL, *tmp;
    size_t total = 0;
    while (entry != curr_proc->io.root_dir && entry != root_mount->root) {
        if (entry == entry->mount->root)
            entry = entry->mount->mountpoint;

        size_t len = strlen(entry->name);
        total += (buf) ? len + 1 : len;

        tmp = kmalloc(total + 1, MM_KERNEL);
        memcpy(tmp, entry->name, len + 1);
        if (buf) {
            tmp[len] = '/';
            strcpy(&tmp[len + 1], buf);
            kfree(buf);
        }
        buf = tmp;

        /* It is safe for us to go through the tree without locking or
         * referencing. Because we have a reference to the starting entry, none
         * of the parent entries will be freed. */
        entry = entry->parent;
        if (!entry) {
            /* Unlinked entry. */
            rwlock_unlock(&curr_proc->io.lock);
            return STATUS_NOT_FOUND;
        }
    }

    rwlock_unlock(&curr_proc->io.lock);

    /* Prepend a '/'. */
    tmp = kmalloc((++total) + 1, MM_KERNEL);
    tmp[0] = '/';
    if (buf) {
        memcpy(&tmp[1], buf, total);
        kfree(buf);
    } else {
        tmp[1] = 0;
    }

    *_path = tmp;
    return STATUS_SUCCESS;
}

/**
 * Internal implementation functions.
 */

/** Prepare to create a filesystem entry.
 * @param path          Path to node to create.
 * @param _entry        Where to store pointer to created directory entry
 *                      structure. This can then be used to create the entry on
 *                      the filesystem. Its parent will be instantiated and
 *                      locked.
 * @return              Status code describing result of the operation. */
static status_t fs_create_prepare(const char *path, fs_dentry_t **_entry) {
    status_t ret;

    /* Split path into directory/name. */
    char *dir  = kdirname(path, MM_KERNEL);
    char *name = kbasename(path, MM_KERNEL);

    /* It is possible for kbasename() to return a string with a '/' character
     * if the path refers to the root of the FS. */
    if (strchr(name, '/')) {
        ret = STATUS_ALREADY_EXISTS;
        goto out_free_name;
    }

    dprintf("fs: create '%s': dirname = '%s', basename = '%s'\n", path, dir, name);

    /* Check for disallowed names. */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        ret = STATUS_ALREADY_EXISTS;
        goto out_free_name;
    }

    /* Look up the parent entry. */
    fs_dentry_t *parent;
    ret = fs_lookup(dir, FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK, &parent);
    if (ret != STATUS_SUCCESS)
        goto out_free_name;

    if (parent->node->file.type != FILE_TYPE_DIR) {
        ret = STATUS_NOT_DIR;
        goto out_release_parent;
    }

    /* Check if the name we're creating already exists. */
    fs_dentry_t *entry;
    ret = fs_dentry_lookup(parent, name, &entry);
    if (ret != STATUS_NOT_FOUND) {
        if (ret == STATUS_SUCCESS)
            ret = STATUS_ALREADY_EXISTS;

        goto out_release_parent;
    }

    /* Check that we are on a writable filesystem and that we have write
     * permission to the directory. */
    if (fs_node_is_read_only(parent->node)) {
        ret = STATUS_READ_ONLY;
        goto out_release_parent;
    } else if (!file_access(&parent->node->file, FILE_ACCESS_WRITE)) {
        ret = STATUS_ACCESS_DENIED;
        goto out_release_parent;
    }

    *_entry = fs_dentry_alloc(name, parent->mount, parent);
    ret = STATUS_SUCCESS;
    goto out_free_name;

out_release_parent:
    fs_dentry_release_locked(parent);

out_free_name:
    kfree(dir);
    kfree(name);
    return ret;
}

/** Publish a newly created entry.
 * @param entry         Directory entry. Parent entry will be unlocked and
 *                      released by this function. Entry itself will _not_ be
 *                      released.
 * @param node          Node to attach to the entry. Reference count will not
 *                      be changed. */
static void fs_create_finish(fs_dentry_t *entry, fs_node_t *node) {
    fs_dentry_t *parent = entry->parent;

    /* Instantiate the directory entry and attach to the parent. */
    refcount_set(&entry->count, 1);
    entry->node = node;
    radix_tree_insert(&parent->entries, entry->name, entry);

    fs_dentry_release_locked(parent);
}

/** Common creation code.
 * @param path          Path to node to create.
 * @param type          Type to give the new node.
 * @param target        For symbolic links, the target of the link.
 * @param _entry        Where to store pointer to created entry (can be NULL).
 * @return              Status code describing result of the operation. */
static status_t fs_create(
    const char *path, file_type_t type, const char *target,
    fs_dentry_t **_entry)
{
    status_t ret;

    fs_dentry_t*entry;
    ret = fs_create_prepare(path, &entry);
    if (ret != STATUS_SUCCESS)
        return ret;

    fs_dentry_t* parent = entry->parent;
    if (!parent->node->ops->create) {
        ret = STATUS_NOT_SUPPORTED;
        goto err_free_entry;
    }

    fs_node_t *node = fs_node_alloc(parent->mount);

    node->file.type = type;

    ret = parent->node->ops->create(parent->node, entry, node, target);
    if (ret != STATUS_SUCCESS)
        goto err_free_node;

    dprintf(
        "fs: created '%s': node %" PRIu64 " (%p) in %" PRIu64 " (%p) on %" PRIu16 " (%p)\n",
        path, node->id, node, parent->node->id, parent->node, parent->mount->id,
        parent->mount);

    /* Attach the node to the mount. */
    mutex_lock(&parent->mount->lock);
    avl_tree_insert(&parent->mount->nodes, node->id, &node->tree_link);
    mutex_unlock(&parent->mount->lock);

    fs_create_finish(entry, node);

    if (_entry) {
        *_entry = entry;
    } else {
        fs_dentry_release(entry);
    }

    return STATUS_SUCCESS;

err_free_node:
    slab_cache_free(fs_node_cache, node);

err_free_entry:
    fs_dentry_free(entry);
    fs_dentry_release_locked(parent);
    return ret;
}

/**
 * File operations.
 */

/** Open a FS handle. */
static status_t fs_file_open(file_handle_t *handle) {
    status_t ret = STATUS_SUCCESS;

    if (handle->access & FILE_ACCESS_WRITE && fs_node_is_read_only(handle->node))
        return STATUS_READ_ONLY;

    if (handle->node->ops && handle->node->ops->open)
        ret = handle->node->ops->open(handle);

    if (ret == STATUS_SUCCESS)
        fs_dentry_retain(handle->entry);

    return ret;
}

/** Close a FS handle. */
static void fs_file_close(file_handle_t *handle) {
    if (handle->node->ops->close)
        handle->node->ops->close(handle);

    /* Just release the directory entry, we don't have an extra reference on
     * the node as the entry has one for us. */
    fs_dentry_release(handle->entry);
}

/** Get the name of a FS object. */
static char *fs_file_name(file_handle_t *handle) {
    char *path;
    status_t ret = fs_dentry_path(handle->entry, &path);
    return (ret == STATUS_SUCCESS) ? path : NULL;
}

/** Signal that a file event is being waited for. */
static status_t fs_file_wait(file_handle_t *handle, object_event_t *event) {
    /* TODO. */
    return STATUS_NOT_IMPLEMENTED;
}

/** Stop waiting for a file. */
static void fs_file_unwait(file_handle_t *handle, object_event_t *event) {
    /* TODO. */
}

/** Perform I/O on a file. */
static status_t fs_file_io(file_handle_t *handle, io_request_t *request) {
    return (handle->node->ops->io)
        ? handle->node->ops->io(handle, request)
        : STATUS_NOT_SUPPORTED;
}

/** Map a file into memory. */
static status_t fs_file_map(file_handle_t *handle, vm_region_t *region) {
    fs_node_t *node = (fs_node_t *)handle->file;

    if (!node->ops->get_cache)
        return STATUS_NOT_SUPPORTED;

    region->private = node->ops->get_cache(handle);
    region->ops     = &vm_cache_region_ops;

    return STATUS_SUCCESS;
}

/** Read the next directory entry. */
static status_t fs_file_read_dir(file_handle_t *handle, dir_entry_t **_entry) {
    if (!handle->node->ops->read_dir)
        return STATUS_NOT_SUPPORTED;

    dir_entry_t *entry;
    status_t ret = handle->node->ops->read_dir(handle, &entry);
    if (ret != STATUS_SUCCESS)
        return ret;

    mutex_lock(&handle->entry->lock);

    fs_mount_t *mount = handle->entry->mount;

    /* Fix up the entry. */
    entry->mount = mount->id;
    if (handle->entry == mount->root && strcmp(entry->name, "..") == 0) {
        /* This is the '..' entry, and the directory is the root of its mount.
         * Change the node and mount IDs to be those of the mountpoint, if any. */
        if (mount->mountpoint) {
            entry->id = mount->mountpoint->id;
            entry->mount = mount->mountpoint->mount->id;
        }
    } else {
        /* Check if the entry refers to a mountpoint. In this case we need to
         * change the IDs to those of the mount root, rather than the mountpoint.
         * If we don't have an entry in the cache with the same name as this
         * entry, then it won't be a mountpoint (mountpoints are always in the
         * cache). */
        fs_dentry_t *child = radix_tree_lookup(&handle->entry->entries, entry->name);
        if (child && child->mounted) {
            entry->id = child->mounted->root->id;
            entry->mount = child->mounted->id;
        }
    }

    mutex_unlock(&handle->entry->lock);

    *_entry = entry;
    return STATUS_SUCCESS;
}

/** Modify the size of a file. */
static status_t fs_file_resize(file_handle_t *handle, offset_t size) {
    return (handle->node->ops->resize)
        ? handle->node->ops->resize(handle->node, size)
        : STATUS_NOT_SUPPORTED;
}

/** Get information about a file. */
static void fs_file_info(file_handle_t *handle, file_info_t *info) {
    fs_node_info(handle->node, info);
}

/** Flush changes to a file. */
static status_t fs_file_sync(file_handle_t *handle) {
    status_t ret = STATUS_SUCCESS;

    if (!fs_node_is_read_only(handle->node) && handle->node->ops->flush)
        ret = handle->node->ops->flush(handle->node);

    return ret;
}

/** FS file object operations. */
static file_ops_t fs_file_ops = {
    .open     = fs_file_open,
    .close    = fs_file_close,
    .name     = fs_file_name,
    .wait     = fs_file_wait,
    .unwait   = fs_file_unwait,
    .io       = fs_file_io,
    .map      = fs_file_map,
    .read_dir = fs_file_read_dir,
    .resize   = fs_file_resize,
    .info     = fs_file_info,
    .sync     = fs_file_sync,
};

/**
 * Public kernel interface.
 */

/**
 * Opens a handle to an entry in the filesystem, optionally creating it if it
 * doesn't exist. If the entry does not exist and it is specified to create it,
 * it will be created as a regular file.
 *
 * @param path          Path to open.
 * @param access        Requested access rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param create        Whether to create the file. If FS_OPEN, the file will
 *                      not be created if it doesn't exist. If FS_CREATE, it
 *                      will be created if it doesn't exist. If FS_MUST_CREATE,
 *                      it must be created, and an error will be returned if it
 *                      already exists.
 * @param _handle       Where to store pointer to handle structure.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_open(
    const char *path, uint32_t access, uint32_t flags, unsigned create,
    object_handle_t **_handle)
{
    status_t ret;

    assert(path);
    assert(_handle);

    if (create != FS_OPEN && create != FS_CREATE && create != FS_MUST_CREATE)
        return STATUS_INVALID_ARG;

    /* Look up the filesystem entry. */
    fs_node_t *node;
    fs_dentry_t *entry;
    ret = fs_lookup(path, FS_LOOKUP_FOLLOW, &entry);
    if (ret != STATUS_SUCCESS) {
        if (ret != STATUS_NOT_FOUND || create == FS_OPEN)
            return ret;

        /* Caller wants to create the node. */
        ret = fs_create(path, FILE_TYPE_REGULAR, NULL, &entry);
        if (ret != STATUS_SUCCESS)
            return ret;

        node = entry->node;
    } else if (create == FS_MUST_CREATE) {
        fs_dentry_release(entry);
        return STATUS_ALREADY_EXISTS;
    } else {
        node = entry->node;

        /* FIXME: We should handle other types here too as well. Devices will
         * eventually be redirected to the device layer, pipes should be
         * openable and get directed into the pipe implementation. */
        switch (node->file.type) {
            case FILE_TYPE_REGULAR:
            case FILE_TYPE_DIR:
                break;
            default:
                fs_dentry_release(entry);
                return STATUS_NOT_SUPPORTED;
        }

        /* Check for the requested access to the file. We don't do this when we
         * have first created the file: we allow the requested access regardless
         * of the ACL upon first creation. TODO: The read-only FS check should
         * be moved to an access() hook when ACLs are implemented. */
        if (access && !file_access(&node->file, access)) {
            fs_dentry_release(entry);
            return STATUS_ACCESS_DENIED;
        } else if (access & FILE_ACCESS_WRITE && fs_node_is_read_only(node)) {
            fs_dentry_release(entry);
            return STATUS_READ_ONLY;
        }
    }

    file_handle_t *handle = file_handle_alloc(&node->file, access, flags);

    handle->entry = entry;

    /* Call the FS' open hook, if any. */
    if (node->ops->open) {
        ret = node->ops->open(handle);
        if (ret != STATUS_SUCCESS) {
            file_handle_free(handle);
            fs_dentry_release(entry);
            return ret;
        }
    }

    *_handle = file_handle_create(handle);
    return STATUS_SUCCESS;
}

/**
 * Creates a new directory in the file system. This function cannot open a
 * handle to the created directory. The reason for this is that it is unlikely
 * that anything useful can be done on the new handle, for example reading
 * entries from a new directory will only give '.' and '..' entries.
 *
 * @param path          Path to directory to create.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_create_dir(const char *path) {
    return fs_create(path, FILE_TYPE_DIR, NULL, NULL);
}

/**
 * Creates a new FIFO in the filesystem. A FIFO is a named pipe. Opening it
 * with FILE_ACCESS_READ will give access to the read end, and FILE_ACCESS_WRITE
 * gives access to the write end.
 *
 * @param path          Path to FIFO to create.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_create_fifo(const char *path) {
    return fs_create(path, FILE_TYPE_FIFO, NULL, NULL);
}

/**
 * Creates a new symbolic link in the filesystem. The link target can be on any
 * mount (not just the same one as the link itself), and does not have to exist.
 * If it is a relative path, it is relative to the directory containing the
 * link.
 *
 * @param path          Path to symbolic link to create.
 * @param target        Target for the symbolic link.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_create_symlink(const char *path, const char *target) {
    return fs_create(path, FILE_TYPE_SYMLINK, target, NULL);
}

/**
 * Reads the target of a symbolic link and returns it as a pointer to a string
 * allocated with kmalloc(). Should be freed with kfree() when no longer
 * needed.
 *
 * @param path          Path to the symbolic link to read.
 * @param _target       Where to store pointer to link target string.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_read_symlink(const char *path, char **_target) {
    status_t ret;

    assert(path);
    assert(_target);

    /* Find the link node. */
    fs_dentry_t *entry;
    ret = fs_lookup(path, 0, &entry);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (entry->node->file.type != FILE_TYPE_SYMLINK) {
        fs_dentry_release(entry);
        return STATUS_NOT_SYMLINK;
    } else if (!entry->node->ops->read_symlink) {
        fs_dentry_release(entry);
        return STATUS_NOT_SUPPORTED;
    }

    ret = entry->node->ops->read_symlink(entry->node, _target);
    fs_dentry_release(entry);
    return ret;
}

static void parse_mount_opts(const char *str, fs_mount_option_t **_opts, size_t *_count) {
    fs_mount_option_t *opts = NULL;
    size_t count = 0;

    if (str) {
        /* Duplicate the string to allow modification with strsep(). */
        char *dup = kstrdup(str, MM_KERNEL);
        char *value;
        while ((value = strsep(&dup, ","))) {
            char *name = strsep(&value, "=");
            if (strlen(name) == 0) {
                continue;
            } else if (value && strlen(value) == 0) {
                value = NULL;
            }

            opts = krealloc(opts, sizeof(*opts) * (count + 1), MM_KERNEL);

            opts[count].name  = kstrdup(name, MM_KERNEL);
            opts[count].value = (value) ? kstrdup(value, MM_KERNEL) : NULL;

            count++;
        }

        kfree(dup);
    }

    *_opts  = opts;
    *_count = count;
}

static void free_mount_opts(fs_mount_option_t *opts, size_t count) {
    for (size_t i = 0; i < count; i++) {
        kfree((char *)opts[i].name);
        if (opts[i].value)
            kfree((char *)opts[i].value);
    }

    kfree(opts);
}

/**
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 * The flags argument specifies generic mount options, the opts string is
 * passed into the filesystem driver to specify options specific to the
 * filesystem type.
 *
 * @param device        Device tree path to device filesystem resides on (can
 *                      be NULL if the filesystem does not require a device).
 * @param path          Path to directory to mount on.
 * @param type          Name of filesystem type (if not specified, device will
 *                      be probed to determine the correct type - in this case,
 *                      a device must be specified).
 * @param opts          Options string.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_mount(
    const char *device, const char *path, const char *type, uint32_t flags,
    const char *opts)
{
    status_t ret;

    assert(path);
    assert(device || type);

    if (!security_check_priv(PRIV_FS_MOUNT))
        return STATUS_PERM_DENIED;

    /* Parse the options string. */
    fs_mount_option_t *opt_array;
    size_t opt_count;
    parse_mount_opts(opts, &opt_array, &opt_count);

    /* Lock the mount lock across the entire operation, so that only one mount
     * can take place at a time. */
    mutex_lock(&fs_mount_lock);

    /* If the root filesystem is not yet mounted, the only place we can mount
     * is '/'. */
    fs_dentry_t *mountpoint;
    if (!root_mount) {
        assert(curr_proc == kernel_proc);
        if (strcmp(path, "/") != 0)
            fatal("Root filesystem is not yet mounted");

        mountpoint = NULL;
    } else {
        /* Look up the destination mountpoint. */
        ret = fs_lookup(path, 0, &mountpoint);
        if (ret != STATUS_SUCCESS)
            goto err_unlock;

        /* Check that it is not being used as a mount point already. */
        if (mountpoint->mount->root == mountpoint) {
            ret = STATUS_IN_USE;
            goto err_release_mp;
        }
    }

    fs_mount_t *mount = kmalloc(sizeof(*mount), MM_KERNEL | MM_ZERO);

    mutex_init(&mount->lock, "fs_mount_lock", 0);
    avl_tree_init(&mount->nodes);
    list_init(&mount->used_entries);
    list_init(&mount->unused_entries);
    list_init(&mount->header);

    mount->flags      = flags;
    mount->mountpoint = mountpoint;

    /* If a type is specified, look it up. */
    if (type) {
        mount->type = fs_type_lookup(type);
        if (!mount->type) {
            ret = STATUS_NOT_FOUND;
            goto err_free_mount;
        }
    }

    /* Look up the device if the type needs one or we need to probe. */
    if (!type || mount->type->probe) {
        if (!device) {
            ret = STATUS_INVALID_ARG;
            goto err_free_mount;
        }

        fatal("TODO: Devices");
    }

    /* Allocate a mount ID. */
    if (next_mount_id == UINT16_MAX) {
        ret = STATUS_FS_FULL;
        goto err_free_mount;
    }

    mount->id = next_mount_id++;

    /* Create root directory entry. It will be filled in by the FS' mount
     * operation. */
    mount->root = fs_dentry_alloc("", mount, NULL);

    /* Call the filesystem's mount operation. */
    ret = mount->type->mount(mount, opt_array, opt_count);
    if (ret != STATUS_SUCCESS)
        goto err_free_root;

    assert(mount->ops);

    /* Get the root node. */
    ret = fs_dentry_instantiate(mount->root);
    if (ret != STATUS_SUCCESS)
        goto err_unmount;

    /* Instantiating leaves the entry locked. */
    mutex_unlock(&mount->root->lock);

    /* Make the mountpoint point to the new mount. */
    if (mount->mountpoint)
        mount->mountpoint->mounted = mount;

    refcount_inc(&mount->type->count);
    list_append(&fs_mount_list, &mount->header);
    if (!root_mount) {
        root_mount = mount;

        /* Give the kernel process a correct current/root directory. */
        fs_dentry_retain(root_mount->root);
        curr_proc->io.root_dir = root_mount->root;
        fs_dentry_retain(root_mount->root);
        curr_proc->io.curr_dir = root_mount->root;
    }

    dprintf(
        "fs: mounted %s%s%s on %s (mount: %p, root: %p)\n",
        mount->type->name, (device) ? ":" : "", (device) ? device : "", path,
        mount, mount->root);

    mutex_unlock(&fs_mount_lock);
    free_mount_opts(opt_array, opt_count);
    return STATUS_SUCCESS;

err_unmount:
    if (mount->ops->unmount)
        mount->ops->unmount(mount);

err_free_root:
    fs_dentry_free(mount->root);

err_free_mount:
    kfree(mount);

err_release_mp:
    if (mountpoint)
        fs_dentry_release(mountpoint);

err_unlock:
    mutex_unlock(&fs_mount_lock);
    free_mount_opts(opt_array, opt_count);
    return ret;
}

/**
 * Flushes all modifications to a filesystem (if it is not read-only) and
 * unmounts it. If any entries in the filesystem are in use, then the operation
 * will fail.
 *
 * @param path          Path to mount point of filesystem.
 * @param flags         Behaviour flags.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_unmount(const char *path, unsigned flags) {
    status_t ret;

    if (!security_check_priv(PRIV_FS_MOUNT))
        return STATUS_PERM_DENIED;

    mutex_lock(&fs_mount_lock);

    fs_dentry_t *root;
    ret = fs_lookup(path, 0, &root);
    if (ret != STATUS_SUCCESS)
        goto err_unlock;

    fs_mount_t *mount = root->mount;

    if (root->node->file.type != FILE_TYPE_DIR) {
        ret = STATUS_NOT_DIR;
        goto err_release_root;
    } else if (root != mount->root) {
        ret = STATUS_NOT_MOUNT;
        goto err_release_root;
    } else if (!mount->mountpoint) {
        /* Can't unmount the root filesystem. */
        ret = STATUS_IN_USE;
        goto err_release_root;
    }

    /* Lock the entry containing the mountpoint. Once we have determined that
     * no entries on the mount are in use, this will ensure that no lookups
     * will descend into the mount. */
    fs_dentry_t *parent = mount->mountpoint->parent;
    mutex_lock(&parent->lock);
    mutex_lock(&mount->lock);

    /* Check that we are the only user of the root, and whether any entries
     * other than the root are in use. Drop the reference we just got to the
     * root, and check that the count is now 1 for the reference added
     * by fs_mount(). */
    if (refcount_dec(&root->count) != 1) {
        assert(refcount_get(&root->count));
        ret = STATUS_IN_USE;
        goto err_unlock_mount;
    } else if (!list_is_singular(&mount->used_entries)) {
        ret = STATUS_IN_USE;
        goto err_unlock_mount;
    }

    /* Free all unused directory entries. */
    list_foreach_safe(&mount->unused_entries, iter) {
        fs_dentry_t *entry = list_entry(iter, fs_dentry_t, mount_link);

        assert(refcount_get(&entry->count) == 0);
        assert(!entry->node);

        if (!(entry->flags & FS_DENTRY_KEEP)) {
            spinlock_lock(&unused_entries_lock);
            assert(!list_empty(&entry->unused_link));
            unused_entry_count--;
            list_remove(&entry->unused_link);
            spinlock_unlock(&unused_entries_lock);
        }

        list_remove(&entry->mount_link);
        fs_dentry_free(entry);
    }

    /* Free all nodes other than the root node. We have to free the root node
     * and directory entry last as we still want to leave the mount in the
     * correct state if we fail to flush some nodes. */
    avl_tree_foreach_safe(&mount->nodes, iter) {
        fs_node_t *node = avl_tree_entry(iter, fs_node_t, tree_link);

        if (node == root->node)
            continue;

        assert(refcount_get(&node->count) == 0);

        /* Forcibly free the node ignoring I/O errors if requested. */
        if (flags & FS_UNMOUNT_FORCE)
            node->flags |= FS_NODE_REMOVED;

        ret = fs_node_free(node);
        if (ret != STATUS_SUCCESS)
            goto err_unlock_mount;
    }

    /* Free the root node itself. Drop reference to satisfy assertion in
     * fs_node_free(). */
    refcount_dec(&root->node->count);
    if (flags & FS_UNMOUNT_FORCE)
        root->node->flags |= FS_NODE_REMOVED;
    ret = fs_node_free(root->node);
    if (ret != STATUS_SUCCESS) {
        refcount_inc(&root->node->count);
        goto err_unlock_mount;
    }

    list_remove(&root->mount_link);
    fs_dentry_free(root);

    /* Detach from the mountpoint. */
    mount->mountpoint->mounted = NULL;
    mutex_unlock(&parent->lock);
    fs_dentry_release(mount->mountpoint);

    if (mount->ops->unmount)
        mount->ops->unmount(mount);

    if (mount->device)
        object_handle_release(mount->device);

    refcount_dec(&mount->type->count);

    list_remove(&mount->header);
    mutex_unlock(&mount->lock);
    kfree(mount);
    mutex_unlock(&fs_mount_lock);
    return STATUS_SUCCESS;

err_unlock_mount:
    mutex_unlock(&mount->lock);
    mutex_unlock(&parent->lock);
    goto err_unlock;

err_release_root:
    fs_dentry_release(root);

err_unlock:
    mutex_unlock(&fs_mount_lock);
    return ret;
}

/**
 * Given a handle to a file or directory, returns the absolute path that was
 * used to open the handle. If the handle specified is NULL, the path to the
 * current directory will be returned.
 *
 * @param handle        Handle to get path from.
 * @param _path         Where to store pointer to path string.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_path(object_handle_t *handle, char **_path) {
    fs_dentry_t *entry;
    if (handle) {
        file_handle_t *fhandle = handle->private;

        if (fhandle->file->ops != &fs_file_ops)
            return STATUS_NOT_SUPPORTED;

        entry = fhandle->entry;
    } else {
        rwlock_read_lock(&curr_proc->io.lock);
        entry = curr_proc->io.curr_dir;
    }

    status_t ret = fs_dentry_path(entry, _path);
    if (!handle)
        rwlock_unlock(&curr_proc->io.lock);

    return ret;
}

/** Gets information about a filesystem entry.
 * @param path          Path to get information on.
 * @param follow        Whether to follow if last path component is a symbolic
 *                      link.
 * @param info          Information structure to fill in.
 * @return              Status code describing result of the operation. */
status_t fs_info(const char *path, bool follow, file_info_t *info) {
    assert(path);
    assert(info);

    fs_dentry_t *entry;
    status_t ret = fs_lookup(path, (follow) ? FS_LOOKUP_FOLLOW : 0, &entry);
    if (ret != STATUS_SUCCESS)
        return ret;

    fs_node_info(entry->node, info);
    fs_dentry_release(entry);
    return STATUS_SUCCESS;
}

/**
 * Creates a new hard link in the filesystem referring to the same underlying
 * node as the source link. Both paths must exist on the same mount. If the
 * source path refers to a symbolic link, the new link will refer to the node
 * pointed to by the symbolic link, not the symbolic link itself.
 *
 * @param path          Path to new link.
 * @param source        Path to source node for the link.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_link(const char *path, const char *source) {
    status_t ret;

    fs_dentry_t *entry;
    ret = fs_lookup(source, FS_LOOKUP_FOLLOW, &entry);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* We just need the node, we don't care about the source dentry. */
    fs_node_t *node = entry->node;
    refcount_inc(&node->count);
    fs_dentry_release(entry);

    /* Can't hard link to directories. */
    if (node->file.type == FILE_TYPE_DIR) {
        ret = STATUS_IS_DIR;
        goto err_release_node;
    }

    ret = fs_create_prepare(path, &entry);
    if (ret != STATUS_SUCCESS)
        goto err_release_node;

    fs_dentry_t *parent = entry->parent;
    if (parent->mount != node->mount) {
        ret = STATUS_DIFFERENT_FS;
        goto err_free_entry;
    } else if (!parent->node->ops->link) {
        ret = STATUS_NOT_SUPPORTED;
        goto err_free_entry;
    }

    entry->id = node->id;

    ret = parent->node->ops->link(parent->node, entry, node);
    if (ret != STATUS_SUCCESS)
        goto err_free_entry;

    dprintf(
        "fs: linked '%s': node %" PRIu64 " (%p) in %" PRIu64 " (%p) on %" PRIu16 " (%p)\n", 
        path, node->id, node, parent->node->id, parent->node, parent->mount->id,
        parent->mount);

    /* The node reference is taken over by fs_create_finish(). */
    fs_create_finish(entry, node);
    fs_dentry_release(entry);
    return STATUS_SUCCESS;

err_free_entry:
    fs_dentry_free(entry);
    fs_dentry_release_locked(parent);

err_release_node:
    fs_node_release(node);
    return ret;
}

/**
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path          Path to node to decrease link count of.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_unlink(const char *path) {
    status_t ret;

    /* Split path into directory/name. */
    char *dir  = kdirname(path, MM_KERNEL);
    char *name = kbasename(path, MM_KERNEL);

    /* It is possible for kbasename() to return a string with a '/' character
     * if the path refers to the root of the FS. */
    if (strchr(name, '/')) {
        ret = STATUS_IN_USE;
        goto out_free_name;
    }

    dprintf("fs: unlink '%s': dirname = '%s', basename = '%s'\n", path, dir, name);

    if (strcmp(name, ".") == 0) {
        /* Trying to unlink '.' is invalid, it means "remove the '.' entry from
         * the directory", rather than "remove the entry referring to the
         * directory in the parent". */
        ret = STATUS_INVALID_ARG;
        goto out_free_name;
    } else if (strcmp(name, "..") == 0) {
        ret = STATUS_NOT_EMPTY;
        goto out_free_name;
    }

    /* Look up the parent entry. */
    fs_dentry_t *parent;
    ret = fs_lookup(dir, FS_LOOKUP_FOLLOW | FS_LOOKUP_LOCK, &parent);
    if (ret != STATUS_SUCCESS)
        goto out_free_name;

    if (parent->node->file.type != FILE_TYPE_DIR) {
        ret = STATUS_NOT_DIR;
        goto out_release_parent;
    }

    /* Look up the child entry. */
    fs_dentry_t *entry;
    ret = fs_dentry_lookup(parent, name, &entry);
    if (ret != STATUS_SUCCESS)
        goto out_release_parent;
    ret = fs_dentry_instantiate(entry);
    if (ret != STATUS_SUCCESS)
        goto out_release_parent;

    /* Check whether we can unlink the entry. */
    if (entry->mounted) {
        ret = STATUS_IN_USE;
        goto out_release_entry;
    } else if (fs_node_is_read_only(parent->node)) {
        ret = STATUS_READ_ONLY;
        goto out_release_entry;
    } else if (!file_access(&parent->node->file, FILE_ACCESS_WRITE)) {
        ret = STATUS_ACCESS_DENIED;
        goto out_release_entry;
    } else if (!parent->node->ops->unlink) {
        ret = STATUS_NOT_SUPPORTED;
        goto out_release_entry;
    }

    /* If the node being unlinked is a directory, check whether we have anything
     * anything in the cache for it. While this is not a sufficient emptiness
     * check (there may be entries we haven't got cached), it avoids a call out
     * to the FS if we know that it is not empty already. Also, ramfs relies on
     * this check being here, as it exists entirely in the cache. */
    if (!radix_tree_empty(&entry->entries)) {
        ret = STATUS_NOT_EMPTY;
        goto out_release_entry;
    }

    ret = parent->node->ops->unlink(parent->node, entry, entry->node);
    if (ret != STATUS_SUCCESS)
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
 * Renames a link on the filesystem. This first creates a new link referring to
 * the same underlying filesystem node as the source link, and then removes
 * the source link. Both paths must exist on the same mount. If the specified
 * destination path exists, it is first removed.
 *
 * @param source        Path to original link.
 * @param dest          Path for new link.
 *
 * @return              Status code describing result of the operation.
 */
status_t fs_rename(const char *source, const char *dest) {
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Flushes all cached filesystem modifications that have yet to be written to
 * the disk.
 */
status_t fs_sync(void) {
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Debugger commands.
 */

/** Print information about mounted filesystems. */
static kdb_status_t kdb_cmd_mount(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<mount ID|addr>]\n\n", argv[0]);

        kdb_printf("Given a mount ID or an address of a mount structure, prints out details of that\n");
        kdb_printf("mount, or given no arguments, prints out a list of all mounted filesystems.\n");
        return KDB_SUCCESS;
    } else if (argc != 1 && argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    if (argc == 2) {
    uint64_t val;
        if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        fs_mount_t *mount;
        if (val >= KERNEL_BASE) {
            mount = (fs_mount_t *)((ptr_t)val);
        } else {
            mount = fs_mount_lookup(val);
            if (!mount) {
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
        kdb_printf(
            "lock:       %d (%" PRId32 ")\n",
            atomic_load(&mount->lock.value),
            (mount->lock.holder) ? mount->lock.holder->id : -1);
        kdb_printf("flags:      0x%x\n", mount->flags);
        kdb_printf("ops:        %ps\n", mount->ops);
        kdb_printf("private:    %p\n", mount->private);
        kdb_printf("device:     %p\n", mount->device);
        kdb_printf("root:       %p\n", mount->root);
        kdb_printf(
            "mountpoint: %p ('%s')\n",
            mount->mountpoint, (mount->mountpoint) ? mount->mountpoint->name : "<root>");
    } else {
        kdb_printf("ID  Type       Flags    Device             Mountpoint\n");
        kdb_printf("==  ====       =====    ======             ==========\n");

        list_foreach(&fs_mount_list, iter) {
            fs_mount_t *mount = list_entry(iter, fs_mount_t, header);

            kdb_printf(
                "%-3" PRIu16 " %-10s 0x%-6x %-18p %p ('%s')\n",
                mount->id, (mount->type) ? mount->type->name : "none",
                mount->flags, mount->device, mount->mountpoint,
                (mount->mountpoint) ? mount->mountpoint->name : "<root>");
        }
    }

    return KDB_SUCCESS;
}

/** Display the children of a directory entry. */
static void dump_children(fs_dentry_t *entry, bool descend) {
    kdb_printf("Entry              Count  Flags    Mount Node     Name\n");
    kdb_printf("=====              =====  =====    ===== ====     ====\n");

    /* We're in the debugger and descending through a potentially very large
     * tree. Don't use recursion, we really don't want to overrun the stack. */
    fs_dentry_t *child = NULL;
    fs_dentry_t *prev = NULL;
    unsigned depth = 0;
    while (true) {
        radix_tree_foreach(&entry->entries, iter) {
            child = radix_tree_entry(iter, fs_dentry_t);

            if (prev) {
                if (child == prev)
                    prev = NULL;

                child = NULL;
                continue;
            }

            kdb_printf(
                "%-18p %-6d 0x%-6x %-5" PRId16 " %-8" PRIu64 " %*s%s\n",
                child, refcount_get(&child->count), child->flags,
                (child->mount) ? child->mount->id : -1, child->id, depth * 2,
                "", child->name);

            if (!descend) {
                child = NULL;
                continue;
            }

            if (child->parent != entry) {
                kdb_printf("-- Incorrect parent %p\n", child->parent);
                child = NULL;
                continue;
            } else if (child->mounted) {
                if (child->mounted->mountpoint != child) {
                    kdb_printf("-- Incorrect mountpoint %p\n", child->mounted->mountpoint);
                    child = NULL;
                    continue;
                }

                child = child->mounted->root;
            }

            break;
        }

        if (child) {
            /* Go to child. */
            depth++;
            entry = child;
            prev = child = NULL;
        } else {
            /* Go back to parent. */
            if (depth == 0)
                return;

            if (entry == entry->mount->root) {
                prev = entry->mount->mountpoint;
            } else {
                prev = entry;
            }

            entry = prev->parent;
            depth--;
        }
    }
}

/** Print information about the directory cache. */
static kdb_status_t kdb_cmd_dentry(int argc, char **argv, kdb_filter_t *filter) {

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [--descend] [<addr>]\n\n", argv[0]);

        kdb_printf("Given the address of a directory entry structure, prints out details of that\n");
        kdb_printf("entry. If the `--descend' argument is given, the entire directory cache tree\n");
        kdb_printf("below the given entry will be dumped rather than just its immediate children.\n");
        kdb_printf("Given no address, the starting point will be the root.\n");
        return KDB_SUCCESS;
    } else if (argc > 3) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    int idx;
    bool descend = false;
    if (argc > 1 && argv[1][0] == '-') {
        if (strcmp(argv[1], "--descend") == 0) {
            descend = true;
        } else {
            kdb_printf("Unrecognized option. See 'help %s' for help.\n", argv[0]);
            return KDB_FAILURE;
        }

        idx = 2;
    } else {
        idx = 1;
    }

    fs_dentry_t *entry;
    if (idx < argc) {
        uint64_t val;
        if (kdb_parse_expression(argv[idx], &val, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        entry = (fs_dentry_t *)((ptr_t)val);
    } else {
        entry = root_mount->root;
    }

    kdb_printf("Entry %p ('%s')\n", entry, entry->name);
    kdb_printf("=================================================\n");
    kdb_printf(
        "lock:    %d (%" PRId32 ")\n",
        atomic_load(&entry->lock.value), (entry->lock.holder) ? entry->lock.holder->id : -1);
    kdb_printf("count:   %d\n", refcount_get(&entry->count));
    kdb_printf("flags:   0x%x\n", entry->flags);
    kdb_printf("mount:   %p%c", entry->mount, (entry->mount) ? ' ' : '\n');
    if (entry->mount)
        kdb_printf("(%" PRIu16 ")\n", entry->mount->id);
    kdb_printf("id:      %" PRIu64 "\n", entry->id);
    kdb_printf("node:    %p%c", entry->node, (entry->node) ? ' ' : '\n');
    if (entry->node)
        kdb_printf("(%" PRIu64 ")\n", entry->node->id);
    kdb_printf("parent:  %p%c", entry->parent, (entry->parent) ? ' ' : '\n');
    if (entry->parent)
        kdb_printf("('%s')\n", entry->parent->name);
    kdb_printf("mounted: %p%c", entry->mounted, (entry->mounted) ? ' ' : '\n');
    if (entry->mounted)
        kdb_printf("(%" PRIu16 ")\n", entry->mounted->id);

    if (!radix_tree_empty(&entry->entries)) {
        kdb_printf("\n");
        dump_children(entry, descend);
    }

    return KDB_SUCCESS;
}

/** Convert a file type to a string.
 * @param type          File type.
 * @return              String representation of file type. */
static inline const char *file_type_name(file_type_t type) {
    switch (type) {
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

/** Print information about a node. */
static kdb_status_t kdb_cmd_node(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s <mount ID>\n", argv[0]);
        kdb_printf("       %s <mount ID> <node ID>\n", argv[0]);
        kdb_printf("       %s <addr>\n\n", argv[0]);

        kdb_printf("The first form of this command prints a list of all nodes currently in memory\n");
        kdb_printf("for the specified mount. The second two forms prints details of a single node\n");
        kdb_printf("currently in memory, specified by either a mount ID and node ID pair, or the\n");
        kdb_printf("address of a node structure\n");
        return KDB_SUCCESS;
    } else if (argc != 2 && argc != 3) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    uint64_t val;
    if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    fs_node_t *node = NULL;
    fs_mount_t *mount;
    if (val >= KERNEL_BASE) {
        node = (fs_node_t *)((ptr_t)val);
    } else {
        mount = fs_mount_lookup(val);
        if (!mount) {
            kdb_printf("Unknown mount ID %" PRIu64 ".\n", val);
            return KDB_FAILURE;
        }

        if (argc == 3) {
            if (kdb_parse_expression(argv[2], &val, NULL) != KDB_SUCCESS)
                return KDB_FAILURE;

            node = avl_tree_lookup(&mount->nodes, val, fs_node_t, tree_link);
            if (!node) {
                kdb_printf("Unknown node ID %" PRIu64 ".\n", val);
                return KDB_FAILURE;
            }
        }
    }

    if (node) {
        /* Print out basic node information. */
        kdb_printf(
            "Node %p (%" PRIu16 ":%" PRIu64 ")\n",
            node, node->mount->id, node->id);
        kdb_printf("=================================================\n");
        kdb_printf("count:   %d\n", refcount_get(&node->count));
        kdb_printf("type:    %d (%s)\n", node->file.type, file_type_name(node->file.type));
        kdb_printf("flags:   0x%x\n", node->flags);
        kdb_printf("ops:     %ps\n", node->ops);
        kdb_printf("private: %p\n", node->private);
        kdb_printf("mount:   %p%c", node->mount, (node->mount) ? ' ' : '\n');
        if (node->mount)
            kdb_printf("(%" PRIu16 ")\n", node->mount->id);
    } else {
        kdb_printf("ID       Count Flags    Type              Private\n");
        kdb_printf("==       ===== =====    ====              =======\n");

        avl_tree_foreach(&mount->nodes, iter) {
            node = avl_tree_entry(iter, fs_node_t, tree_link);

            kdb_printf(
                "%-8" PRIu64 " %-5d 0x%-6x %-17s %p\n",
                node->id, refcount_get(&node->count), node->flags,
                file_type_name(node->file.type), node->private);
        }
    }

    return KDB_SUCCESS;
}

/** Initialize the filesystem layer. */
__init_text void fs_init(void) {
    fs_node_cache = object_cache_create(
        "fs_node_cache", fs_node_t, NULL, NULL, NULL, 0, MM_BOOT);
    fs_dentry_cache = object_cache_create(
        "fs_dentry_cache", fs_dentry_t, fs_dentry_ctor, NULL, NULL, 0, MM_BOOT);

    /* Register the KDB commands. */
    kdb_register_command(
        "mount",
        "Display information about mounted filesystems.",
        kdb_cmd_mount);
    kdb_register_command(
        "dentry",
        "Display information about the directory cache.",
        kdb_cmd_dentry);
    kdb_register_command(
        "node",
        "Display information about a filesystem node.",
        kdb_cmd_node);
}

/** Shut down the filesystem layer. */
void fs_shutdown(void) {
    /* TODO */
}

/**
 * System calls.
 */

/**
 * Opens a handle to an entry in the filesystem, optionally creating it if it
 * doesn't exist. If the entry does not exist and it is specified to create it,
 * it will be created as a regular file.
 *
 * @param path          Path to open.
 * @param access        Requested access rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param create        Whether to create the file. If FS_OPEN, the file will
 *                      not be created if it doesn't exist. If FS_CREATE, it
 *                      will be created if it doesn't exist. If FS_MUST_CREATE,
 *                      it must be created, and an error will be returned if it
 *                      already exists.
 * @param _handle       Where to store created handle.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_open(
    const char *path, uint32_t access, uint32_t flags, unsigned create,
    handle_t *_handle)
{
    status_t ret;

    if (!path || !_handle)
        return STATUS_INVALID_ARG;

    char *kpath = NULL;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *handle;
    ret = fs_open(kpath, access, flags, create, &handle);
    if (ret != STATUS_SUCCESS) {
        kfree(kpath);
        return ret;
    }

    ret = object_handle_attach(handle, NULL, _handle);
    object_handle_release(handle);
    kfree(kpath);
    return ret;
}

/**
 * Creates a new directory in the file system. This function cannot open a
 * handle to the created directory. The reason for this is that it is unlikely
 * that anything useful can be done on the new handle, for example reading
 * entries from a new directory will only give '.' and '..' entries.
 *
 * @param path          Path to directory to create.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_create_dir(const char *path) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = fs_create_dir(kpath);
    kfree(kpath);
    return ret;
}

/**
 * Creates a new FIFO in the filesystem. A FIFO is a named pipe. Opening it
 * with FILE_ACCESS_READ will give access to the read end, and FILE_ACCESS_WRITE
 * gives access to the write end.
 *
 * @param path          Path to FIFO to create.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_create_fifo(const char *path) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = fs_create_fifo(kpath);
    kfree(kpath);
    return ret;
}

/**
 * Create a new symbolic link in the filesystem. The link target can be on any
 * mount (not just the same one as the link itself), and does not have to exist.
 * If it is a relative path, it is relative to the directory containing the
 * link.
 *
 * @param path          Path to symbolic link to create.
 * @param target        Target for the symbolic link.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_create_symlink(const char *path, const char *target) {
    status_t ret;

    if (!path || !target)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    char *ktarget;
    ret = strndup_from_user(target, FS_PATH_MAX, &ktarget);
    if (ret != STATUS_SUCCESS) {
        kfree(kpath);
        return ret;
    }

    ret = fs_create_symlink(kpath, ktarget);
    kfree(ktarget);
    kfree(kpath);
    return ret;
}

/**
 * Reads the destination of a symbolic link into a buffer. A NULL byte will
 * always be placed at the end of the string.
 *
 * @param path          Path to the symbolic link to read.
 * @param buf           Buffer to read into.
 * @param size          Size of buffer. If this is too small, the function will
 *                      return STATUS_TOO_SMALL.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_read_symlink(const char *path, char *buf, size_t size) {
    status_t ret;

    if (!path || !buf)
        return STATUS_INVALID_ARG;

    if (!size)
        return STATUS_TOO_SMALL;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    char *kbuf;
    ret = fs_read_symlink(kpath, &kbuf);
    if (ret == STATUS_SUCCESS) {
        size_t len = strlen(kbuf) + 1; 
        if (len > size) {
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
 * Mounts a filesystem onto an existing directory in the filesystem hierarchy.
 * Mounting multiple filesystems on one directory at a time is not allowed.
 * The flags argument specifies generic mount options, the opts string is
 * passed into the filesystem driver to specify options specific to the
 * filesystem type.
 *
 * @param device        Device tree path to device filesystem resides on (can
 *                      be NULL if the filesystem does not require a device).
 * @param path          Path to directory to mount on.
 * @param type          Name of filesystem type (if not specified, device will
 *                      be probed to determine the correct type - in this case,
 *                      a device must be specified).
 * @param opts          Options string.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_mount(
    const char *device, const char *path, const char *type, uint32_t flags,
    const char *opts)
{
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kdevice = NULL, *kpath = NULL, *ktype = NULL, *kopts = NULL;

    if (device) {
        ret = strndup_from_user(device, FS_PATH_MAX, &kdevice);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        goto out;

    if (type) {
        ret = strndup_from_user(type, FS_PATH_MAX, &ktype);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    if (opts) {
        ret = strndup_from_user(opts, FS_PATH_MAX, &kopts);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = fs_mount(kdevice, kpath, ktype, flags, kopts);

out:
    kfree(kdevice);
    kfree(kpath);
    kfree(ktype);
    kfree(kopts);

    return ret;
}

/** Gets information on mounted filesystems.
 * @param infos         Array of mount information structures to fill in. If
 *                      NULL, the function will only return the number of
 *                      mounted filesystems.
 * @param _count        If infos is not NULL, this should point to a value
 *                      containing the size of the provided array. Upon
 *                      successful completion, the value will be updated to
 *                      be the number of structures filled in. If infos is NULL,
 *                      the number of mounted filesystems will be stored here.
 * @return              Status code describing result of the operation. */
status_t kern_fs_mount_info(mount_info_t *infos, size_t *_count) {
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Flushes all modifications to a filesystem (if it is not read-only) and
 * unmounts it. If any entries in the filesystem are in use, then the operation
 * will fail.
 *
 * @param path          Path to mount point of filesystem.
 * @param flags         Behaviour flags.
 *
 * @return              Status code describing result of the operation.
 */

status_t kern_fs_unmount(const char *path, unsigned flags) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = fs_unmount(kpath, flags);
    kfree(kpath);
    return ret;
}

/**
 * Given a handle to a file or directory, returns the absolute path that was
 * used to open the handle. If the handle specified is INVALID_HANDLE, the path
 * to the current directory will be returned.
 *
 * @param handle        Handle to get path from.
 * @param buf           Buffer to write path string to.
 * @param size          Size of buffer. If this is too small, STATUS_TOO_SMALL
 *                      will be returned.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_path(handle_t handle, char *buf, size_t size) {
    status_t ret;

    object_handle_t *khandle = NULL;
    if (handle >= 0) {
        ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    char *path;
    ret = fs_path(khandle, &path);
    if (khandle)
        object_handle_release(khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    size_t len = strlen(path);
    if (len < size) {
        ret = memcpy_to_user(buf, path, len + 1);
    } else {
        ret = STATUS_TOO_SMALL;
    }

    kfree(path);
    return ret;
}

/** Sets the current working directory.
 * @param path          Path to change to.
 * @return              Status code describing result of the operation. */
status_t kern_fs_set_curr_dir(const char *path) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    fs_dentry_t *entry;
    ret = fs_lookup(kpath, FS_LOOKUP_FOLLOW, &entry);
    if (ret != STATUS_SUCCESS) {
        goto out_free;
    } else if (entry->node->file.type != FILE_TYPE_DIR) {
        ret = STATUS_NOT_DIR;
        goto out_release;
    }

    /* Must have execute permission to use as working directory. */
    if (!file_access(&entry->node->file, FILE_ACCESS_EXECUTE)) {
        ret = STATUS_ACCESS_DENIED;
        goto out_release;
    }

    rwlock_write_lock(&curr_proc->io.lock);
    swap(entry, curr_proc->io.curr_dir);
    rwlock_unlock(&curr_proc->io.lock);

out_release:
    fs_dentry_release(entry);

out_free:
    kfree(kpath);
    return ret;
}

/**
 * Sets both the current directory and the root directory for the calling
 * process to the directory specified. Any processes spawned by the process
 * after this call will also have the same root directory. Note that this
 * function is not entirely the same as chroot() on a UNIX system: it enforces
 * the new root by changing the current directory to it, and then does not let
 * the process ascend out of it using '..' in a path. On UNIX systems, however,
 * the root user is allowed to ascend out via '..'.
 *
 * @param path          Path to directory to change to.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_set_root_dir(const char *path) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    if (!security_check_priv(PRIV_FS_SETROOT))
        return STATUS_PERM_DENIED;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    fs_dentry_t *entry;
    ret = fs_lookup(kpath, FS_LOOKUP_FOLLOW, &entry);
    if (ret != STATUS_SUCCESS) {
        goto out_free;
    } else if (entry->node->file.type != FILE_TYPE_DIR) {
        ret = STATUS_NOT_DIR;
        goto out_release;
    }

    /* Must have execute permission to use as working directory. */
    if (!file_access(&entry->node->file, FILE_ACCESS_EXECUTE)) {
        ret = STATUS_ACCESS_DENIED;
        goto out_release;
    }

    /* We set both the root and current directories to this entry, so we need
     * to add another reference. */
    fs_dentry_t *curr = entry;
    fs_dentry_retain(curr);

    rwlock_write_lock(&curr_proc->io.lock);
    swap(entry, curr_proc->io.root_dir);
    swap(curr, curr_proc->io.curr_dir);
    rwlock_unlock(&curr_proc->io.lock);

    fs_dentry_release(curr);

out_release:
    fs_dentry_release(entry);

out_free:
    kfree(kpath);
    return ret;
}

/** Gets information about a node.
 * @param path          Path to get information on.
 * @param follow        Whether to follow if last path component is a symbolic
 *                      link.
 * @param info          Information structure to fill in.
 * @return              Status code describing result of the operation. */
status_t kern_fs_info(const char *path, bool follow, file_info_t *info) {
    status_t ret;

    if (!path || !info)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    file_info_t kinfo;
    ret = fs_info(kpath, follow, &kinfo);
    if (ret == STATUS_SUCCESS)
        ret = memcpy_to_user(info, &kinfo, sizeof(*info));

    kfree(kpath);
    return ret;
}

/**
 * Creates a new hard link in the filesystem referring to the same underlying
 * node as the source link. Both paths must exist on the same mount. If the
 * source path refers to a symbolic link, the new link will refer to the node
 * pointed to by the symbolic link, not the symbolic link itself.
 *
 * @param path          Path to new link.
 * @param source        Path to source node for the link.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_link(const char *path, const char *source) {
    status_t ret;

    if (!path || !source)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    char *ksource;
    ret = strndup_from_user(source, FS_PATH_MAX, &ksource);
    if (ret != STATUS_SUCCESS) {
        kfree(kpath);
        return ret;
    }

    ret = fs_link(kpath, ksource);
    kfree(ksource);
    kfree(kpath);
    return ret;
}

/**
 * Decreases the link count of a filesystem node, and removes the directory
 * entry for it. If the link count becomes 0, then the node will be removed
 * from the filesystem once the node's reference count becomes 0. If the given
 * node is a directory, then the directory should be empty.
 *
 * @param path          Path to node to decrease link count of.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_unlink(const char *path) {
    status_t ret;

    if (!path)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = fs_unlink(kpath);
    kfree(kpath);
    return ret;
}

/**
 * Renames a link on the filesystem. This first creates a new link referring to
 * the same underlying filesystem node as the source link, and then removes
 * the source link. Both paths must exist on the same mount. If the specified
 * destination path exists, it is first removed.
 *
 * @param source        Path to original link.
 * @param dest          Path for new link.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_fs_rename(const char *source, const char *dest) {
    status_t ret;

    if (!source || !dest)
        return STATUS_INVALID_ARG;

    char *ksource;
    ret = strndup_from_user(source, FS_PATH_MAX, &ksource);
    if (ret != STATUS_SUCCESS)
        return ret;

    char *kdest;
    ret = strndup_from_user(dest, FS_PATH_MAX, &kdest);
    if (ret != STATUS_SUCCESS) {
        kfree(ksource);
        return ret;
    }

    ret = fs_rename(ksource, kdest);
    kfree(kdest);
    kfree(ksource);
    return ret;
}

/**
 * Flushes all cached filesystem modifications that have yet to be written to
 * the disk.
 */
status_t kern_fs_sync(void) {
    return fs_sync();
}
