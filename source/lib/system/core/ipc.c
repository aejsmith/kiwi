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
 * @brief               IPC API.
 */

#include <core/ipc.h>
#include <core/list.h>
#include <core/log.h>
#include <core/time.h>

#include <kernel/status.h>

#include <stdlib.h>
#include <string.h>

#include "libsystem.h"

struct core_connection {
    handle_t handle;
    uint32_t flags;

    /** Next serial number for a request on this connection. */
    uint64_t next_serial;

    /**
     * Queue of messages which have been received but were not what we were
     * expecting right now. They will be returned later when possible. This is
     * used to handle messages coming out of order when performing synchronous
     * operations (e.g. while we're waiting for a reply to a request, we could
     * receive a signal).
     */
     core_list_t receive_queue;
};

struct core_message {
    core_list_t link;                       /**< Link to connection message queue. */
    ipc_message_t message;                  /**< Wrapped kernel message structure. */
    uint32_t flags;                         /**< Message flags. */
    handle_t handle;                        /**< Attached handle. */
};

/** Bit offset where the type is stored in the kernel message ID. */
#define CORE_MESSAGE_ID_TYPE_SHIFT      28
#define CORE_MESSAGE_ID_TYPE_MASK       0xf0000000u

/** Indices in the kernel message arguments array. */
enum {
    CORE_MESSAGE_ARG_SERIAL     = 0,
    CORE_MESSAGE_ARG_TOTAL_SIZE = 1,
    CORE_MESSAGE_ARG_FIRST_DATA = 2,
};

/** Total amount of data that can be sent inline in the kernel message. */
#define CORE_MESSAGE_INLINE_DATA_MAX    ((IPC_MESSAGE_ARGS_COUNT - CORE_MESSAGE_ARG_FIRST_DATA) * sizeof(uint64_t))

/** Internal message flags. */
enum {
    /** Select only user-facing flags. */
    CORE_MESSAGE_USER_FLAGS_MASK    = (1<<16) - 1,

    /** Message has a security context attached to it. */
    CORE_MESSAGE_HAS_SECURITY       = (1<<16),

    /** Message owns the attached handle. */
    CORE_MESSAGE_OWNS_HANDLE        = (1<<17),
};

/** Default timeout for sending signals/replies. TODO: Make this configurable. */
#define SEND_TIMEOUT    core_secs_to_nsecs(5)

static size_t calc_message_alloc_size(size_t size, uint32_t flags) {
    /* Whole message is allocated in one chunk. */
    size_t alloc_size = sizeof(core_message_t);

    if (flags & CORE_MESSAGE_HAS_SECURITY)
        alloc_size += sizeof(security_context_t);

    /* Data is inlined into the base message if small enough. */
    size_t payload_size = (size > CORE_MESSAGE_INLINE_DATA_MAX) ? size : 0;
    alloc_size += payload_size;

    return alloc_size;
}

static core_message_t *create_message(core_message_type_t type, uint32_t id, size_t size, uint32_t flags) {
    libsystem_assert((id & CORE_MESSAGE_ID_TYPE_MASK) == 0);

    size_t alloc_size = calc_message_alloc_size(size, flags);

    /* Message allocation is zeroed to avoid inadvertently leaking data from
     * our address space to the other side if not everything is initialised. */
    core_message_t *message = calloc(1, alloc_size);
    if (!message)
        return NULL;

    core_list_init(&message->link);

    message->message.id                                = ((uint32_t)type << CORE_MESSAGE_ID_TYPE_SHIFT) | id;
    message->message.size                              = (size > CORE_MESSAGE_INLINE_DATA_MAX) ? size : 0;
    message->message.args[CORE_MESSAGE_ARG_TOTAL_SIZE] = size;
    message->flags                                     = flags;
    message->handle                                    = INVALID_HANDLE;

    if (flags & CORE_MESSAGE_SEND_SECURITY)
        message->message.flags |= IPC_MESSAGE_SECURITY;

    return message;
}

static inline bool is_data_inline(const core_message_t *message) {
    return message->message.args[CORE_MESSAGE_ARG_TOTAL_SIZE] <= CORE_MESSAGE_INLINE_DATA_MAX;
}

/**
 * Create a new connection object from an existing connection handle. If
 * successful, this will take ownership of the handle (i.e. calling
 * core_connection_close() on the resulting connection object will close the
 * handle).
 *
 * There should not have been any previous communication on the connection -
 * if there has this would cause inconsistency in message serial numbers.
 *
 * @param handle        Connection handle.
 * @param flags         Flags for the connection (CORE_CONNECTION_*).
 *
 * @return              Created connection object on success.
 *                      NULL if memory allocation fails.
 */
