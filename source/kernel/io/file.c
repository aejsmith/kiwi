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
 * @brief               File object interface.
 */

#include <io/file.h>
#include <io/request.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/vm.h>

#include <assert.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

/** Close a handle to a file.
 * @param handle        Handle to the file. */
static void file_object_close(object_handle_t *handle) {
    file_handle_t *fhandle = handle->private;

    if (fhandle->file->ops->close)
        fhandle->file->ops->close(fhandle);

    file_handle_free(fhandle);
}

/** Get the name of a file object.
 * @param handle        Handle to the file.
 * @return              Pointer to allocated name string. */
static char *file_object_name(object_handle_t *handle) {
    file_handle_t *fhandle = handle->private;

    if (!fhandle->file->ops->name)
        return NULL;

    return fhandle->file->ops->name(fhandle);
}

/** Signal that a file event is being waited for.
 * @param handle        Handle to the file.
 * @param event         Event that is being waited for.
 * @return              Status code describing result of the operation. */
static status_t file_object_wait(object_handle_t *handle, object_event_t *event) {
    file_handle_t *fhandle = handle->private;

    if (!fhandle->file->ops->wait)
        return STATUS_NOT_SUPPORTED;

    return fhandle->file->ops->wait(fhandle, event);
}

/** Stop waiting for a file event.
 * @param handle        Handle to the file.
 * @param event         Event that is being waited for. */
static void file_object_unwait(object_handle_t *handle, object_event_t *event) {
    file_handle_t *fhandle = handle->private;

    assert(fhandle->file->ops->unwait);
    return fhandle->file->ops->unwait(fhandle, event);
}

/** Map a file object into memory.
 * @param handle        Handle to object.
 * @param region        Region being mapped.
 * @return              Status code describing result of the operation. */
static status_t file_object_map(object_handle_t *handle, vm_region_t *region) {
    file_handle_t *fhandle = handle->private;
    uint32_t access = 0;

    /* Directories cannot be memory-mapped. */
    if (fhandle->file->type == FILE_TYPE_DIR || !fhandle->file->ops->map)
        return STATUS_NOT_SUPPORTED;

    /* Check for the necessary access rights. Don't need write permission for
     * private mappings, changes won't be written back to the file. */
    if (region->access & VM_ACCESS_READ) {
        access |= FILE_ACCESS_READ;
    } else if (region->access & VM_ACCESS_WRITE && !(region->flags & VM_MAP_PRIVATE)) {
        access |= FILE_ACCESS_WRITE;
    } else if (region->access & VM_ACCESS_EXECUTE) {
        access |= FILE_ACCESS_EXECUTE;
    }

    if ((fhandle->access & access) != access)
        return STATUS_ACCESS_DENIED;

    return fhandle->file->ops->map(fhandle, region);
}

/** File object type definition. */
static object_type_t file_object_type = {
    .id = OBJECT_TYPE_FILE,
    .flags = OBJECT_TRANSFERRABLE,
    .close = file_object_close,
    .name = file_object_name,
    .wait = file_object_wait,
    .unwait = file_object_unwait,
    .map = file_object_map,
};

/**
 * Check for access to a file.
 *
 * Checks the current thread's security context against a file's ACL to
 * determine whether that it has the specified access rights to the file.
 *
 * @param file          File to check.
 * @param access        Access rights to check for.
 *
 * @return              Whether the thread is allowed the access.
 */
bool file_access(file_t *file, uint32_t access) {
    // TODO
    return true;
}

/** Allocate a new file handle structure.
 * @param file          File that handle is to.
 * @param access        Access rights for the handle.
 * @param flags         Flags for the handle.
 * @return              Pointer to allocated structure. */
file_handle_t *file_handle_alloc(file_t *file, uint32_t access, uint32_t flags) {
    file_handle_t *fhandle;

    fhandle = kmalloc(sizeof(*fhandle), MM_KERNEL);
    mutex_init(&fhandle->lock, "file_handle_lock", 0);
    fhandle->file = file;
    fhandle->access = access;
    fhandle->flags = flags;
    fhandle->private = NULL;
    fhandle->offset = 0;
    return fhandle;
}

/** Free a file handle structure.
 * @param fhandle       Handle to free. */
void file_handle_free(file_handle_t *fhandle) {
    kfree(fhandle);
}

