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
 * @brief               User file API.
 *
 * TODO:
 *  - Currently this only supports basic direct I/O, with no memory mapping or
 *    any kind of kernel level caching. In future, this could be extended to
 *    allow both of these by adding a page-based I/O model (read/write whole
 *    pages at a time) with a vm_cache in the kernel. However, the current
 *    model would need to be retained as an option, as page-based I/O is not
 *    suitable for implementation of character devices.
 *  - This could later be expanded to allow full filesystem implementations in
 *    user mode, like FUSE.
 */

#include <io/file.h>
#include <io/request.h>

#include <ipc/ipc.h>

#include <kernel/status.h>
#include <kernel/user_file.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <assert.h>

/** User file operation structure. */
typedef struct user_file_op {
    list_t header;

    unsigned id;                        /**< Operation ID. */
    uint64_t serial;                    /**< Serial number. */
    bool complete;                      /**< Whether completed. */
    ipc_kmessage_t *msg;                /**< Message to send, message received if complete. */
    condvar_t cvar;                     /**< Condition variable to wait for completion. */
} user_file_op_t;

/** User file structure. */
typedef struct user_file {
    file_t file;

    mutex_t lock;                       /**< Lock for file. */
    refcount_t count;                   /**< Reference count of open handles. */
    ipc_endpoint_t *endpoint;           /**< Endpoint for kernel side of the connection. */
    list_t ops;                         /**< Outstanding operations. */
    uint64_t next_serial;               /**< Next operation serial number. */
} user_file_t;

static slab_cache_t *user_file_op_cache;

/**
 * Closes the file's connection, cancels all outstanding operations, and makes
 * all subsequent operations fail. Done in response to the other side closing
 * the connection, or invalid data being received from it.
 */
static void user_file_terminate(user_file_t *file) {
    if (file->endpoint) {
        ipc_connection_close(file->endpoint);
        file->endpoint = NULL;
    }

    /* Cancel outstanding operations. */
    list_foreach(&file->ops, iter) {
        user_file_op_t *op = list_entry(iter, user_file_op_t, header);
        condvar_signal(&op->cvar);
    }
}

/** Indicate that an invalid reply has been received for an operation. */
static status_t user_file_invalid_reply(user_file_t *file, user_file_op_t *op) {
    kprintf(LOG_DEBUG, "user_file: invalid reply received for operation %d, terminating\n", op->id);
    user_file_terminate(file);
    return STATUS_DEVICE_ERROR;
}

static void user_file_op_ctor(void *obj, void *data) {
    user_file_op_t *op = obj;

    list_init(&op->header);
    condvar_init(&op->cvar, "user_file_op");
}

static user_file_op_t *user_file_op_alloc(user_file_t *file, unsigned id, size_t size) {
    assert(size <= IPC_DATA_MAX);

    user_file_op_t *op = slab_cache_alloc(user_file_op_cache, MM_KERNEL);

    op->id          = id;
    op->serial      = file->next_serial++;
    op->complete    = false;
    op->msg         = ipc_kmessage_alloc();
    op->msg->msg.id = id;

    op->msg->msg.args[USER_FILE_MESSAGE_ARG_SERIAL] = op->serial;

    if (size > 0) {
        void *data = kmalloc(size, MM_KERNEL);
        ipc_kmessage_set_data(op->msg, data, size);
    }

    return op;
}

static void user_file_op_free(user_file_op_t *op) {
    if (op->msg)
        ipc_kmessage_release(op->msg);

    slab_cache_free(user_file_op_cache, op);
}

static status_t user_file_op_send(user_file_t *file, user_file_op_t *op) {
    if (!file->endpoint)
        return STATUS_DEVICE_ERROR;

    status_t ret = ipc_connection_send(file->endpoint, op->msg, IPC_INTERRUPTIBLE, MM_KERNEL);

    /* Don't need this any more. If we return success, it'll be replaced with
     * the reply message. */
    ipc_kmessage_release(op->msg);
    op->msg = NULL;

    if (ret == STATUS_SUCCESS) {
        list_append(&file->ops, &op->header);

        /* Wait for completion. */
        ret = condvar_wait_etc(&op->cvar, &file->lock, -1, SLEEP_INTERRUPTIBLE);

        list_remove(&op->header);

        /* If we're woken and not complete, the connection hung up. */
        if (ret == STATUS_SUCCESS) {
            if (!op->complete) {
                assert(!file->endpoint);
                ret = STATUS_DEVICE_ERROR;
            } else {
                assert(op->msg);

                if (op->msg->msg.id != op->id)
                    ret = user_file_invalid_reply(file, op);
            }
        }
    } else if (ret == STATUS_CONN_HUNGUP) {
        user_file_terminate(file);
        ret = STATUS_DEVICE_ERROR;
    }

    return ret;
}