core_connection_t *core_connection_create(handle_t handle, uint32_t flags) {
    core_connection_t *conn = malloc(sizeof(core_connection_t));
    if (!conn)
        return NULL;

    core_list_init(&conn->receive_queue);

    conn->handle      = handle;
    conn->flags       = flags;
    conn->next_serial = 0;

    return conn;
}

/**
 * Create a new connection object by connecting to a port.
 *
 * @param port          Port handle to connect to.
 * @param timeout       Timeout in nanoseconds (see kern_connection_open()).
 * @param flags         Flags for the connection (CORE_CONNECTION_*).
 * @param _conn         Where to return connection object upon success.
 *
 * @return              STATUS_SUCCESS if connection is successfully opened.
 *                      STATUS_NO_MEMORY if memory allocation fails.
 *                      Any possible status code from kern_connection_open().
 */
status_t core_connection_open(handle_t port, nstime_t timeout, uint32_t flags, core_connection_t **_conn) {
    core_connection_t *conn = malloc(sizeof(core_connection_t));
    if (!conn)
        return STATUS_NO_MEMORY;

    core_list_init(&conn->receive_queue);

    conn->flags       = flags;
    conn->next_serial = 0;

    status_t ret = kern_connection_open(port, timeout, &conn->handle);
    if (ret != STATUS_SUCCESS) {
        free(conn);
        return ret;
    }

    *_conn = conn;
    return STATUS_SUCCESS;
}

/** Close a connection object.
 * @param conn          Connection object. */
void core_connection_close(core_connection_t *conn) {
    libsystem_assert(conn);

    kern_handle_close(conn->handle);
    core_connection_destroy(conn);
}

/**
 * Destroy a connection object whose underlying handle is already closed (e.g.
 * after forking, since connections are not inherited across a fork).
 *
 * @param conn          Connection object.
 */
void core_connection_destroy(core_connection_t *conn) {
    libsystem_assert(conn);

    while (!core_list_empty(&conn->receive_queue)) {
        core_message_t *message = core_list_first(&conn->receive_queue, core_message_t, link);
        core_list_remove(&message->link);
        core_message_destroy(message);
    }

    free(conn);
}

/**
 * Get the underlying connection handle for a connection object. This can be
 * used to wait for events on the handle - when an event is fired, call
 * core_connection_receive() to process it. This handle should not be used to
 * send/receive messages directly.
 *
 * @param conn          Connection object.
 *
 * @return              Underlying connection handle.
 */
handle_t core_connection_handle(const core_connection_t *conn) {
    libsystem_assert(conn);

    return conn->handle;
}

/**
 * Send a signal over a connection. This is sent asynchronously - there is no
 * acknowledgement that the other side has received and processed the signal.
 * This function may block if the remote message queue is full, and will time
 * out if it fails to send within a set time.
 *
 * @param conn          Connection object.
 * @param signal        Message to send (must be created with
 *                      core_message_create_signal()).
 *
 * @return              STATUS_SUCCESS if message sent successfully.
 *                      Any possible status code from kern_connection_send().
 */
status_t core_connection_signal(core_connection_t *conn, core_message_t *signal) {
    libsystem_assert(conn);
    libsystem_assert(signal);
    libsystem_assert(core_message_type(signal) == CORE_MESSAGE_SIGNAL);

    void *data = (is_data_inline(signal)) ? NULL : core_message_data(signal);

    return kern_connection_send(conn->handle, &signal->message, data, signal->handle, SEND_TIMEOUT);
}

/**
 * Receive the next message from the connection. Some validation is performed
 * on the message: if it is malformed, it will be dropped by returning
 * STATUS_SUCCESS but a NULL message pointer. Additionally, if the message is
 * not one the connection wants to handle, it will also be dropped.
 */
