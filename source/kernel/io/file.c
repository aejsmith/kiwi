/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		File object interface.
 */

#include <io/file.h>
#include <io/request.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <assert.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

/** Determine whether a file is seekable.
 * @param file		File to check.
 * @return		Whether the file is seekable. */
static inline bool is_seekable(file_t *file) {
	return (file->type == FILE_TYPE_REGULAR || file->type == FILE_TYPE_BLOCK);
}

/** Perform an I/O request on a file.
 * @param handle	Handle to file to perform request on.
 * @param request	I/O request to perform.
 * @param bytesp	Where to store number of bytes transferred (optional).
 * @return		Status code describing result of the operation. */
static status_t file_io(object_handle_t *handle, io_request_t *request, size_t *bytesp) {
	file_t *file;
	file_handle_t *data;
	object_rights_t right;
	bool update_offset;
	file_info_t info;
	size_t total = 0;
	status_t ret;

	if(handle->object->type->id != OBJECT_TYPE_FILE) {
		ret = STATUS_INVALID_HANDLE;
		goto out;
	}

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	right = (request->op == IO_OP_WRITE) ? FILE_RIGHT_WRITE : FILE_RIGHT_READ;
	if(!object_handle_rights(handle, right)) {
		ret = STATUS_ACCESS_DENIED;
		goto out;
	}

	if(file->type == FILE_TYPE_DIR || !file->ops->io) {
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	}

	/* Don't do anything more if we have nothing to transfer. */
	if(!request->count) {
		ret = STATUS_SUCCESS;
		goto out;
	}

	/* Determine the offset to perform the I/O at and handle the FILE_APPEND
	 * flag. TODO: We don't handle atomicity at all here. For regular files,
	 * should we lock the handle across the operation so that nothing else
	 * can do I/O while this is in progress? */
	if(is_seekable(file)) {
		if(request->offset < 0) {
			if(request->op == IO_OP_WRITE && data->flags & FILE_APPEND) {
				mutex_lock(&data->lock);
				file->ops->info(file, data, &info);
				data->offset = request->offset = info.size;
				mutex_unlock(&data->lock);
			} else {
				request->offset = data->offset;
			}

			update_offset = true;
		} else {
			update_offset = false;
		}
	} else {
		if(request->offset >= 0) {
			ret = STATUS_NOT_SUPPORTED;
			goto out;
		}

		update_offset = false;
	}

	ret = file->ops->io(file, data, request, &total);
out:
	/* Update the file handle offset. */
	if(total && update_offset) {
		mutex_lock(&data->lock);
		data->offset += total;
		mutex_unlock(&data->lock);
	}

	if(bytesp)
		*bytesp = total;

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
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read data into.
 * @param size		Number of bytes to read. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset to read from. If negative, handle's offset will
 *			be used.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_read(object_handle_t *handle, void *buf, size_t size, offset_t offset,
	size_t *bytesp)
{
	io_vec_t vec;
	io_request_t request;
	status_t ret;

	assert(handle);
	assert(buf);

	vec.buffer = buf;
	vec.size = size;

	ret = io_request_init(&request, &vec, 1, offset, IO_OP_READ,
		IO_TARGET_KERNEL);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = file_io(handle, &request, bytesp);
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
 * @param handle	Handle to file to write to. Must have the
 *			FILE_RIGHT_WRITE access right.
 * @param buf		Buffer containing data to write.
 * @param size		Number of bytes to write. The supplied buffer should be
 *			at least this size.
 * @param offset	Offset to write to. If negative, handle's offset will
 *			be used.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_write(object_handle_t *handle, const void *buf, size_t size,
	offset_t offset, size_t *bytesp)
{
	io_vec_t vec;
	io_request_t request;
	status_t ret;

	assert(handle);
	assert(buf);

	vec.buffer = (void *)buf;
	vec.size = size;

	ret = io_request_init(&request, &vec, 1, offset, IO_OP_WRITE,
		IO_TARGET_KERNEL);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = file_io(handle, &request, bytesp);
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
 * @param handle	Handle to file to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param vecs		I/O vectors describing buffers to read into.
 * @param count		Number of I/O vectors.
 * @param offset	Offset to read from. If negative, handle's offset will
 *			be used.
 * @param bytesp	Where to store number of bytes read (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been read.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_read_vecs(object_handle_t *handle, const io_vec_t *vecs, size_t count,
	offset_t offset, size_t *bytesp)
{
	io_request_t request;
	status_t ret;

	assert(handle);

	ret = io_request_init(&request, vecs, count, offset, IO_OP_READ,
		IO_TARGET_KERNEL);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = file_io(handle, &request, bytesp);
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
 * @param handle	Handle to file to write to. Must have the
 *			FILE_RIGHT_WRITE access right.
 * @param vecs		I/O vectors describing buffers containing data to write.
 * @param count		Number of I/O vectors.
 * @param offset	Offset to write to. If negative, handle's offset will
 *			be used.
 * @param bytesp	Where to store number of bytes written (optional). This
 *			is updated even upon failure, as it can fail when part
 *			of the data has been written.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_write_vecs(object_handle_t *handle, const io_vec_t *vecs, size_t count,
	offset_t offset, size_t *bytesp)
{
	io_request_t request;
	status_t ret;

	assert(handle);

	ret = io_request_init(&request, vecs, count, offset, IO_OP_WRITE,
		IO_TARGET_KERNEL);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = file_io(handle, &request, bytesp);
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
 * @param handle	Handle to directory to read from. Must have the
 *			FILE_RIGHT_READ access right.
 * @param buf		Buffer to read entry in to.
 * @param size		Size of buffer (if not large enough, the function will
 *			return STATUS_TOO_SMALL).
 *
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_NOT_FOUND if the end of the directory has been
 *			reached.
 *			STATUS_TOO_SMALL if the buffer is too small for the
 *			entry.
 */
status_t file_read_dir(object_handle_t *handle, dir_entry_t *buf, size_t size) {
	file_t *file;
	file_handle_t *data;
	dir_entry_t *entry;
	status_t ret;

	assert(handle);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	if(!object_handle_rights(handle, FILE_RIGHT_READ)) {
		return STATUS_ACCESS_DENIED;
	} else if(file->type != FILE_TYPE_DIR) {
		return STATUS_NOT_DIR;
	} else if(!file->ops->read_dir) {
		return STATUS_NOT_SUPPORTED;
	}

	/* Lock the handle around the call, the implementation is allowed to
	 * modify the offset. */
	mutex_lock(&data->lock);
	ret = file->ops->read_dir(file, data, &entry);
	mutex_unlock(&data->lock);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(entry->length > size) {
		kfree(entry);
		return STATUS_TOO_SMALL;
	}

	memcpy(buf, entry, entry->length);
	kfree(entry);
	return STATUS_SUCCESS;
}

/** Rewind to the beginning of a directory.
 * @param handle	Handle to directory to rewind.
 * @return		Status code describing result of the operation. */
status_t file_rewind_dir(object_handle_t *handle) {
	file_t *file;
	file_handle_t *data;

	assert(handle);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	if(!object_handle_rights(handle, FILE_RIGHT_READ)) {
		return STATUS_ACCESS_DENIED;
	} else if(file->type != FILE_TYPE_DIR) {
		return STATUS_NOT_DIR;
	} else if(!file->ops->read_dir) {
		return STATUS_NOT_SUPPORTED;
	}

	mutex_lock(&data->lock);
	data->offset = 0;
	mutex_unlock(&data->lock);
	return STATUS_SUCCESS;
}

/**
 * Modify the size of a file.
 *
 * Modifies the size of a file. If the new size is smaller than the previous
 * size of the file, then the extra data is discarded. If it is larger than the
 * previous size, then the extended space will be filled with zero bytes.
 *
 * @param handle	Handle to file to resize. Must have the FILE_RIGHT_WRITE
 *			access right.
 * @param size		New size of the file.
 *
 * @return		Status code describing result of the operation.
 */
status_t file_resize(object_handle_t *handle, offset_t size) {
	file_t *file;
	file_handle_t *data;

	assert(handle);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	if(!object_handle_rights(handle, FILE_RIGHT_WRITE)) {
		return STATUS_ACCESS_DENIED;
	} else if(file->type != FILE_TYPE_REGULAR) {
		return STATUS_NOT_REGULAR;
	} else if(!file->ops->resize) {
		return STATUS_NOT_SUPPORTED;
	}

	return file->ops->resize(file, data, size);
}

/**
 * Set the offset of a file handle.
 *
 * Modifies the offset of a file handle (the position that will next be read
 * from or written to) according to the specified action, and returns the new
 * offset.
 *
 * @param handle	Handle to modify offset of.
 * @param action	Operation to perform (FILE_SEEK_*).
 * @param offset	Value to perform operation with.
 * @param newp		Where to store new offset value (optional).
 *
 * @return		Status code describing result of the operation.
 */
status_t file_seek(object_handle_t *handle, unsigned action, offset_t offset,
	offset_t *resultp)
{
	file_t *file;
	file_handle_t *data;
	offset_t result;
	file_info_t info;

	assert(handle);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	if(!is_seekable(file))
		return STATUS_NOT_SUPPORTED;

	mutex_lock(&data->lock);

	switch(action) {
	case FILE_SEEK_SET:
		result = offset;
		break;
	case FILE_SEEK_ADD:
		result = data->offset + offset;
		break;
	case FILE_SEEK_END:
		file->ops->info(file, data, &info);
		result = info.size + offset;
		break;
	default:
		mutex_unlock(&data->lock);
		return STATUS_INVALID_ARG;
	}

	if(result < 0) {
		mutex_unlock(&data->lock);
		return STATUS_INVALID_ARG;
	}

	data->offset = result;
	mutex_unlock(&data->lock);

	if(resultp)
		*resultp = result;

	return STATUS_SUCCESS;
}

/** Get information about a file or directory.
 * @param handle	Handle to file to get information for.
 * @param info		Information structure to fill in.
 * @return		Status code describing result of the operation. */
status_t file_info(object_handle_t *handle, file_info_t *info) {
	file_t *file;
	file_handle_t *data;

	assert(handle);
	assert(info);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	file->ops->info(file, data, info);
	return STATUS_SUCCESS;
}

/** Flush changes to a file to the FS.
 * @param handle	Handle to file to flush.
 * @return		Status code describing result of the operation. */
status_t file_sync(object_handle_t *handle) {
	file_t *file;
	file_handle_t *data;

	assert(handle);

	if(handle->object->type->id != OBJECT_TYPE_FILE)
		return STATUS_INVALID_HANDLE;

	file = (file_t *)handle->object;
	data = (file_handle_t *)handle->data;

	return file->ops->sync(file, data);
}

object_handle_t *file_from_memory(const void *buf, size_t size) {
	fatal("meow");
}

status_t kern_file_read(handle_t handle, void *buf, size_t size, offset_t offset,
	size_t *bytesp)
{
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_write(handle_t handle, const void *buf, size_t size,
	offset_t offset, size_t *bytesp)
{
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_read_vecs(handle_t handle, const io_vec_t *vecs, size_t count,
	offset_t offset, size_t *bytesp)
{
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_write_vecs(handle_t handle, const io_vec_t *vecs, size_t count,
	offset_t offset, size_t *bytesp)
{
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_read_dir(handle_t handle, dir_entry_t *buf, size_t size) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_rewind_dir(handle_t handle) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_resize(handle_t handle, offset_t size) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_seek(handle_t handle, unsigned action, offset_t offset,
	offset_t *resultp)
{
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_info(handle_t handle, file_info_t *info) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t kern_file_sync(handle_t handle) {
	return STATUS_NOT_IMPLEMENTED;
}