/** Handle a message received on a user file endpoint. */
static status_t user_file_endpoint_receive(
    ipc_endpoint_t *endpoint, ipc_kmessage_t *msg, unsigned flags,
    nstime_t timeout)
{
    user_file_t *file = endpoint->private;

    mutex_lock(&file->lock);

    uint64_t serial = msg->msg.args[USER_FILE_MESSAGE_ARG_SERIAL];
    status_t ret    = STATUS_CANCELLED;

    list_foreach(&file->ops, iter) {
        user_file_op_t *op = list_entry(iter, user_file_op_t, header);

        if (op->serial == serial) {
            assert(!op->msg);

            ipc_kmessage_retain(msg);

            op->msg      = msg;
            op->complete = true;

            condvar_signal(&op->cvar);
            list_remove(&op->header);

            ret = STATUS_SUCCESS;
            break;
        }
    }

    mutex_unlock(&file->lock);
    return ret;
}

/** Handle the connection being closed. */
static void user_file_endpoint_close(ipc_endpoint_t *endpoint) {
    user_file_t *file = endpoint->private;

    mutex_lock(&file->lock);
    user_file_terminate(file);
    mutex_unlock(&file->lock);
}

static ipc_endpoint_ops_t user_file_endpoint_ops = {
    .receive = user_file_endpoint_receive,
    .close   = user_file_endpoint_close,
};

/** Open a user file. */
static status_t user_file_open(file_handle_t *handle) {
    user_file_t *file = handle->user_file;

    refcount_inc(&file->count);
    return STATUS_SUCCESS;
}

/** Close a user file. */
static void user_file_close(file_handle_t *handle) {
    user_file_t *file = handle->user_file;

    if (refcount_dec(&file->count) == 0) {
        /* This will prevent any more messages from being sent on the connection
         * if the other side still has a handle open, which means our callbacks
         * won't be called so it is safe to free the file after this. */
        if (file->endpoint)
            ipc_connection_close(file->endpoint);

        assert(list_empty(&file->ops));

        kfree(file);
    }
}

/** Perform I/O on a user file. */
static status_t user_file_io(file_handle_t *handle, io_request_t *request) {
    user_file_t *file = handle->user_file;
    status_t ret      = STATUS_SUCCESS;

    mutex_lock(&file->lock);

    user_file_op_t *op = NULL;

    /* We need to split into chunks of IPC_DATA_MAX or less. */
    while (request->transferred < request->total) {
        offset_t offset = request->offset + request->transferred;
        size_t size     = min(request->total - request->transferred, IPC_DATA_MAX);

        if (request->op == IO_OP_READ) {
            op = user_file_op_alloc(file, USER_FILE_OP_READ, 0);

            op->msg->msg.args[USER_FILE_MESSAGE_ARG_READ_OFFSET] = offset;
            op->msg->msg.args[USER_FILE_MESSAGE_ARG_READ_SIZE]   = size;
        } else {
            op = user_file_op_alloc(file, USER_FILE_OP_WRITE, size);

            ret = io_request_copy(request, op->msg->data, size);
            if (ret != STATUS_SUCCESS)
                break;

            op->msg->msg.args[USER_FILE_MESSAGE_ARG_WRITE_OFFSET] = offset;
        }

        op->msg->msg.args[USER_FILE_MESSAGE_ARG_FLAGS] = handle->flags;

        ret = user_file_op_send(file, op);
        if (ret != STATUS_SUCCESS)
            break;

        assert(op->msg);

        size_t transfer_size;

        if (request->op == IO_OP_READ) {
            transfer_size = op->msg->msg.size;

            if (transfer_size > size) {
                ret = user_file_invalid_reply(file, op);
            } else if (transfer_size > 0) {
                ret = io_request_copy(request, op->msg->data, transfer_size);
            }

            if (ret == STATUS_SUCCESS)
                ret = op->msg->msg.args[USER_FILE_MESSAGE_ARG_READ_STATUS];
        } else {
            transfer_size = op->msg->msg.args[USER_FILE_MESSAGE_ARG_WRITE_SIZE];

            if (transfer_size > size) {
                ret           = user_file_invalid_reply(file, op);
                transfer_size = 0;
            } else {
                ret = op->msg->msg.args[USER_FILE_MESSAGE_ARG_WRITE_STATUS];
            }

            /* If less data was written than we asked, update the I/O request
             * (transferred count was updated when we copied out of it before
             * sending the op). */
            request->transferred -= size - transfer_size;
        }

        /* Stop if any error was indicated or we have transferred less than we
         * should have (e.g. end of file). */
        if (ret != STATUS_SUCCESS || transfer_size < size)
            break;

        user_file_op_free(op);
        op = NULL;
    }

    if (op)
        user_file_op_free(op);

    mutex_unlock(&file->lock);
    return ret;
}

