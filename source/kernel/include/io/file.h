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

#ifndef __IO_FILE_H
#define __IO_FILE_H

#include <kernel/file.h>

#include <sync/mutex.h>

#include <object.h>

struct file;
struct file_handle;
struct io_request;

/** Operations for a file. */
typedef struct file_ops {
	/** Close a file.
	 * @param file		File being closed.
	 * @param handle	File handle structure. All data allocated for
	 *			the handle should be freed. */
	void (*close)(struct file *file, struct file_handle *handle);

	/** Perform I/O on a file.
	 * @param file		File to perform I/O on.
	 * @param handle	File handle structure.
	 * @param request	I/O request.
	 * @param bytesp	Where to store total number of bytes transferred.
	 * @return		Status code describing result of the operation. */
	status_t (*io)(struct file *file, struct file_handle *handle,
		struct io_request *request, size_t *bytesp);

	/** Read the next directory entry.
	 * @note		The implementation can make use of the offset
	 *			field in the handle to store whatever it needs
	 *			to implement this function. It will be set to
	 *			0 when the handle is initially opened, and
	 *			when rewind_dir() is called on the handle.
	 * @param file		File to read from.
	 * @param handle	File handle structure.
	 * @param entryp	Where to store pointer to directory entry
	 *			structure (must be allocated using a
	 *			kmalloc()-based function).
	 * @return		Status code describing result of the operation. */
	status_t (*read_dir)(struct file *file, struct file_handle *handle,
		dir_entry_t **entryp);

	/** Modify the size of a file.
	 * @param file		File to resize.
	 * @param handle	File handle structure.
	 * @param size		New size of the file.
	 * @return		Status code describing result of the operation. */
	status_t (*resize)(struct file *file, struct file_handle *handle, offset_t size);

	/** Get information about a file.
	 * @param file		File to get information on.
	 * @param handle	File handle structure.
	 * @param infop		Information structure to fill in. */
	void (*info)(struct file *file, struct file_handle *handle, file_info_t *info);

	/** Flush changes to a file.
	 * @param file		File to flush.
	 * @param handle	File handle structure.
	 * @return		Status code describing result of the operation. */
	status_t (*sync)(struct file *file, struct file_handle *handle);
} file_ops_t;

/** Header for a file object. */
typedef struct file {
	object_t obj;			/**< Kernel object header. */

	file_type_t type;		/**< Type of the file. */
	file_ops_t *ops;		/**< File operations structure. */
} file_t;

/** File handle information. */
typedef struct file_handle {
	void *data;			/**< Implementation data pointer. */
	uint32_t flags;			/**< Flags the file was opened with. */
	mutex_t lock;			/**< Lock to protect offset. */
	offset_t offset;		/**< Current file offset. */
} file_handle_t;

extern status_t file_read(object_handle_t *handle, void *buf, size_t size,
	offset_t offset, size_t *bytesp);
extern status_t file_write(object_handle_t *handle, const void *buf, size_t size,
	offset_t offset, size_t *bytesp);

extern status_t file_read_vecs(object_handle_t *handle, const io_vec_t *vecs,
	size_t count, offset_t offset, size_t *bytesp);
extern status_t file_write_vecs(object_handle_t *handle, const io_vec_t *vecs,
	size_t count, offset_t offset, size_t *bytesp);

extern status_t file_read_dir(object_handle_t *handle, dir_entry_t *buf, size_t size);
extern status_t file_rewind_dir(object_handle_t *handle);

extern status_t file_resize(object_handle_t *handle, offset_t size);
extern status_t file_seek(object_handle_t *handle, unsigned action, offset_t offset,
	offset_t *resultp);
extern status_t file_info(object_handle_t *handle, file_info_t *info);
extern status_t file_sync(object_handle_t *handle);

extern object_handle_t *file_from_memory(const void *buf, size_t size);

#endif /* __IO_FILE_H */