static status_t receive_message(core_connection_t *conn, nstime_t timeout, core_message_t **_message) {
    status_t ret;

    *_message = NULL;

    /* We have to retrieve the base message to determine the allocation size,
     * then copy it into allocated message structure. */
    ipc_message_t kmessage;
    security_context_t security;
    ret = kern_connection_receive(
        conn->handle,
        &kmessage,
        (conn->flags & CORE_CONNECTION_RECEIVE_SECURITY) ? &security : NULL,
        timeout);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Check if this is a message that we care about, drop it if not. */
    core_message_type_t type = kmessage.id >> CORE_MESSAGE_ID_TYPE_SHIFT;
    if ((type == CORE_MESSAGE_REQUEST && !(conn->flags & CORE_CONNECTION_RECEIVE_REQUESTS)) ||
        (type == CORE_MESSAGE_SIGNAL  && !(conn->flags & CORE_CONNECTION_RECEIVE_SIGNALS)))
    {
        return STATUS_SUCCESS;
    }

    uint32_t flags = 0;

    if (conn->flags & CORE_CONNECTION_RECEIVE_SECURITY && kmessage.flags & IPC_MESSAGE_SECURITY)
        flags |= CORE_MESSAGE_HAS_SECURITY;

    size_t total_size = kmessage.args[CORE_MESSAGE_ARG_TOTAL_SIZE];

    /* Prevent a malicious client from causing us to overallocate. */
    if (total_size > IPC_DATA_MAX)
        return STATUS_SUCCESS;

    size_t alloc_size = calc_message_alloc_size(total_size, flags);

    /* No need to zero, we'll write over the whole thing. */
    core_message_t *message = malloc(alloc_size);
    if (!message)
        return STATUS_NO_MEMORY;

    core_list_init(&message->link);

    memcpy(&message->message, &kmessage, sizeof(kmessage));
    message->flags = flags;

    if (flags & CORE_MESSAGE_HAS_SECURITY)
        memcpy((void *)core_message_security(message), &security, sizeof(security));

    /* Check for consistency between user-supplied total size and kernel-
     * reported size. */
    if (message->message.size != ((is_data_inline(message)) ? 0 : total_size)) {
        free(message);
        return STATUS_SUCCESS;
    }

    if (!is_data_inline(message)) {
        ret = kern_connection_receive_data(conn->handle, core_message_data(message));
        if (ret != STATUS_SUCCESS) {
            free(message);
            return ret;
        }
    }

    if (message->message.flags & IPC_MESSAGE_HANDLE) {
        ret = kern_connection_receive_handle(conn->handle, &message->handle);
        if (ret != STATUS_SUCCESS) {
            free(message);
            return ret;
        }

        message->flags |= CORE_MESSAGE_OWNS_HANDLE;
    } else {
        message->handle = INVALID_HANDLE;
    }

    *_message = message;
    return STATUS_SUCCESS;
}

/**
 * Send a request over a connection and wait for a reply. This is a synchronous
 * operation which will not return until a reply has been received. However, if
 * the remote message queue is full and we fail to send the initial request
 * within a set time, then this function will fail.
 *
 * @param conn          Connection object.
 * @param request       Message to send (must be created with
 *                      core_message_create_request()).
 * @param _reply        Where to store reply message. This must be destroyed
 *                      with core_message_destroy() once no longer needed.
 *
 * @return              STATUS_SUCCESS if message sent successfully.
 *                      Any possible status code from kern_connection_send(),
 *                      kern_connection_receive(), kern_connection_receive_data()
 *                      or kern_connection_receive_handle().
 */
status_t core_connection_request(core_connection_t *conn, core_message_t *request, core_message_t **_reply) {
    libsystem_assert(conn);
    libsystem_assert(request);
    libsystem_assert(core_message_type(request) == CORE_MESSAGE_REQUEST);
    libsystem_assert(_reply);

    /* Set the serial so we can match reply to request. */
    uint64_t request_serial = conn->next_serial++;
    request->message.args[CORE_MESSAGE_ARG_SERIAL] = request_serial;

    void *data = (is_data_inline(request)) ? NULL : core_message_data(request);

    status_t ret = kern_connection_send(conn->handle, &request->message, data, request->handle, SEND_TIMEOUT);
    if (ret != STATUS_SUCCESS)
        return ret;

    core_message_t *message = NULL;

    /* We might not receive a reply to this request immediately after sending
     * it, other messages can be received in between, so we have to loop and
     * wait for the right reply. */
    while (!message) {
        /* It isn't necessary to check the receive queue here - no other threads
         * should be using the connection simultaneously so all messages
         * received while waiting for the reply should be handled in this loop. */
        ret = receive_message(conn, -1, &message);
        if (ret != STATUS_SUCCESS)
            return ret;

        /* NULL if we get a malformed message or one we don't care about. */
        if (message) {
            if (core_message_type(message) != CORE_MESSAGE_REPLY ||
                message->message.args[CORE_MESSAGE_ARG_SERIAL] != request_serial)
            {
                /* Not the reply, add to the receive queue to process later. */
                core_list_append(&conn->receive_queue, &message->link);
                message = NULL;
            }
        }
    }

    *_reply = message;
    return STATUS_SUCCESS;
}

