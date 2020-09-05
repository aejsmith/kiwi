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
 *
 * This is a userspace extension of the raw kernel IPC interface, adding some
 * extra functionality that the kernel need not concern itself with.
 *
 * From the kernel's point of view a connection is a basic bidirectional
 * communication channel with no explicit relation between messages sent in
 * each direction. This API adds some additional structure to this, relating
 * the messages sent in each direction.
 *
 * Messages are given a type:
 *  - CORE_MESSAGE_REQUEST: Used for making a request from a client to a
 *    service. A reply is expected.
 *  - CORE_MESSAGE_REPLY: Reply to an earlier request from the client.
 *  - CORE_MESSAGE_SIGNAL: A notification to the other side of the connection
 *    unrelated to other messages. No reply is needed.
 *
 * Note that the type of a message is transmitted in the top 4 bits of the ID
 * field of the underlying kernel message, therefore users of this API are
 * restricted to 28-bit message IDs.
 *
 * To match up requests with replies (rather than having to force all request/
 * reply pairs to be in order), messages use serial numbers. Each side of the
 * connection keeps a monotonically increasing serial number for each request
 * it sends. Replies are sent back with the same serial number that the request
 * came in with, allowing the receiver to match reply to request based on the
 * serial numbers.
 *
 * Note that connection and message objects are not thread-safe, users should
 * ensure that they do not access them from multiple threads simultaneously.
 *
 * TODO:
 *  - This is still just a draft/work-in-progress interface and very likely to
 *    change.
 *  - Asynchronous request API.
 *  - Timeout support for message sending functions.
 *  - Add a send queue that allows sending messages to be delayed until there
 *    is space in the kernel message queue, driven by object events. This would
 *    be used to avoid potential denial of service issues where a client could
 *    hang a service by not processing messages.
 *  - Is there any use case for allowing multithreaded use of connections? E.g.
 *    multiple threads sending requests to a service.
 */

#ifndef __CORE_IPC_H
#define __CORE_IPC_H

#include <kernel/ipc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Connection object (opaque). */
typedef struct core_connection core_connection_t;

/** Connection flags. */
enum {
    /**
     * The connection wants to handle requests. If not set, any received request
     * messages will be discarded (should be set for client-only connections).
     * If set, core_connection_receive() must be called periodically to handle
     * any incoming messages, as without doing so, other connection operations
     * may cause received messages to infinitely queue up internally.
     */
    CORE_CONNECTION_RECEIVE_REQUESTS = (1<<0),

    /**
     * The connection wants to handle signals. If not set, any received signal
     * messages will be discarded. If set, core_connection_receive() must be
     * called periodically to handle any incoming messages, as without doing so,
     * other connection operations may cause received messages to infinitely
     * queue up internally.
     */
    CORE_CONNECTION_RECEIVE_SIGNALS = (1<<1),

    /**
     * Receive security credentials in messages on this connection. This can be
     * omitted when not needed to reduce some CPU/allocation overhead.
     */
    CORE_CONNECTION_RECEIVE_SECURITY = (1<<2),
};

/** Message object (opaque). */
typedef struct core_message core_message_t;

/** Message types. */
typedef enum core_message_type {
    CORE_MESSAGE_SIGNAL  = 0,               /**< Standalone notification to the other side. */
    CORE_MESSAGE_REQUEST = 1,               /**< Request (expects a reply). */
    CORE_MESSAGE_REPLY   = 2,               /**< Reply to a previous request from the other side. */
} core_message_type_t;

extern core_connection_t *core_connection_create(handle_t handle, uint32_t flags);
extern status_t core_connection_open(handle_t port, nstime_t timeout, uint32_t flags, core_connection_t **_conn);
extern void core_connection_close(core_connection_t *conn);

extern handle_t core_connection_get_handle(core_connection_t *conn);

extern status_t core_connection_signal(core_connection_t *conn, core_message_t *signal);
extern status_t core_connection_request(core_connection_t *conn, core_message_t *request, core_message_t **_reply);
extern status_t core_connection_reply(core_connection_t *conn, core_message_t *reply);
extern status_t core_connection_receive(core_connection_t *conn, nstime_t timeout, core_message_t **_message);

extern core_message_t *core_message_create_signal(uint32_t id, size_t size);
extern core_message_t *core_message_create_request(uint32_t id, size_t size);
extern core_message_t *core_message_create_reply(const core_message_t *request, size_t size);
extern void core_message_destroy(core_message_t *message);

extern core_message_type_t core_message_get_type(const core_message_t *message);
extern uint32_t core_message_get_id(const core_message_t *message);
extern size_t core_message_get_size(const core_message_t *message);
extern nstime_t core_message_get_timestamp(const core_message_t *message);
extern const security_context_t *core_message_get_security(const core_message_t *message);
extern void *core_message_get_data(core_message_t *message);

// need to distinguish owned vs non-owned handle (don't want to close handles that were attached for a sent message, probably)
extern void core_message_attach_handle(core_message_t *message, handle_t handle, bool own);
extern handle_t core_message_detach_handle(core_message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* __CORE_IPC_H */
