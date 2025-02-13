/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File object interface.
 */

#pragma once

#include <kernel/file.h>

#include <sync/mutex.h>

#include <object.h>

struct device;
struct file_handle;
struct fs_node;
struct io_request;
struct pipe;
struct socket;
struct user_file;

/** Operations for a file. */
typedef struct file_ops {
    /** Opens a file (via file_reopen).
     * @param handle        File handle structure.
     * @return              Status code describing the result of the operation. */
    status_t (*open)(struct file_handle *handle);

    /** Closes a file.
     * @param handle        File handle structure. All data allocated for the
     *                      handle should be freed. */
    void (*close)(struct file_handle *handle);

    /** Gets the name of a file.
     * @param handle        File handle structure.
     * @return              Pointer to allocated name string. */
    char *(*name)(struct file_handle *handle);

    /** Get the name of a file in KDB context.
     * @see                 object_type_t::name().
     * @param handle        File handle structure.
     * @param buf           Buffer to write into.
     * @param size          Size of the buffer.
     * @return              Pointer to start of name string, or NULL if not
     *                      available. */
    char *(*name_unsafe)(struct file_handle *handle, char *buf, size_t size);

    /**
     * Signals that a file event is being waited for. If the event being waited
     * for has occurred already, this function should call the callback function
     * and return success.
     *
     * For FILE_EVENT_{READABLE,WRITABLE}, access flag checks are performed
     * before calling this function. For any other file-specific events, this
     * function should perform any appropriate access checks.
     *
     * @param handle        File handle structure.
     * @param event         Event that is being waited for.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*wait)(struct file_handle *handle, object_event_t *event);

    /** Stops waiting for a file event.
     * @param handle        File handle structure.
     * @param event         Event that is being waited for. */
    void (*unwait)(struct file_handle *handle, object_event_t *event);

    /** Performs I/O on a file.
     * @param handle        File handle structure.
     * @param request       I/O request.
     * @return              Status code describing result of the operation. */
    status_t (*io)(struct file_handle *handle, struct io_request *request);

    /** Maps a file into memory.
     * @see                 object_type_t::map().
     * @param handle        File handle structure.
     * @param region        Region being mapped.
     * @return              Status code describing result of the operation. */
    status_t (*map)(struct file_handle *handle, struct vm_region *region);

    /**
     * Reads the next directory entry. The implementation can make use of the
     * offset field in the handle to store whatever it needs to implement this
     * function. It will be set to 0 when the handle is initially opened, and
     * when rewind_dir() is called on the handle.
     *
     * @param handle        File handle structure.
     * @param _entry        Where to store pointer to directory entry structure
     *                      (must be allocated using a kmalloc()-based function).
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*read_dir)(struct file_handle *handle, dir_entry_t **_entry);

    /** Modifies the size of a file.
     * @param handle        File handle structure.
     * @param size          New size of the file.
     * @return              Status code describing result of the operation. */
    status_t (*resize)(struct file_handle *handle, offset_t size);

    /** Gets information about a file.
     * @param handle        File handle structure.
     * @param info          Information structure to fill in (pre-zeroed). */
    void (*info)(struct file_handle *handle, file_info_t *info);

    /** Flushes changes to a file.
     * @param handle        File handle structure.
     * @return              Status code describing result of the operation. */
    status_t (*sync)(struct file_handle *handle);

    /** Handler for file-specific requests.
     * @param handle        File handle structure.
     * @param request       Request number.
     * @param in            Input buffer.
     * @param in_size       Input buffer size.
     * @param _out          Where to store pointer to kmalloc()'d output buffer.
     * @param _out_size     Where to store output buffer size.
     * @return              Status code describing result of operation. */
    status_t (*request)(
        struct file_handle *handle, uint32_t request, const void *in,
        size_t in_size, void **_out, size_t *_out_size);
} file_ops_t;

/** Header for a file object. */
typedef struct file {
    const file_ops_t *ops;              /**< File operations structure. */
    file_type_t type;                   /**< Type of the file. */
} file_t;

/** File handle information. */
typedef struct file_handle {
    union {
        file_t *file;                   /**< File object. */
        struct fs_node *node;           /**< Filesystem node. */
        struct device *device;          /**< Device node. */
        struct pipe *pipe;              /**< Pipe. */
        struct socket *socket;          /**< Socket. */
        struct user_file *user_file;    /**< User file. */
    };

    uint32_t access;                    /**< Access rights the handle was opened with. */
    atomic_uint32_t _flags;             /**< FILE_* flags (access with file_handle_flags()). */
    void *private;                      /**< Implementation data pointer. */
    mutex_t lock;                       /**< Lock to protect offset. */
    offset_t offset;                    /**< Current file offset. */
    struct fs_dentry *entry;            /**< Directory entry used to open the node. */
} file_handle_t;

/**
 * Implementation functions.
 */

extern bool file_access(file_t *file, uint32_t access);

extern file_handle_t *file_handle_alloc(file_t *file, uint32_t access, uint32_t flags);
extern void file_handle_free(file_handle_t *fhandle);
extern object_handle_t *file_handle_create(file_handle_t *fhandle);
extern status_t file_handle_open(file_t *file, uint32_t access, uint32_t flags, handle_t *_id, handle_t *_uid);

/**
 * Gets the current flags for a file handle. This uses atomic access, and the
 * flags can change between subsequent calls to this due to calls to
 * kern_file_set_flags() - nothing prevents the flags from being changed by
 * another thread while an operation on a handle is in progress. For this
 * reason, operations that need to check flags should only read the flags once
 * and save the relevant bits to use throughout the operation, as reading flags
 * multiple times could lead to inconsistent operation.
 */
static inline uint32_t file_handle_flags(file_handle_t *fhandle) {
    return atomic_load_explicit(&fhandle->_flags, memory_order_relaxed);
}

/**
 * Public kernel interface.
 */

extern status_t file_reopen(object_handle_t *handle, uint32_t access, uint32_t flags, object_handle_t **_new);

extern status_t file_read(
    object_handle_t *handle, void *buf, size_t size, offset_t offset,
    size_t *_bytes);
extern status_t file_write(
    object_handle_t *handle, const void *buf, size_t size, offset_t offset,
    size_t *_bytes);

extern status_t file_read_vecs(
    object_handle_t *handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes);
extern status_t file_write_vecs(
    object_handle_t *handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes);

extern status_t file_read_dir(object_handle_t *handle, dir_entry_t *buf, size_t size);
extern status_t file_rewind_dir(object_handle_t *handle);

extern status_t file_state(
    object_handle_t *handle, uint32_t *_access, uint32_t *_flags,
    offset_t *_offset);
extern status_t file_set_flags(object_handle_t *handle, uint32_t flags);
extern status_t file_seek(
    object_handle_t *handle, uint32_t action, offset_t offset,
    offset_t *_result);

extern status_t file_resize(object_handle_t *handle, offset_t size);
extern status_t file_info(object_handle_t *handle, file_info_t *info);
extern status_t file_sync(object_handle_t *handle);

extern status_t file_request(
    object_handle_t *handle, uint32_t request, const void *in, size_t in_size,
    void **_out, size_t *_out_size);
