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
 * @brief               Filesystem interface.
 */

#pragma once

#include <io/file.h>

#include <kernel/fs.h>

#include <lib/avl_tree.h>
#include <lib/radix_tree.h>
#include <lib/refcount.h>

#include <sync/mutex.h>

#include <object.h>

struct fs_mount;
struct fs_node;
struct fs_dentry;
struct vm_cache;

/** Structure containing a filesystem mount option. */
typedef struct fs_mount_option {
    const char *name;               /**< Argument name. */
    const char *value;              /**< Argument value (can be NULL). */
} fs_mount_option_t;

/** Filesystem type description structure. */
typedef struct fs_type {
    list_t header;                  /**< Link to types list. */

    const char *name;               /**< Short name of the filesystem type. */
    const char *description;        /**< Long name of the type. */
    refcount_t count;               /**< Number of mounts using this type. */

    /**
     * Check whether a device contains this FS type.
     *
     * Checks whether a device contains this FS type. If this method is not
     * provided, then it is assumed that the FS does not use a backing device,
     * and one will not be passed to the mount() method.
     *
     * @param device        Handle to device to check.
     * @param uuid          If not NULL, the function should only return true
     *                      if the filesystem also has this UUID.
     *
     * @return              Whether the device contains this FS type.
     */
    bool (*probe)(object_handle_t *device, const char *uuid);

    /**
     * Mount an instance of this FS type.
     *
     * Mount an instance of this FS type. It is guaranteed that the device will
     * contain the correct FS type when this is called, as the probe() operation
     * is called prior to this. This function should fill in the root directory
     * entry structure pointed to by the mount structure with the ID of the root
     * node and any flags it wishes to set on the entry.
     *
     * @param mount         Mount structure for the mount.
     * @param opts          Array of options passed to the mount call.
     * @param count         Number of options in the array.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*mount)(struct fs_mount *mount, fs_mount_option_t *opts, size_t count);
} fs_type_t;

extern status_t fs_type_register(fs_type_t *type);
extern status_t fs_type_unregister(fs_type_t *type);

/** Mount operations structure. */
typedef struct fs_mount_ops {
    /**
     * Unmount a filesystem.
     *
     * Unmounts an instance of this filesystem type. All nodes will be been
     * freed. This function should free up any data allocated in the mount()
     * method. The flush() operation is not called before this function as
     * unmounting will likely need to change filesystem metadata and flush
     * again, so calling flush() beforehand would be wasteful.
     *
     * @param mount         Filesystem being unmounted.
     */
    void (*unmount)(struct fs_mount *mount);

    /** Flush changes to filesystem metadata.
     * @param mount         Mount to flush.
     * @return              Status code describing result of the operation. */
    status_t (*flush)(struct fs_mount *mount);

    /** Read a node from the filesystem.
     * @param mount         Mount to obtain node from.
     * @param node          Node structure to fill in with node details.
     *                      Contains ID of the node to read.
     * @return              Status code describing result of the operation. */
    status_t (*read_node)(struct fs_mount *mount, struct fs_node *node);
} fs_mount_ops_t;

/** Structure containing details of a mounted filesystem. */
typedef struct fs_mount {
    mutex_t lock;                   /**< Lock to protect structure. */

    unsigned flags;                 /**< Flags for the mount. */
    fs_mount_ops_t *ops;            /**< Mount operations. */
    void *private;                  /**< Filesystem type private data. */
    object_handle_t *device;        /**< Handle to device that the filesystem resides on. */

    avl_tree_t nodes;               /**< Tree mapping node IDs to node structures. */
    struct fs_dentry *root;         /**< Root directory entry. */
    struct fs_dentry *mountpoint;   /**< Directory that this mount is mounted on. */
    list_t used_entries;            /**< List of all used entries. */
    list_t unused_entries;          /**< List of all unused entries. */

    mount_id_t id;                  /**< Mount ID. */
    fs_type_t *type;                /**< Filesystem type. */
    list_t header;                  /**< Link to mounts list. */
} fs_mount_t;

extern fs_mount_t *root_mount;

/** Node operations structure. */
typedef struct fs_node_ops {
    /**
     * Free a node.
     *
     * This function is called when the node is being freed and should free any
     * data allocated for it by the filesystem type. If the node's link count
     * has reached 0, this function should remove it from the filesystem. Note
     * that you should not check the FS_NODE_REMOVED flag to see if the node
     * should be removed, instead check the filesystem's own link count, as this
     * flag can be used internally by the FS layer when the node has not
     * actually been removed from the FS.
     *
     * @param node          Node to free.
     */
    void (*free)(struct fs_node *node);

    /** Flush changes to node metadata.
     * @param node          Node to flush.
     * @return              Status code describing result of the operation. */
    status_t (*flush)(struct fs_node *node);

    /**
     * Create a new node.
     *
     * Creates a new node as a child of an existing directory. The supplied
     * entry structure contains the name of the entry to create, and the
     * supplied node structure contains the attributes for the new node (type,
     * security attributes, etc). This function should fill in the remainder of
     * both of these structures as though lookup() and read_node() had been
     * called on each of them.
     *
     * @param parent        Directory to create in.
     * @param entry         Entry structure for the new entry.
     * @param node          Node structure.
     * @param target        For symbolic links, the target of the link.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*create)(
        struct fs_node *parent, struct fs_dentry *entry, struct fs_node *node,
        const char *target);

    /**
     * Create a hard link.
     *
     * Creates a hard link in a directory to an existing node. This function
     * should fill in any flags it wishes on the directory entry, as with
     * create(). Note that it may be possible that an unlink takes place at the
     * same time as a link on the same node, so when incrementing the node link
     * count this function should check whether it was 0 and clear the
     * FS_NODE_REMOVED flag if it was.
     *
     * @param parent        Directory to create link in.
     * @param entry         Entry structure for the new entry.
     * @param node          Existing node to link to.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*link)(struct fs_node *parent, struct fs_dentry *entry, struct fs_node *node);

    /**
     * Remove an entry from a directory.
     *
     * Removes an entry from a directory. If the link count of the node that the
     * entry refers to reaches 0, the filesystem should set the FS_NODE_REMOVE
     * flag on the node, but not remove it from the filesystem as it may still
     * be in use. This will cause the node to be freed as soon as it has no
     * users, and it should then be removed by the free() method.
     *
     * If the node being removed is a directory, this function should ensure
     * that it is empty aside from . and .. entries, and return STATUS_NOT_EMPTY
     * if it isn't.
     *
     * @param parent        Directory containing the entry to remove.
     * @param entry         Entry in the directory being removed.
     * @param node          Node that the entry refers to.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*unlink)(struct fs_node *parent, struct fs_dentry *entry, struct fs_node *node);

    /** Get information about a node.
     * @param node          Node to get information on.
     * @param info          Information structure to fill in. */
    void (*info)(struct fs_node *node, file_info_t *info);

    /** Modify the size of a file.
     * @param node          Node being resized.
     * @param size          New size of the node.
     * @return              Status code describing result of the operation. */
    status_t (*resize)(struct fs_node *node, offset_t size);

    /** Look up a directory entry.
     * @param node          Node to look up in.
     * @param entry         Entry structure containing entry name to look up,
     *                      node ID should be set by this function.
     * @return              Status code describing result of the operation. */
    status_t (*lookup)(struct fs_node *node, struct fs_dentry *entry);

    /** Read the target of a symbolic link.
     * @param node          Node to read from.
     * @param _target       Where to store pointer to string (must be
     *                      allocated using a kmalloc()-based function)
     *                      containing link target.
     * @return              Status code describing result of the operation. */
    status_t (*read_symlink)(struct fs_node *node, char **_target);

    /**
     * File handle operations.
     */

    /** Open a handle to a node.
     * @param handle        New handle structure.
     * @return              Status code describing result of the operation. */
    status_t (*open)(file_handle_t *handle);

    /** Close a handle to a node.
     * @param handle        File handle structure. */
    void (*close)(file_handle_t *handle);

    /** Perform I/O on a file.
     * @param handle        File handle structure.
     * @param request       I/O request.
     * @return              Status code describing result of the operation. */
    status_t (*io)(file_handle_t *handle, struct io_request *request);

    /** Get the data cache for a file.
     * @param handle        Handle that the cache is needed for.
     * @return              Pointer to node's VM cache. If this function is
     *                      provided, it is assumed that this function will
     *                      always succeed, otherwise it is assumed that the
     *                      file cannot be memory-mapped. */
    struct vm_cache *(*get_cache)(file_handle_t *handle);

    /** Read the next directory entry.
     * @note                It is up to the filesystem implementation to store
     *                      the current offset into the directory. It can make
     *                      use of the offset field in the handle to do so. This
     *                      field is set to 0 when the handle is opened and when
     *                      rewind_dir() is called on it, otherwise it is not
     *                      modified.
     * @param handle        File handle structure.
     * @param _entry        Where to store pointer to directory entry
     *                      structure (must be allocated using a
     *                      kmalloc()-based function).
     * @return              Status code describing result of the operation. */
    status_t (*read_dir)(file_handle_t *handle, dir_entry_t **_entry);
} fs_node_ops_t;

/** Structure containing details of a filesystem node. */
typedef struct fs_node {
    file_t file;                    /**< File object header. */

    refcount_t count;               /**< Number of references to the node. */
    node_id_t id;                   /**< ID of the node. */
    atomic_uint32_t flags;          /**< Flags for the node. */

    fs_node_ops_t *ops;             /**< Node operations. */
    void *private;                  /**< Filesystem type private data. */
    fs_mount_t *mount;              /**< Mount that the node resides on. */

    avl_tree_node_t tree_link;      /**< Link to node tree. */
    list_t unused_link;             /**< Link to global unused node list. */
} fs_node_t;

/** Flags for a filesystem node. */
#define FS_NODE_KEEP        (1<<0)  /**< Do not remove the node. */
#define FS_NODE_REMOVED     (1<<1)  /**< Node should be freed as soon as no longer needed. */

/** Atomically set a flag on a filesystem node.
 * @param node          Node to set flag on.
 * @param flag          Flag to set. */
static inline void fs_node_set_flag(fs_node_t *node, uint32_t flag) {
    atomic_fetch_or(&node->flags, flag);
}

/** Atomically clear a flag on a filesystem node.
 * @param node          Node to clear flag on.
 * @param flag          Flag to clear. */
static inline void fs_node_clear_flag(fs_node_t *node, uint32_t flag) {
    atomic_fetch_and(&node->flags, ~flag);
}

/** Check if a node is read only.
 * @param node          Node to check.
 * @return              Whether the node is read only. */
static inline bool fs_node_is_read_only(fs_node_t *node) {
    return (node->mount && node->mount->flags & FS_MOUNT_READ_ONLY);
}

/** Structure containing a directory entry. */
typedef struct fs_dentry {
    mutex_t lock;                   /**< Lock to protect the entry. */

    /**
     * Reference count.
     *
     * The directory entry reference count holds the actual number of users of
     * the entry. It does not count references by child entries. An entry is
     * placed on the unused list and made freeable once it has no references
     * and no children.
     */
    refcount_t count;

    unsigned flags;                 /**< Flags for the entry. */
    char *name;                     /**< Name of the entry. */
    node_id_t id;                   /**< Node ID that the entry refers to. */
    fs_mount_t *mount;              /**< Mount that the entry resides on. */

    /**
     * Node that the entry refers to.
     *
     * When an entry is in use (its reference count is non-zero), its node
     * pointer is valid. The node pointer is invalid when the reference count
     * is 0, i.e. no open handles refer to entry and it is not in use by any
     * in-progress lookup.
     */
    fs_node_t *node;

    struct fs_dentry *parent;       /**< Parent entry. */
    radix_tree_t entries;           /**< Tree of name to entry mappings. */
    fs_mount_t *mounted;            /**< Filesystem mounted on this entry. */
    list_t mount_link;              /**< Link to mount unused entry list. */
    list_t unused_link;             /**< Link to global unused entry list. */
} fs_dentry_t;

/** Flags for a directory entry. */
#define FS_DENTRY_KEEP      (1<<0)  /**< Do not remove the entry. */

extern void fs_dentry_retain(fs_dentry_t *entry);
extern void fs_dentry_release(fs_dentry_t *entry);

/**
 * Kernel interface.
 */

extern status_t fs_open(
    const char *path, uint32_t rights, uint32_t flags, unsigned create,
    object_handle_t **_handle);

extern status_t fs_create_dir(const char *path);
extern status_t fs_create_fifo(const char *path);
extern status_t fs_create_symlink(const char *path, const char *target);

extern status_t fs_read_symlink(const char *path, char **_target);

extern status_t fs_mount(
    const char *device, const char *path, const char *type, uint32_t flags,
    const char *opts);
extern status_t fs_unmount(const char *path, unsigned flags);

extern status_t fs_path(object_handle_t *handle, char **_path);
extern status_t fs_info(const char *path, bool follow, file_info_t *info);
extern status_t fs_link(const char *path, const char *source);
extern status_t fs_unlink(const char *path);
extern status_t fs_rename(const char *source, const char *dest);
extern status_t fs_sync(void);

/**
 * Initialization/shutdown functions.
 */

extern void fs_init(void);
extern void fs_shutdown(void);
