/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Unidirectional data pipe implementation.
 */

#include <io/request.h>

#include <ipc/pipe.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kmem.h>
#include <mm/malloc.h>

#include <proc/thread.h>

#include <assert.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

/** Next pipe ID (for debug naming). */
static atomic_uint32_t next_pipe_id = 0;

/** Waits for any amount of data. */
static status_t wait_data(pipe_t *pipe, bool nonblock) {
    while (pipe->write_open && pipe->count == 0) {
        if (nonblock)
            return STATUS_WOULD_BLOCK;

        status_t ret = condvar_wait_etc(&pipe->data_cvar, &pipe->lock, -1, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/** Waits for at least the specified amount of space. */
static status_t wait_space(pipe_t *pipe, size_t size, bool nonblock) {
    while (pipe->read_open && (PIPE_SIZE - pipe->count) < size) {
        if (nonblock)
            return STATUS_WOULD_BLOCK;

        status_t ret = condvar_wait_etc(&pipe->space_cvar, &pipe->lock, -1, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    /* If the read end closes we fail. */
    return (pipe->read_open) ? STATUS_SUCCESS : STATUS_PIPE_CLOSED;
}

/** Performs I/O on a pipe.
 * @param pipe          Pipe to perform I/O on.
 * @param request       I/O request.
 * @param nonblock      Whether to allow blocking.
 * @return              Status code describing result of the operation. */
status_t pipe_io(pipe_t *pipe, io_request_t *request, bool nonblock) {
    mutex_lock(&pipe->lock);

    status_t ret = STATUS_SUCCESS;

    /*
     * Writes <= PIPE_SIZE should be atomic (all data must be written as a
     * contiguous chunk), but anything larger can be non-atomic (interleaved
     * with other writes).
     *
     * Therefore we split up the operation into PIPE_SIZE chunks.
     *
     * We also do this for reads to transfer data in batches where possible
     * rather than byte-wise, but reads can return less data than requested in
     * some conditions.
     */
    while (request->transferred < request->total) {
        size_t remaining = request->total - request->transferred;
        size_t size      = min(remaining, PIPE_SIZE);
        size_t pos;

        if (request->op == IO_OP_READ) {
            /* Amount can be less (or 0 if write end closed) after this returns. */
            ret  = wait_data(pipe, nonblock);
            size = min(size, pipe->count);
            if (ret != STATUS_SUCCESS || size == 0)
                break;

            pos = pipe->start;
        } else {
            ret = wait_space(pipe, size, nonblock);
            if (ret != STATUS_SUCCESS)
                break;

            assert((PIPE_SIZE - pipe->count) >= size);
            pos = (pipe->start + pipe->count) % PIPE_SIZE;
        }

        status_t err;

        /* Buffer is circular so we might need to split into 2 copies. */
        if (pos + size > PIPE_SIZE) {
            size_t split = PIPE_SIZE - pos;

            err = io_request_copy(request, &pipe->buf[pos], split, true);
            if (err == STATUS_SUCCESS) {
                err = io_request_copy(request, &pipe->buf[0], size - split, true);
                if (err != STATUS_SUCCESS) {
                    /* Don't do a partial transfer in the copy fail case. */
                    request->transferred -= split;
                }
            }
        } else {
            err = io_request_copy(request, &pipe->buf[pos], size, true);
        }

        /* Only update the pipe if we succeeded in copying. */
        if (err == STATUS_SUCCESS) {
            if (request->op == IO_OP_READ) {
                pipe->start  = (pipe->start + size) % PIPE_SIZE;
                pipe->count -= size;

                condvar_broadcast(&pipe->space_cvar);
                notifier_run(&pipe->space_notifier, NULL, false);
            } else {
                pipe->count += size;

                condvar_broadcast(&pipe->data_cvar);
                notifier_run(&pipe->data_notifier, NULL, false);
            }
        } else {
            ret = err;
            break;
        }
    }

    mutex_unlock(&pipe->lock);
    return ret;
}

/**
 * Waits for a pipe to become readable or writable, and notifies the specified
 * object wait pointer when it is. This is a convenience function, for example
 * for devices that use pipes internally.
 *
 * @param pipe          Pipe to wait for.
 * @param write         Whether to wait to be writable (pipe is classed as
 *                      writable when there is space in the buffer).
 * @param event         Object event structure.
 */
void pipe_wait(pipe_t *pipe, bool write, object_event_t *event) {
    mutex_lock(&pipe->lock);

    if (write) {
        /* Pipe is not writable if the other end is closed. */
        if (pipe->count < PIPE_SIZE && pipe->read_open) {
            object_event_signal(event, 0);
        } else {
            notifier_register(&pipe->space_notifier, object_event_notifier, event);
        }
    } else {
        /* Consider the pipe readable if the other end is closed. */
        if (pipe->count > 0 || !pipe->write_open) {
            object_event_signal(event, 0);
        } else {
            notifier_register(&pipe->data_notifier, object_event_notifier, event);
        }
    }

    mutex_unlock(&pipe->lock);
}

/** Stops waiting for a pipe event.
 * @param pipe          Pipe to stop waiting for.
 * @param write         Whether waiting to be writable.
 * @param event         Object event structure. */
void pipe_unwait(pipe_t *pipe, bool write, object_event_t *event) {
    notifier_t *notifier = (write) ? &pipe->space_notifier : &pipe->data_notifier;
    notifier_unregister(notifier, object_event_notifier, event);
}

static char *pipe_file_name(file_handle_t *handle) {
    pipe_t *pipe = handle->pipe;

    const size_t prefix_len  = strlen("pipe");
    const size_t u32_max_len = 10;
    const size_t len         = prefix_len + u32_max_len + 2;

    char *name = kmalloc(len, MM_KERNEL);
    snprintf(name, len, "pipe:%" PRIu32, pipe->id);
    return name;
}

static char *pipe_file_name_unsafe(file_handle_t *handle, char *buf, size_t size) {
    pipe_t *pipe = handle->pipe;

    snprintf(buf, size, "pipe:%" PRIu32, pipe->id);
    return buf;
}

static void pipe_file_close(file_handle_t *handle) {
    pipe_t *pipe = handle->pipe;

    assert(handle->access == FILE_ACCESS_READ || handle->access == FILE_ACCESS_WRITE);

    mutex_lock(&pipe->lock);

    /* This will need changing to refcounts if we add support for reopen. */
    if (handle->access & FILE_ACCESS_READ) {
        assert(pipe->read_open);
        pipe->read_open = false;

        /* Wake anyone waiting for space so that they can fail. */
        condvar_broadcast(&pipe->space_cvar);
    } else {
        assert(pipe->write_open);
        pipe->write_open = false;

        /* Wake anyone waiting for data so that they can fail. */
        condvar_broadcast(&pipe->data_cvar);
        notifier_run(&pipe->data_notifier, NULL, false);
    }

    bool destroy = !pipe->read_open && !pipe->write_open;

    mutex_unlock(&pipe->lock);

    if (destroy)
        pipe_destroy(pipe);
}

static status_t pipe_file_wait(file_handle_t *handle, object_event_t *event) {
    pipe_t *pipe = handle->pipe;

    switch (event->event) {
        case FILE_EVENT_READABLE:
            /* It'll never become readable if this isn't the read end. */
            if (handle->access & FILE_ACCESS_READ)
                pipe_wait(pipe, false, event);

            return STATUS_SUCCESS;
        case FILE_EVENT_WRITABLE:
            if (handle->access & FILE_ACCESS_WRITE)
                pipe_wait(pipe, true, event);

            return STATUS_SUCCESS;
        default:
            return STATUS_INVALID_EVENT;
    }
}

static void pipe_file_unwait(file_handle_t *handle, object_event_t *event) {
    pipe_t *pipe = handle->pipe;

    switch (event->event) {
        case FILE_EVENT_READABLE:
            pipe_unwait(pipe, false, event);
            break;
        case FILE_EVENT_WRITABLE:
            pipe_unwait(pipe, true, event);
            break;
    }
}

static status_t pipe_file_io(file_handle_t *handle, io_request_t *request) {
    pipe_t *pipe   = handle->pipe;
    uint32_t flags = file_handle_flags(handle);
    return pipe_io(pipe, request, flags & FILE_NONBLOCK);
}

static void pipe_file_info(file_handle_t *handle, file_info_t *info) {
    info->type       = FILE_TYPE_PIPE;
    info->links      = 1;
    info->block_size = PAGE_SIZE;
}

static const file_ops_t pipe_file_ops = {
    .close       = pipe_file_close,
    .name        = pipe_file_name,
    .name_unsafe = pipe_file_name_unsafe,
    .wait        = pipe_file_wait,
    .unwait      = pipe_file_unwait,
    .io          = pipe_file_io,
    .info        = pipe_file_info,
};

/** Creates a new pipe.
 * @param mmflag        Allocation flags to use for allocating the pipe buffer.
 * @return              Pointer to pipe, or null on allocation failure. */
pipe_t *pipe_create(unsigned mmflag) {
    pipe_t *pipe = kmalloc(sizeof(*pipe), MM_KERNEL);

    mutex_init(&pipe->lock, "pipe_lock", 0);
    condvar_init(&pipe->space_cvar, "pipe_space_cvar");
    condvar_init(&pipe->data_cvar, "pipe_data_cvar");
    notifier_init(&pipe->space_notifier, pipe);
    notifier_init(&pipe->data_notifier, pipe);

    pipe->id         = atomic_fetch_add(&next_pipe_id, 1);
    pipe->file.ops   = &pipe_file_ops;
    pipe->file.type  = FILE_TYPE_PIPE;
    pipe->read_open  = true;
    pipe->write_open = true;
    pipe->start      = 0;
    pipe->count      = 0;

    pipe->buf = kmem_alloc(PIPE_SIZE, mmflag);
    if (!pipe->buf) {
        kfree(pipe);
        return NULL;
    }

    return pipe;
}

/**
 * Destroys a pipe. The caller must ensure that nothing is using the pipe.
 *
 * @param pipe          Pipe to destroy.
 */
void pipe_destroy(pipe_t *pipe) {
    assert(!mutex_held(&pipe->lock));
    assert(notifier_empty(&pipe->space_notifier));
    assert(notifier_empty(&pipe->data_notifier));

    kmem_free(pipe->buf, PIPE_SIZE);
    kfree(pipe);
}

/**
 * System calls.
 */

/**
 * Creates a pipe, which is a undirectional data channel. Two handles are
 * returned, one referring to the read end and the other to the write end. Data
 * written to the write end is returned when reading from the read end.
 *
 * Pipes have an intermediate buffer with a maximum size. Writing data to a
 * pipe will block if the buffer is full (unless FILE_NONBLOCK is set on the
 * handle), which can happen if data is being written faster than it is being
 * read. Similarly, reading from the pipe will block (unless FILE_NONBLOCK is
 * set) if no data is available in the buffer.
 *
 * Reads of less than or equal to the pipe buffer size will either read all the
 * requested data, or none at all. Reads of greater than the pipe buffer size
 * may only return part of the data. Similarly, writes of less than or equal to
 * the pipe buffer size will either write all the requested data, or none at
 * all. Writes of greater than the pipe buffer size may only write part of the
 * data.
 
 * Attempts to read from a pipe whose write end has been closed will return
 * end-of-file (read 0 bytes). Attempts to write to a pipe whose read end has
 * been closed will return STATUS_PIPE_CLOSED.
 *
 * @param read_flags    Flags to open the read handle with (FILE_*).
 * @param write_flags   Flags to open the write handle with (FILE_*).
 * @param _read         Where to store handle to read end.
 * @param _write        Where to store handle to write end.
 *
 * @return              Status code describing the result of the operation.
 */
status_t kern_pipe_create(
    uint32_t read_flags, uint32_t write_flags, handle_t *_read,
    handle_t *_write)
{
    status_t ret;

    pipe_t *pipe = pipe_create(MM_USER);
    if (!pipe)
        return STATUS_NO_MEMORY;

    /* Prevent another thread coming in and immediately closing the read handle
     * before we've had a chance to try creating the write handle. */
    mutex_lock(&pipe->lock);

    pipe->read_open  = false;
    pipe->write_open = false;

    handle_t read;
    ret = file_handle_open(&pipe->file, FILE_ACCESS_READ, read_flags, &read, _read);
    if (ret != STATUS_SUCCESS) {
        mutex_unlock(&pipe->lock);
        pipe_destroy(pipe);
        return ret;
    }

    pipe->read_open = true;

    ret = file_handle_open(&pipe->file, FILE_ACCESS_WRITE, write_flags, NULL, _write);
    if (ret != STATUS_SUCCESS) {
        mutex_unlock(&pipe->lock);

        /* This should take care of cleaning up since write_open is false. */
        object_handle_detach(read, _read);
        return ret;
    }

    pipe->write_open = true;

    mutex_unlock(&pipe->lock);
    return STATUS_SUCCESS;
}
