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
 * @brief		Memory file functions.
 */

#include <io/memory_file.h>
#include <io/request.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/** Data used by a memory file. */
typedef struct memory_file {
	file_t file;			/**< File header. */
	const void *data;		/**< File data. */
	size_t size;			/**< Size of the data. */
} memory_file_t;

/** Close a handle to a memory file.
 * @param file		File being closed.
 * @param handle	File handle structure. */
static void memory_file_close(file_t *file, file_handle_t *handle) {
	kfree(file);
}

/** Perform I/O on a memory file.
 * @param file		File to perform I/O on.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t memory_file_io(file_t *file, file_handle_t *handle, io_request_t *request) {
	memory_file_t *data = (memory_file_t *)file;
	size_t size;

	assert(request->op == IO_OP_READ);

	if(request->offset >= (offset_t)data->size)
		return STATUS_SUCCESS;

	size = ((request->offset + request->total) > data->size)
		? data->size - request->offset
		: request->total;

	return io_request_copy(request, (void *)data->data + request->offset, size);
}

/** Get information about a memory file.
 * @param file		File to get information on.
 * @param handle	File handle structure.
 * @param info		Information structure to fill in. */
static void memory_file_info(file_t *file, file_handle_t *handle, file_info_t *info) {
	memory_file_t *data = (memory_file_t *)file;

	info->id = 0;
	info->mount = 0;
	info->type = file->type;
	info->block_size = 1;
	info->size = data->size;
	info->links = 1;
	info->created = info->accessed = info->modified = unix_time();
}

/** File operations for a memory-backed file. */
static file_ops_t memory_file_ops = {
	.close = memory_file_close,
	.io = memory_file_io,
	.info = memory_file_info,
};

/**
 * Create a read-only file backed by a chunk of memory.
 *
 * Creates a special read-only file that is backed by the given chunk of memory.
 * This is useful to pass data stored in memory to code that expects to be
 * operating on files, such as the module loader. The given memory area will
 * not be duplicated, and therefore it must remain in memory for the lifetime
 * of the handle.
 *
 * @note		Files created with this function do not support being
 *			memory-mapped.
 *
 * @param buf		Pointer to memory area to use.
 * @param size		Size of memory area.
 *
 * @return		Pointer to handle to file (has FILE_RIGHT_READ right).
 */
object_handle_t *memory_file_create(const void *buf, size_t size) {
	memory_file_t *file;

	file = kmalloc(sizeof(*file), MM_BOOT);
	file_init(&file->file, &memory_file_ops, FILE_TYPE_REGULAR);
	file->data = buf;
	file->size = size;

	return file_handle_create(&file->file, FILE_RIGHT_READ, 0, NULL);
}