/**
 * Reply to a previously received request message. This is sent asynchronously -
 * there is no acknowledgement that the other side has received and processed
 * the reply. This function may block if the remote message queue is full, and
 * will time out if it fails to send within a set time.
 *
 * @param conn          Connection object.
 * @param reply         Message to send (must be created with
 *                      core_message_create_reply() using the message that is
 *                      being replied to).
 *
 * @return              STATUS_SUCCESS if message sent successfully.
 *                      Any possible status code from kern_connection_send().
 */
status_t core_connection_reply(core_connection_t *conn, core_message_t *reply) {
    libsystem_assert(conn);
    libsystem_assert(reply);
    libsystem_assert(core_message_type(reply) == CORE_MESSAGE_REPLY);

    void *data = (is_data_inline(reply)) ? NULL : core_message_data(reply);

    return kern_connection_send(conn->handle, &reply->message, data, reply->handle, SEND_TIMEOUT);
}

/**
 * Receive a message from the connection. At least one type of message must be
 * enabled via CORE_CONNECTION_RECEIVE_{REQUESTS,SIGNALS} flags, and if any
 * message type is received that is not enabled then it will be discarded.
 *
 * @param conn          Connection object.
 * @param timeout       Timeout in nanoseconds (see kern_connection_receive()).
 * @param _message      Where to store received message. This must be destroyed
 *                      with core_message_destroy() once no longer needed.
 *
 * @return              STATUS_SUCCESS if message received successfully.
 *                      Any possible status code from kern_connection_receive().
 */
status_t core_connection_receive(core_connection_t *conn, nstime_t timeout, core_message_t **_message) {
    libsystem_assert(conn);
    libsystem_assert(_message);

    core_message_t *message = NULL;

    /* Return queued messages in the order they came in. */
    if (!core_list_empty(&conn->receive_queue)) {
        message = core_list_first(&conn->receive_queue, core_message_t, link);
        core_list_remove(&message->link);
    } else {
        /* This can return NULL if we get a malformed message or one we don't
         * care about. Therefore we have to loop. TODO: Timeouts other than
         * 0/-1 are not handled properly. */
        while (!message) {
            libsystem_assert(timeout <= 0);
            status_t ret = receive_message(conn, timeout, &message);
            if (ret != STATUS_SUCCESS)
                return ret;
        }
    }

    *_message = message;
    return STATUS_SUCCESS;
}

/**
 * Create a signal message.
 *
 * @param id            Message ID.
 * @param size          Size of message data.
 * @param flags         Message flags.
 *
 * @return              Allocated message, destroy with core_message_destroy()
 *                      once no longer needed.
 *                      NULL on failure.
 */
core_message_t *core_message_create_signal(uint32_t id, size_t size, uint32_t flags) {
    return create_message(CORE_MESSAGE_SIGNAL, id, size, flags & CORE_MESSAGE_USER_FLAGS_MASK);
}

/**
 * Create a request message.
 *
 * @param id            Message ID.
 * @param size          Size of message data.
 * @param flags         Message flags.
 *
 * @return              Allocated message, destroy with core_message_destroy()
 *                      once no longer needed.
 *                      NULL on failure.
 */
core_message_t *core_message_create_request(uint32_t id, size_t size, uint32_t flags) {
    return create_message(CORE_MESSAGE_REQUEST, id, size, flags & CORE_MESSAGE_USER_FLAGS_MASK);
}

/**
 * Create a reply message.
 *
 * @param request       Request that this is a reply to.
 * @param size          Size of message data.
 * @param flags         Message flags.
 *
 * @return              Allocated message, destroy with core_message_destroy()
 *                      once no longer needed.
 *                      NULL on failure.
 */
core_message_t *core_message_create_reply(const core_message_t *request, size_t size, uint32_t flags) {
    libsystem_assert(request);

    uint32_t id = core_message_id(request);

    core_message_t *message = create_message(CORE_MESSAGE_REPLY, id, size, flags & CORE_MESSAGE_USER_FLAGS_MASK);
    if (!message)
        return NULL;

    /* Use the serial of the original request. */
    message->message.args[CORE_MESSAGE_ARG_SERIAL] = request->message.args[CORE_MESSAGE_ARG_SERIAL];

    return message;
}

