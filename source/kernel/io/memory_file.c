/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Memory file functions.
 */

#include <io/memory_file.h>
#include <io/request.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/** Data used by a memory file. */
typedef struct memory_file {
    file_t file;                    /**< File header. */
    const void *data;               /**< File data. */
    size_t size;                    /**< Size of the data. */
} memory_file_t;

/** Close a handle to a memory file. */
static void memory_file_close(file_handle_t *handle) {
    kfree(handle->file);
}

/** Perform I/O on a memory file. */
static status_t memory_file_io(file_handle_t *handle, io_request_t *request) {
    memory_file_t *file = (memory_file_t *)handle->file;

    assert(request->op == IO_OP_READ);

    if (request->offset >= (offset_t)file->size)
        return STATUS_SUCCESS;

    size_t size = ((request->offset + request->total) > file->size)
        ? file->size - request->offset
        : request->total;

    return io_request_copy(request, (void *)file->data + request->offset, size, true);
}

/** Get information about a memory file. */
static void memory_file_info(file_handle_t *handle, file_info_t *info) {
    memory_file_t *file = (memory_file_t *)handle->file;

    info->id         = 0;
    info->mount      = 0;
    info->type       = file->file.type;
    info->block_size = 1;
    info->size       = file->size;
    info->links      = 1;
    info->created    = info->accessed = info->modified = unix_time();
}

/** File operations for a memory-backed file. */
static const file_ops_t memory_file_ops = {
    .close = memory_file_close,
    .io    = memory_file_io,
    .info  = memory_file_info,
};

/**
 * Creates a special read-only file that is backed by the given chunk of memory.
 * This is useful to pass data stored in memory to code that expects to be
 * operating on files, such as the module loader. The given memory area will
 * not be duplicated, and therefore it must remain in memory for the lifetime
 * of the handle.
 *
 * @note                Files created with this function do not support being
 *                      memory-mapped.
 *
 * @param buf           Pointer to memory area to use.
 * @param size          Size of memory area.
 *
 * @return              Pointer to handle to file (has FILE_ACCESS_READ set).
 */
object_handle_t *memory_file_create(const void *buf, size_t size) {
    memory_file_t *file = kmalloc(sizeof(*file), MM_BOOT);

    file->file.ops  = &memory_file_ops;
    file->file.type = FILE_TYPE_REGULAR;
    file->data      = buf;
    file->size      = size;

    file_handle_t *handle = file_handle_alloc(&file->file, FILE_ACCESS_READ, 0);
    return file_handle_create(handle);
}