/** Create an object handle from a file handle structure.
 * @param fhandle       Filled in file handle structure.
 * @return              Created object handle. */
object_handle_t *file_handle_create(file_handle_t *fhandle) {
    return object_handle_create(&file_object_type, fhandle);
}

/** Determine whether a file is seekable.
 * @param file          File to check.
 * @return              Whether the file is seekable. */
static inline bool is_seekable(file_t *file) {
    return (file->type == FILE_TYPE_REGULAR || file->type == FILE_TYPE_BLOCK);
}

/** Perform an I/O request on a file.
 * @param handle        Handle to file to perform request on.
 * @param request       I/O request to perform.
 * @return              Status code describing result of the operation. */
static status_t file_io(object_handle_t *handle, io_request_t *request) {
    file_handle_t *fhandle;
    uint32_t access;
    bool update_offset = false;
    file_info_t info;
    status_t ret;

    if (handle->type->id != OBJECT_TYPE_FILE) {
        ret = STATUS_INVALID_HANDLE;
        goto out;
    }

    fhandle = handle->private;

    access = (request->op == IO_OP_WRITE) ? FILE_ACCESS_WRITE : FILE_ACCESS_READ;
    if (!(fhandle->access & access)) {
        ret = STATUS_ACCESS_DENIED;
        goto out;
    }

    if (fhandle->file->type == FILE_TYPE_DIR || !fhandle->file->ops->io) {
        ret = STATUS_NOT_SUPPORTED;
        goto out;
    }

    /* Don't do anything more if we have nothing to transfer. */
    if (!request->count) {
        ret = STATUS_SUCCESS;
        goto out;
    }

    /* Determine the offset to perform the I/O at and handle the FILE_APPEND
     * flag. TODO: We don't handle atomicity at all here. For regular files,
     * should we lock the handle across the operation so that nothing else can
     * do I/O while this is in progress? */
    if (is_seekable(fhandle->file)) {
        if (request->offset < 0) {
            if (request->op == IO_OP_WRITE && fhandle->flags & FILE_APPEND) {
                mutex_lock(&fhandle->lock);
                fhandle->file->ops->info(fhandle, &info);
                fhandle->offset = request->offset = info.size;
                mutex_unlock(&fhandle->lock);
            } else {
                request->offset = fhandle->offset;
            }

            update_offset = true;
        }
    } else {
        if (request->offset >= 0) {
            ret = STATUS_NOT_SUPPORTED;
            goto out;
        }
    }

    ret = fhandle->file->ops->io(fhandle, request);

out:
    /* Update the file handle offset. */
    if (request->transferred && update_offset) {
        mutex_lock(&fhandle->lock);
        fhandle->offset += request->transferred;
        mutex_unlock(&fhandle->lock);
    }

    return ret;
}

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. If the specified offset is greater
 * than or equal to 0, then data will be read from exactly that offset in the
 * file, and the handle's offset will not be modified. Otherwise, the read will
 * occur from the file handle's current offset, and before returning the offset
 * will be incremented by the number of bytes read.
 *
 * @param handle        Handle to file to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param buf           Buffer to read data into.
 * @param size          Number of bytes to read. The supplied buffer should be
 *                      at least this size.
 * @param offset        Offset to read from. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes read (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been read.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_read(
    object_handle_t *handle, void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    io_vec_t vec;
    io_request_t request;
    status_t ret;

    assert(handle);
    assert(buf);

    vec.buffer = buf;
    vec.size = size;

    ret = io_request_init(&request, &vec, 1, offset, IO_OP_READ, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_io(handle, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
    return ret;
}

/**
 * Write to a file.
 *
 * Writes data from a buffer to a file. If the specified offset is greater than
 * or equal to 0, then data will be written to exactly that offset in the file,
 * and the handle's offset will not be modified. Otherwise, the write will
 * occur at the file handle's current offset (which will be set to the end of
 * the file if the handle has the FILE_APPEND flag set), and before returning
 * the offset will be incremented by the number of bytes written.
 *
 * @param handle        Handle to file to write to. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param buf           Buffer containing data to write.
 * @param size          Number of bytes to write. The supplied buffer should be
 *                      at least this size.
 * @param offset        Offset to write to. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes written (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been written.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_write(
    object_handle_t *handle, const void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    io_vec_t vec;
    io_request_t request;
    status_t ret;

    assert(handle);
    assert(buf);

    vec.buffer = (void *)buf;
    vec.size = size;

    ret = io_request_init(&request, &vec, 1, offset, IO_OP_WRITE, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_io(handle, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
    return ret;
}

/**
 * Read from a file.
 *
 * Reads data from a file into multiple buffers. If the specified offset is
 * greater than or equal to 0, then data will be read from exactly that offset
 * in the file, and the handle's offset will not be modified. Otherwise, the
 * read will occur from the file handle's current offset, and before returning
 * the offset will be incremented by the number of bytes read.
 *
 * @param handle        Handle to file to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param vecs          I/O vectors describing buffers to read into.
 * @param count         Number of I/O vectors.
 * @param offset        Offset to read from. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes read (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been read.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_read_vecs(
    object_handle_t *handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes)
{
    io_request_t request;
    status_t ret;

    assert(handle);

    ret = io_request_init(&request, vecs, count, offset, IO_OP_READ, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_io(handle, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
    return ret;
}

/**
 * Write to a file.
 *
 * Writes data from multiple buffers to a file. If the specified offset is
 * greater than or equal to 0, then data will be written to exactly that offset
 * in the file, and the handle's offset will not be modified. Otherwise, the
 * write will occur at the file handle's current offset (which will be set to
 * the end of the file if the handle has the FILE_APPEND flag set), and before
 * returning the offset will be incremented by the number of bytes written.
 *
 * @param handle        Handle to file to write to. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param vecs          I/O vectors describing buffers containing data to write.
 * @param count         Number of I/O vectors.
 * @param offset        Offset to write to. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes written (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been written.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_write_vecs(
    object_handle_t *handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes)
{
    io_request_t request;
    status_t ret;

    assert(handle);

    ret = io_request_init(&request, vecs, count, offset, IO_OP_WRITE, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_io(handle, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
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
 * @param handle        Handle to directory to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param buf           Buffer to read entry in to.
 * @param size          Size of buffer (if not large enough, the function will
 *                      return STATUS_TOO_SMALL).
 *
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_NOT_FOUND if the end of the directory has been
 *                      reached.
 *                      STATUS_TOO_SMALL if the buffer is too small for the
 *                      entry.
 */
status_t file_read_dir(object_handle_t *handle, dir_entry_t *buf, size_t size) {
    file_handle_t *fhandle;
    dir_entry_t *entry;
    status_t ret;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (!(fhandle->access & FILE_ACCESS_READ)) {
        return STATUS_ACCESS_DENIED;
    } else if (fhandle->file->type != FILE_TYPE_DIR) {
        return STATUS_NOT_DIR;
    } else if (!fhandle->file->ops->read_dir) {
        return STATUS_NOT_SUPPORTED;
    }

    /* Lock the handle around the call, the implementation is allowed to modify
     * the offset. */
    mutex_lock(&fhandle->lock);
    ret = fhandle->file->ops->read_dir(fhandle, &entry);
    mutex_unlock(&fhandle->lock);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (entry->length > size) {
        kfree(entry);
        return STATUS_TOO_SMALL;
    }

    memcpy(buf, entry, entry->length);
    kfree(entry);
    return STATUS_SUCCESS;
}

/** Rewind to the beginning of a directory.
 * @param handle        Handle to directory to rewind.
 * @return              Status code describing result of the operation. */
status_t file_rewind_dir(object_handle_t *handle) {
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (!(fhandle->access & FILE_ACCESS_READ)) {
        return STATUS_ACCESS_DENIED;
    } else if (fhandle->file->type != FILE_TYPE_DIR) {
        return STATUS_NOT_DIR;
    } else if (!fhandle->file->ops->read_dir) {
        return STATUS_NOT_SUPPORTED;
    }

    mutex_lock(&fhandle->lock);
    fhandle->offset = 0;
    mutex_unlock(&fhandle->lock);
    return STATUS_SUCCESS;
}

/** Get file handle state.
 * @param handle        Handle to get state for.
 * @param _access       Where to store access rights (optional).
 * @param _flags        Where to store handle flags (optional).
 * @param _offset       Where to store current offset (optional).
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is not a file.
 *                      STATUS_NOT_SUPPORTED if attempting to retrieve current
 *                      offset and the file is not seekable. */
status_t file_state(object_handle_t *handle, uint32_t *_access, uint32_t *_flags, offset_t *_offset) {
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (_access)
        *_access = fhandle->access;
    if (_flags)
        *_flags = fhandle->flags;
    if (_offset) {
        if (!is_seekable(fhandle->file))
            return STATUS_NOT_SUPPORTED;

        *_offset = fhandle->offset;
    }

    return STATUS_SUCCESS;
}

/** Set a file handle's flags.
 * @param handle        Handle to set flags for.
 * @param flags         New flags to set.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is not a file. */
status_t file_set_flags(object_handle_t *handle, uint32_t flags) {
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    /* TODO: We'll need an underlying FS call for certain flag changes, e.g.
     * FILE_DIRECT. */
    fhandle = handle->private;
    fhandle->flags = flags;
    return STATUS_SUCCESS;
}

/**
 * Set the offset of a file handle.
 *
 * Modifies the offset of a file handle (the position that will next be read
 * from or written to) according to the specified action, and returns the new
 * offset.
 *
 * @param handle        Handle to modify offset of.
 * @param action        Operation to perform (FILE_SEEK_*).
 * @param offset        Value to perform operation with.
 * @param newp          Where to store new offset value (optional).
 *
 * @return              Status code describing result of the operation.
 */
status_t file_seek(object_handle_t *handle, unsigned action, offset_t offset, offset_t *_result) {
    file_handle_t *fhandle;
    offset_t result;
    file_info_t info;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (!is_seekable(fhandle->file))
        return STATUS_NOT_SUPPORTED;

    mutex_lock(&fhandle->lock);

    switch (action) {
    case FILE_SEEK_SET:
        result = offset;
        break;
    case FILE_SEEK_ADD:
        result = fhandle->offset + offset;
        break;
    case FILE_SEEK_END:
        fhandle->file->ops->info(fhandle, &info);
        result = info.size + offset;
        break;
    default:
        mutex_unlock(&fhandle->lock);
        return STATUS_INVALID_ARG;
    }

    if (result < 0) {
        mutex_unlock(&fhandle->lock);
        return STATUS_INVALID_ARG;
    }

    fhandle->offset = result;
    mutex_unlock(&fhandle->lock);

    if (_result)
        *_result = result;

    return STATUS_SUCCESS;
}

/**
 * Modify the size of a file.
 *
 * Modifies the size of a file. If the new size is smaller than the previous
 * size of the file, then the extra data is discarded. If it is larger than the
 * previous size, then the extended space will be filled with zero bytes.
 *
 * @param handle        Handle to file to resize. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param size          New size of the file.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_resize(object_handle_t *handle, offset_t size) {
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (!(fhandle->access & FILE_ACCESS_WRITE)) {
        return STATUS_ACCESS_DENIED;
    } else if (fhandle->file->type != FILE_TYPE_REGULAR) {
        return STATUS_NOT_REGULAR;
    } else if (!fhandle->file->ops->resize) {
        return STATUS_NOT_SUPPORTED;
    }

    return fhandle->file->ops->resize(fhandle, size);
}

/** Get information about a file or directory.
 * @param handle        Handle to file to get information for.
 * @param info          Information structure to fill in.
 * @return              Status code describing result of the operation. */
status_t file_info(object_handle_t *handle, file_info_t *info) {
    file_handle_t *fhandle;

    assert(handle);
    assert(info);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    fhandle->file->ops->info(fhandle, info);
    return STATUS_SUCCESS;
}

/** Flush changes to a file to the FS.
 * @param handle        Handle to file to flush.
 * @return              Status code describing result of the operation. */
status_t file_sync(object_handle_t *handle) {
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    /* If it's not implemented, assume there is nothing to sync. */
    if (!fhandle->file->ops->sync)
        return STATUS_SUCCESS;

    return fhandle->file->ops->sync(fhandle);
}

/** Perform a file-specific operation.
 * @param handle        Handle to device to perform operation on.
 * @param request       Operation number to perform.
 * @param in            Optional input buffer containing data to pass to the
 *                      operation handler.
 * @param in_size       Size of input buffer.
 * @param _out          Where to store pointer to data returned by the
 *                      operation handler (optional).
 * @param _out_size     Where to store size of data returned.
 * @return              Status code describing result of the operation. */
status_t file_request(
    object_handle_t *handle, unsigned request, const void *in, size_t in_size,
    void **_out, size_t *_out_size)
{
    file_handle_t *fhandle;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    fhandle = handle->private;

    if (!fhandle->file->ops->request)
        return STATUS_INVALID_REQUEST;

    return fhandle->file->ops->request(fhandle, request, in, in_size, _out, _out_size);
}

/**
 * System calls.
 */

/**
 * Read from a file.
 *
 * Reads data from a file into a buffer. If the specified offset is greater
 * than or equal to 0, then data will be read from exactly that offset in the
 * file, and the handle's offset will not be modified. Otherwise, the read will
 * occur from the file handle's current offset, and before returning the offset
 * will be incremented by the number of bytes read.
 *
 * @param handle        Handle to file to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param buf           Buffer to read data into.
 * @param size          Number of bytes to read. The supplied buffer should be
 *                      at least this size.
 * @param offset        Offset to read from. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes read (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been read.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_read(
    handle_t handle, void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    object_handle_t *khandle;
    io_vec_t vec;
    io_request_t request;
    status_t ret, err;

    request.transferred = 0;

    if (!buf) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    vec.buffer = buf;
    vec.size = size;

    ret = io_request_init(&request, &vec, 1, offset, IO_OP_READ, IO_TARGET_USER);
    if (ret != STATUS_SUCCESS) {
        object_handle_release(khandle);
        goto out;
    }

    ret = file_io(khandle, &request);
    io_request_destroy(&request);
    object_handle_release(khandle);

out:
    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    return ret;
}

/**
 * Write to a file.
 *
 * Writes data from a buffer to a file. If the specified offset is greater than
 * or equal to 0, then data will be written to exactly that offset in the file,
 * and the handle's offset will not be modified. Otherwise, the write will
 * occur at the file handle's current offset (which will be set to the end of
 * the file if the handle has the FILE_APPEND flag set), and before returning
 * the offset will be incremented by the number of bytes written.
 *
 * @param handle        Handle to file to write to. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param buf           Buffer containing data to write.
 * @param size          Number of bytes to write. The supplied buffer should be
 *                      at least this size.
 * @param offset        Offset to write to. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes written (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been written.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_write(
    handle_t handle, const void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    object_handle_t *khandle;
    io_vec_t vec;
    io_request_t request;
    status_t ret, err;

    request.transferred = 0;

    if (!buf) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    vec.buffer = (void *)buf;
    vec.size = size;

    ret = io_request_init(&request, &vec, 1, offset, IO_OP_WRITE, IO_TARGET_USER);
    if (ret != STATUS_SUCCESS) {
        object_handle_release(khandle);
        goto out;
    }

    ret = file_io(khandle, &request);
    io_request_destroy(&request);
    object_handle_release(khandle);

out:
    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    return ret;
}

/**
 * Read from a file.
 *
 * Reads data from a file into multiple buffers. If the specified offset is
 * greater than or equal to 0, then data will be read from exactly that offset
 * in the file, and the handle's offset will not be modified. Otherwise, the
 * read will occur from the file handle's current offset, and before returning
 * the offset will be incremented by the number of bytes read.
 *
 * @param handle        Handle to file to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param vecs          I/O vectors describing buffers to read into.
 * @param count         Number of I/O vectors.
 * @param offset        Offset to read from. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes read (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been read.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_read_vecs(
    handle_t handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes)
{
    object_handle_t *khandle;
    io_vec_t *kvecs;
    io_request_t request;
    status_t ret, err;

    request.transferred = 0;

    if (!vecs) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    kvecs = kmalloc(sizeof(*kvecs) * count, MM_USER);
    if (!kvecs) {
        ret = STATUS_NO_MEMORY;
        goto out;
    }

    ret = memcpy_from_user(kvecs, vecs, sizeof(*kvecs) * count);
    if (ret != STATUS_SUCCESS) {
        kfree(kvecs);
        goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS) {
        kfree(kvecs);
        goto out;
    }

    ret = io_request_init(&request, kvecs, count, offset, IO_OP_READ, IO_TARGET_USER);
    kfree(kvecs);
    if (ret != STATUS_SUCCESS) {
        object_handle_release(khandle);
        goto out;
    }

    ret = file_io(khandle, &request);
    io_request_destroy(&request);
    object_handle_release(khandle);

out:
    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    return ret;
}

/**
 * Write to a file.
 *
 * Writes data from multiple buffers to a file. If the specified offset is
 * greater than or equal to 0, then data will be written to exactly that offset
 * in the file, and the handle's offset will not be modified. Otherwise, the
 * write will occur at the file handle's current offset (which will be set to
 * the end of the file if the handle has the FILE_APPEND flag set), and before
 * returning the offset will be incremented by the number of bytes written.
 *
 * @param handle        Handle to file to write to. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param vecs          I/O vectors describing buffers containing data to write.
 * @param count         Number of I/O vectors.
 * @param offset        Offset to write to. If negative, handle's offset will
 *                      be used.
 * @param _bytes        Where to store number of bytes written (optional). This
 *                      is updated even upon failure, as it can fail when part
 *                      of the data has been written.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_write_vecs(
    handle_t handle, const io_vec_t *vecs, size_t count, offset_t offset,
    size_t *_bytes)
{
    object_handle_t *khandle;
    io_vec_t *kvecs;
    io_request_t request;
    status_t ret, err;

    request.transferred = 0;

    if (!vecs) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    kvecs = kmalloc(sizeof(*kvecs) * count, MM_USER);
    if (!kvecs) {
        ret = STATUS_NO_MEMORY;
        goto out;
    }

    ret = memcpy_from_user(kvecs, vecs, sizeof(*kvecs) * count);
    if (ret != STATUS_SUCCESS) {
        kfree(kvecs);
        goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS) {
        kfree(kvecs);
        goto out;
    }

    ret = io_request_init(&request, kvecs, count, offset, IO_OP_WRITE, IO_TARGET_USER);
    kfree(kvecs);
    if (ret != STATUS_SUCCESS) {
        object_handle_release(khandle);
        goto out;
    }

    ret = file_io(khandle, &request);
    io_request_destroy(&request);
    object_handle_release(khandle);

out:
    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

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
 * @param handle        Handle to directory to read from. Must have the
 *                      FILE_ACCESS_READ access right.
 * @param buf           Buffer to read entry in to.
 * @param size          Size of buffer (if not large enough, the function will
 *                      return STATUS_TOO_SMALL).
 *
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_NOT_FOUND if the end of the directory has been
 *                      reached.
 *                      STATUS_TOO_SMALL if the buffer is too small for the
 *                      entry.
 */
status_t kern_file_read_dir(handle_t handle, dir_entry_t *buf, size_t size) {
    object_handle_t *khandle;
    dir_entry_t *kbuf;
    status_t ret;

    if (!buf)
        return STATUS_INVALID_ARG;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    kbuf = kmalloc(size, MM_USER);
    if (!kbuf) {
        object_handle_release(khandle);
        return STATUS_NO_MEMORY;
    }

    ret = file_read_dir(khandle, kbuf, size);
    if (ret == STATUS_SUCCESS)
        ret = memcpy_to_user(buf, kbuf, kbuf->length);

    kfree(kbuf);
    object_handle_release(khandle);
    return ret;
}

/** Rewind to the beginning of a directory.
 * @param handle        Handle to directory to rewind.
 * @return              Status code describing result of the operation. */
status_t kern_file_rewind_dir(handle_t handle) {
    object_handle_t *khandle;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_rewind_dir(khandle);
    object_handle_release(khandle);
    return ret;
}

/** Get file handle state.
 * @param handle        Handle to get state for.
 * @param _access       Where to store access rights (optional).
 * @param _flags        Where to store handle flags (optional).
 * @param _offset       Where to store current offset (optional).
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is not a file.
 *                      STATUS_NOT_SUPPORTED if attempting to retrieve current
 *                      offset and the file is not seekable. */
status_t kern_file_state(handle_t handle, uint32_t *_access, uint32_t *_flags, offset_t *_offset) {
    object_handle_t *khandle;
    uint32_t access, flags;
    offset_t offset;
    status_t ret;

    if (!_access && !_flags && !_offset)
        return STATUS_INVALID_ARG;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_state(khandle, &access, &flags, (_offset) ? &offset : NULL);
    if (ret != STATUS_SUCCESS)
        goto out;

    if (_access) {
        ret = write_user(_access, access);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    if (_flags) {
        ret = write_user(_flags, flags);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    if (_offset) {
        ret = write_user(_offset, offset);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

out:
    object_handle_release(khandle);
    return ret;
}

/** Set a file handle's flags.
 * @param handle        Handle to set flags for.
 * @param flags         New flags to set. */
status_t kern_file_set_flags(handle_t handle, uint32_t flags) {
    object_handle_t *khandle;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_set_flags(khandle, flags);
    object_handle_release(khandle);
    return ret;
}

/**
 * Set the offset of a file handle.
 *
 * Modifies the offset of a file handle (the position that will next be read
 * from or written to) according to the specified action, and returns the new
 * offset.
 *
 * @param handle        Handle to modify offset of.
 * @param action        Operation to perform (FILE_SEEK_*).
 * @param offset        Value to perform operation with.
 * @param newp          Where to store new offset value (optional).
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_seek(handle_t handle, unsigned action, offset_t offset, offset_t *_result) {
    object_handle_t *khandle;
    offset_t result;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_seek(khandle, action, offset, &result);
    if (ret == STATUS_SUCCESS && _result)
        ret = write_user(_result, result);

    object_handle_release(khandle);
    return ret;
}

/**
 * Modify the size of a file.
 *
 * Modifies the size of a file. If the new size is smaller than the previous
 * size of the file, then the extra data is discarded. If it is larger than the
 * previous size, then the extended space will be filled with zero bytes.
 *
 * @param handle        Handle to file to resize. Must have the
 *                      FILE_ACCESS_WRITE access right.
 * @param size          New size of the file.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_file_resize(handle_t handle, offset_t size) {
    object_handle_t *khandle;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_resize(khandle, size);
    object_handle_release(khandle);
    return ret;
}

/** Get information about a file or directory.
 * @param handle        Handle to file to get information for.
 * @param info          Information structure to fill in.
 * @return              Status code describing result of the operation. */
status_t kern_file_info(handle_t handle, file_info_t *info) {
    object_handle_t *khandle;
    file_info_t kinfo;
    status_t ret;

    if (!info)
        return STATUS_INVALID_ARG;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_info(khandle, &kinfo);
    if (ret == STATUS_SUCCESS)
        ret = memcpy_to_user(info, &kinfo, sizeof(*info));

    object_handle_release(khandle);
    return ret;
}

/** Flush changes to a file to the FS.
 * @param handle        Handle to file to flush.
 * @return              Status code describing result of the operation. */
status_t kern_file_sync(handle_t handle) {
    object_handle_t *khandle;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = file_sync(khandle);
    object_handle_release(khandle);
    return ret;
}

/** Perform a file-specific operation.
 * @param handle        Handle to device to perform operation on.
 * @param request       Operation number to perform.
 * @param in            Optional input buffer containing data to pass to the
 *                      operation handler.
 * @param in_size       Size of input buffer.
 * @param out           Optional output buffer.
 * @param out_size      Size of output buffer.
 * @param _bytes        Where to store number of bytes copied into output buffer.
 * @return              Status code describing result of the operation. */
status_t kern_file_request(
    handle_t handle, unsigned request, const void *in, size_t in_size,
    void *out, size_t out_size, size_t *_bytes)
{
    void *kin = NULL, *kout = NULL;
    object_handle_t *khandle;
    status_t ret, err;
    size_t kout_size;

    if (in_size && !in)
        return STATUS_INVALID_ARG;
    if (out_size && !out)
        return STATUS_INVALID_ARG;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    if (in_size) {
        kin = kmalloc(in_size, MM_USER);
        if (!kin) {
            ret = STATUS_NO_MEMORY;
            goto out;
        }

        ret = memcpy_from_user(kin, in, in_size);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = file_request(khandle, request, kin, in_size, (out) ? &kout : NULL, (out) ? &kout_size : NULL);

    if (kout) {
        if (kout_size > out_size) {
            ret = STATUS_TOO_SMALL;
            goto out;
        }

        err = memcpy_to_user(out, kout, kout_size);
        if (err != STATUS_SUCCESS) {
            ret = err;
            goto out;
        }

        if (_bytes) {
            err = write_user(_bytes, kout_size);
            if (err != STATUS_SUCCESS)
                ret = err;
        }
    }

out:
    if (kin)
        kfree(kin);
    if (kout)
        kfree(kout);

    object_handle_release(khandle);
    return ret;
}