/** Get information about a file. */
static void user_file_info(file_handle_t *handle, file_info_t *info) {
    user_file_t *file = handle->user_file;
    status_t ret;

    memset(info, 0, sizeof(*info));

    mutex_lock(&file->lock);

    user_file_op_t *op = user_file_op_alloc(file, USER_FILE_OP_INFO, 0);

    ret = user_file_op_send(file, op);
    if (ret == STATUS_SUCCESS) {
        assert(op->msg);

        if (op->msg->msg.size != sizeof(*info)) {
            user_file_invalid_reply(file, op);
        } else {
            memcpy(info, op->msg->data, sizeof(*info));
        }
    }

    mutex_unlock(&file->lock);

    user_file_op_free(op);

    /* We always set these ourself and override what we were sent. */
    info->mount = 0;
    info->type  = file->file.type;
}

/** Handler for file-specific requests. */
static status_t user_file_request(
    struct file_handle *handle, unsigned request, const void *in,
    size_t in_size, void **_out, size_t *_out_size)
{
    user_file_t *file = handle->user_file;
    status_t ret;

    /* Has to fit in a single message. */
    if (in_size > IPC_DATA_MAX)
        return STATUS_TOO_LARGE;

    mutex_lock(&file->lock);

    user_file_op_t *op = user_file_op_alloc(file, USER_FILE_OP_REQUEST, in_size);

    if (in_size > 0)
        memcpy(op->msg->data, in, in_size);

    op->msg->msg.args[USER_FILE_MESSAGE_ARG_FLAGS]       = handle->flags;
    op->msg->msg.args[USER_FILE_MESSAGE_ARG_REQUEST_NUM] = request;

    ret = user_file_op_send(file, op);
    if (ret == STATUS_SUCCESS) {
        assert(op->msg);

        if (_out) {
            /* Take over this buffer from the message. */
            *_out      = op->msg->data;
            *_out_size = op->msg->msg.size;

            op->msg->data     = NULL;
            op->msg->msg.size = 0;
        }

        ret = op->msg->msg.args[USER_FILE_MESSAGE_ARG_REQUEST_STATUS];
    }

    mutex_unlock(&file->lock);

    user_file_op_free(op);

    return ret;
}

static file_ops_t user_file_ops = {
    .open    = user_file_open,
    .close   = user_file_close,
    .io      = user_file_io,
    .info    = user_file_info,
    .request = user_file_request,
};

/**
 * Creates a new user file. A user file is one on which all operations are
 * implemented by a user mode process (the one which created it).
 *
 * Two handles are returned by this function:
 *  - A file handle. This can be used like any other file handle and passed to
 *    other processes via inheritance, IPC, etc.
 *  - A connection handle. This is a connection between the kernel and the
 *    calling process which implements operations on the file. Operations
 *    performed on the file will result in a message being sent by the kernel
 *    over this connection, and replies complete the operations.
 *
 * @param type          Type of the file.
 * @param access        Requested access rights for the file handle.
 * @param flags         Behaviour flags for the file handle.
 * @param _conn         Where to return connection handle (must not be NULL).
 * @param _file         Where to return file handle (must not be NULL).
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if any arguments are invalid.
 *                      STATUS_NO_HANDLES if there is no free space in the
 *                      handle table.
 */
status_t kern_user_file_create(
    file_type_t type, uint32_t access, uint32_t flags, handle_t *_conn,
    handle_t *_file)
{
    status_t ret;

    if (!_conn || !_file)
        return STATUS_INVALID_ARG;

    user_file_t *file = kmalloc(sizeof(user_file_t), MM_KERNEL);

    mutex_init(&file->lock, "user_file_lock", 0);
    refcount_set(&file->count, 1);
    list_init(&file->ops);

    file->file.ops    = &user_file_ops;
    file->file.type   = type;
    file->next_serial = 0;

    // TODO: Initialize ACL. To what?

    handle_t conn;
    ret = ipc_connection_create(0, &user_file_endpoint_ops, file, &file->endpoint, &conn, _conn);
    if (ret != STATUS_SUCCESS)
        goto err_free;

    ret = file_handle_attach(&file->file, access, flags, NULL, _file);
    if (ret != STATUS_SUCCESS)
        goto err_close_conn;

    return STATUS_SUCCESS;

err_close_conn:
    ipc_connection_close(file->endpoint);
    object_handle_detach(conn);

err_free:
    kfree(file);
    return ret;
}

static __init_text void user_file_init(void) {
    user_file_op_cache = object_cache_create(
        "user_file_op_cache",
        user_file_op_t, user_file_op_ctor, NULL, NULL, 0, MM_BOOT);
}

INITCALL(user_file_init);
