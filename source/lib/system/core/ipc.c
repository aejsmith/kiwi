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
 * @brief               IPC API.
 */

#include <core/ipc.h>

#include <kernel/status.h>

struct core_connection {
    handle_t handle;
    uint32_t flags;
};

struct core_message {
    ipc_message_t message;                  /**< Wrapped kernel message structure. */
    uint32_t flags;                         /**< Message flags. */
};

/** Bit offset where the type is stored in the kernel message ID. */
#define CORE_MESSAGE_ID_TYPE_SHIFT      28

/** Indices in the kernel message arguments array. */
enum {
    CORE_MESSAGE_ARG_SERIAL     = 0,
    CORE_MESSAGE_ARG_TOTAL_SIZE = 1,
    CORE_MESSAGE_ARG_FIRST_DATA = 2,
};

/** Total amount of data that can be sent inline in the kernel message. */
#define CORE_MESSAGE_INLINE_DATA_MAX    ((IPC_MESSAGE_ARGS_COUNT - CORE_MESSAGE_ARG_FIRST_DATA) * sizeof(uint64_t))

/** Message flags. */
enum {
    /** Message has a security context attached to it. */
    CORE_MESSAGE_SECURITY    = (1<<0),

    /** Message owns the attached handle. */
    CORE_MESSAGE_OWNS_HANDLE = (1<<1),
};

/**
 * Create a new connection object from an existing connection handle. If
 * successful, this will take ownership of the handle (i.e. calling
 * core_connection_close() on the resulting connection object will close the
 * handle).
 *
 * @param conn          Connection handle.
 * @param flags         Flags for the connection (CORE_CONNECTION_*).
 *
 * @return              Created connection object on success.
 *                      NULL if memory allocation fails.
 */
core_connection_t *core_connection_create(handle_t conn, uint32_t flags) {
    return NULL;
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
    return STATUS_NOT_IMPLEMENTED;
}

/** Close a connection object.
 * @param conn          Connection object. */
void core_connection_close(core_connection_t *conn) {

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
handle_t core_connection_get_handle(core_connection_t *conn) {
    return INVALID_HANDLE;
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
status_t core_connection_signal(core_connection_t *conn, const core_message_t *signal) {
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Send a request over a connection and wait for a reply. This is a synchronous
 * operation which will not return until a reply has been received.
 *
 * @param conn          Connection object.
 * @param request       Message to send (must be created with
 *                      core_message_create_request()).
 * @param _reply        Where to store reply message. This must be destroyed
 *                      with core_message_destroy() once no longer needed.
 *
 * @return              STATUS_SUCCESS if message sent successfully.
 *                      Any possible status code from kern_connection_send().
 */
status_t core_connection_request(core_connection_t *conn, const core_message_t *request, core_message_t **_reply) {
    return STATUS_NOT_IMPLEMENTED;
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
status_t core_connection_reply(core_connection_t *conn, const core_message_t *reply) {
    return STATUS_NOT_IMPLEMENTED;
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
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Create a signal message.
 *
 * @param id            Message ID.
 * @param size          Size of message data.
 *
 * @return              Allocated message. Destroy with core_message_destroy()
 *                      once no longer needed.
 */
core_message_t *core_message_create_signal(uint32_t id, size_t size) {
    return NULL;
}

/**
 * Create a request message.
 *
 * @param id            Message ID.
 * @param size          Size of message data.
 *
 * @return              Allocated message. Destroy with core_message_destroy()
 *                      once no longer needed.
 */
core_message_t *core_message_create_request(uint32_t id, size_t size) {
    return NULL;
}

/**
 * Create a reply message.
 *
 * @param request       Request that this is a reply to.
 * @param size          Size of message data.
 *
 * @return              Allocated message. Destroy with core_message_destroy()
 *                      once no longer needed.
 */
core_message_t *core_message_create_reply(const core_message_t *request, size_t size) {
    return NULL;
}

/** Destroy a message object.
 * @param message       Message object. */
void core_message_destroy(core_message_t *message) {

}

/** Get the type of a message.
 * @param message       Message object.
 * @return              Type of the message. */
core_message_type_t core_message_get_type(const core_message_t *message) {
    return 0;
}

/** Get the ID of a message.
 * @param message       Message object.
 * @return              ID of the message. */
uint32_t core_message_get_id(const core_message_t *message) {
    return 0;
}

/** Get the data size of a message.
 * @param message       Message object.
 * @return              Data size of the message. */
size_t core_message_get_size(const core_message_t *message) {
    return 0;
}

/**
 * Get the timestamp at which a message was sent. This is only valid for
 * messages returned by core_connection_receive().
 *
 * @param message       Message object.
 *
 * @return              Timestamp of the message.
 */
nstime_t core_message_get_timestamp(const core_message_t *message) {
    return 0;
}

/**
 * Get the security context of a message's sender. This is only valid for
 * messages returned by core_connection_receive() on a connection which has
 * CORE_CONNECTION_RECEIVE_SECURITY enabled.
 *
 * @param message       Message object.
 *
 * @return              Security context of the message.
 *                      NULL if the message has no security context information.
 */
security_context_t *core_message_get_security(const core_message_t *message) {
    return NULL;
}

/** Get the data pointer for a message.
 * @param message       Message object.
 * @return              Data pointer for the message. */
void *core_message_get_data(core_message_t *message) {
    return NULL;
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
    return INVALID_HANDLE;
}