/** Destroy a message object.
 * @param message       Message object. */
void core_message_destroy(core_message_t *message) {
    libsystem_assert(message);

    if (message->handle != INVALID_HANDLE && message->flags & CORE_MESSAGE_OWNS_HANDLE)
        kern_handle_close(message->handle);

    free(message);
}

/** Get the type of a message.
 * @param message       Message object.
 * @return              Type of the message. */
core_message_type_t core_message_type(const core_message_t *message) {
    libsystem_assert(message);

    return message->message.id >> CORE_MESSAGE_ID_TYPE_SHIFT;
}

/** Get the ID of a message.
 * @param message       Message object.
 * @return              ID of the message. */
uint32_t core_message_id(const core_message_t *message) {
    libsystem_assert(message);

    return message->message.id & ~CORE_MESSAGE_ID_TYPE_MASK;
}

/** Get the data size of a message.
 * @param message       Message object.
 * @return              Data size of the message. */
size_t core_message_size(const core_message_t *message) {
    libsystem_assert(message);

    return message->message.args[CORE_MESSAGE_ARG_TOTAL_SIZE];
}

/**
 * Get the timestamp at which a message was sent. This is only valid for
 * messages returned by core_connection_receive().
 *
 * @param message       Message object.
 *
 * @return              Timestamp of the message.
 */
nstime_t core_message_timestamp(const core_message_t *message) {
    libsystem_assert(message);

    return message->message.timestamp;
}

/**
 * Get the security context of a message's sender. This is only available for
 * messages returned by core_connection_receive() on a connection which has
 * CORE_CONNECTION_RECEIVE_SECURITY enabled, and for which the sender attached
 * a security context.
 *
 * @param message       Message object.
 *
 * @return              Security context of the message.
 *                      NULL if the message has no security context attached.
 */
const security_context_t *core_message_security(const core_message_t *message) {
    libsystem_assert(message);

    return (message->flags & CORE_MESSAGE_HAS_SECURITY)
        ? (const security_context_t *)((const char *)message + sizeof(core_message_t))
        : NULL;
}

/** Get the data pointer for a message.
 * @param message       Message object.
 * @return              Data pointer for the message. */
void *core_message_data(core_message_t *message) {
    libsystem_assert(message);

    void *data;

    if (message->message.args[CORE_MESSAGE_ARG_TOTAL_SIZE] == 0) {
        data = NULL;
    } else if (is_data_inline(message)) {
        data = &message->message.args[CORE_MESSAGE_ARG_FIRST_DATA];
    } else {
        size_t offset = sizeof(core_message_t);

        if (message->flags & CORE_MESSAGE_HAS_SECURITY)
            offset += sizeof(security_context_t);

        data = (char *)message + offset;
    }

    return data;
}

/**
 * Attach a handle to a message. Optionally, this can take over ownership of the
 * handle, such that it will be closed once the message is destroyed. If the
 * message already has an attached handle, it will be replaced (and closed if
 * owned by the message).
 *
 * @param message       Message object.
 * @param handle        Handle to attach.
 * @param own           Whether the message should take ownership of the handle.
 */
void core_message_attach_handle(core_message_t *message, handle_t handle, bool own) {
    libsystem_assert(message);

    if (message->handle != INVALID_HANDLE && message->flags & CORE_MESSAGE_OWNS_HANDLE)
        kern_handle_close(message->handle);

    message->handle = handle;

    if (handle != INVALID_HANDLE) {
        message->message.flags |= IPC_MESSAGE_HANDLE;
    } else {
        message->message.flags &= ~IPC_MESSAGE_HANDLE;
    }

    if (own) {
        message->flags |= CORE_MESSAGE_OWNS_HANDLE;
    } else {
        message->flags &= ~CORE_MESSAGE_OWNS_HANDLE;
    }
}

/**
 * Get the handle attached to a message, if any. This will release ownership of
 * the handle from the message, after which it will be the responsibility of the
 * caller to close the handle once it is no longer needed.
 *
 * @param message       Message object.
 *
 * @return              Handle attached to the message.
 *                      INVALID_HANDLE if the message has no handle attached.
 */
handle_t core_message_detach_handle(core_message_t *message) {
    libsystem_assert(message);

    handle_t handle = message->handle;

    message->handle = INVALID_HANDLE;
    message->flags &= ~CORE_MESSAGE_OWNS_HANDLE;

    return handle;
}
