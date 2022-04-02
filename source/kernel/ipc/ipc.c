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
 * @brief               IPC interface.
 */

#include <ipc/ipc.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <security/security.h>

#include <assert.h>
#include <kdb.h>
#include <status.h>

/** Define to enable debug output. */
//#define DEBUG_IPC

#ifdef DEBUG_IPC
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Caches for IPC structures. */
static slab_cache_t *ipc_port_cache;
static slab_cache_t *ipc_connection_cache;
static slab_cache_t *ipc_kmessage_cache;

/** Constructor for port objects. */
static void ipc_port_ctor(void *obj, void *data) {
    ipc_port_t *port = obj;

    mutex_init(&port->lock, "ipc_port_lock", 0);
    list_init(&port->waiting);
    condvar_init(&port->listen_cvar, "ipc_port_listen");
    notifier_init(&port->connection_notifier, port);
}

/** Close a handle to a port. */
static void port_object_close(object_handle_t *handle) {
    ipc_port_release(handle->private);
}

/** Called when a port handle is attached to a process. */
static void port_object_attach(object_handle_t *handle, process_t *process) {
    ipc_port_t *port = handle->private;

    mutex_lock(&port->lock);

    if (process == port->owner)
        port->owner_count++;

    mutex_unlock(&port->lock);
}

/** Called when a port handle is detached from a process. */
static void port_object_detach(object_handle_t *handle, process_t *process) {
    ipc_port_t *port = handle->private;

    mutex_lock(&port->lock);

    if (process == port->owner && --port->owner_count == 0) {
        port->owner = NULL;

        /* Cancel all in-progress connection attempts. */
        list_foreach_safe(&port->waiting, iter) {
            ipc_connection_t *conn = list_entry(iter, ipc_connection_t, header);

            mutex_lock(&conn->lock);

            list_remove(&conn->header);
            conn->state = IPC_CONNECTION_CLOSED;
            condvar_broadcast(&conn->open_cvar);

            mutex_unlock(&conn->lock);
        }

        dprintf("ipc: process %" PRId32 " disowned port %p\n", process->id, port);
    }

    mutex_unlock(&port->lock);
}

/** Signal that a port event is being waited for. */
static status_t port_object_wait(object_handle_t *handle, object_event_t *event) {
    ipc_port_t *port = handle->private;
    status_t ret;

    mutex_lock(&port->lock);

    switch (event->event) {
        case PORT_EVENT_CONNECTION:
            if (curr_proc != port->owner) {
                mutex_unlock(&port->lock);
                return STATUS_ACCESS_DENIED;
            }

            if (!list_empty(&port->waiting)) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&port->connection_notifier, object_event_notifier, event);
            }

            ret = STATUS_SUCCESS;
            break;
        default:
            ret = STATUS_INVALID_EVENT;
            break;
    }

    mutex_unlock(&port->lock);
    return ret;
}

/** Stop waiting for a port event. */
static void port_object_unwait(object_handle_t *handle, object_event_t *event) {
    ipc_port_t *port = handle->private;

    switch (event->event) {
        case PORT_EVENT_CONNECTION:
            notifier_unregister(&port->connection_notifier, object_event_notifier, event);
            break;
    }
}

/** Port object type. */
static const object_type_t port_object_type = {
    .id     = OBJECT_TYPE_PORT,
    .flags  = OBJECT_TRANSFERRABLE,
    .close  = port_object_close,
    .attach = port_object_attach,
    .detach = port_object_detach,
    .wait   = port_object_wait,
    .unwait = port_object_unwait,
};

/** Constructor for connection objects. */
static void ipc_connection_ctor(void *obj, void *data) {
    ipc_connection_t *conn = obj;

    mutex_init(&conn->lock, "ipc_connection_lock", 0);
    condvar_init(&conn->open_cvar, "ipc_connection_open");
    list_init(&conn->header);

    conn->endpoints[0].remote = &conn->endpoints[1];
    conn->endpoints[1].remote = &conn->endpoints[0];

    for (size_t i = 0; i < 2; i++) {
        ipc_endpoint_t *endpoint = &conn->endpoints[i];

        endpoint->conn          = conn;
        endpoint->message_count = 0;
        endpoint->pending       = NULL;

        list_init(&endpoint->messages);
        condvar_init(&endpoint->space_cvar, "ipc_connection_send");
        condvar_init(&endpoint->data_cvar, "ipc_connection_receive");
        notifier_init(&endpoint->hangup_notifier, endpoint);
        notifier_init(&endpoint->message_notifier, endpoint);
    }
}

/** Release an IPC connection. */
static void ipc_connection_release(ipc_connection_t *conn) {
    assert(conn->state == IPC_CONNECTION_CLOSED);

    if (refcount_dec(&conn->count) > 0)
        return;

    dprintf("ipc: destroying connection %p\n", conn);

    /* Message queues should be emptied by ipc_connection_close(). */
    assert(list_empty(&conn->endpoints[0].messages));
    assert(!conn->endpoints[0].pending);
    assert(list_empty(&conn->endpoints[1].messages));
    assert(!conn->endpoints[1].pending);

    slab_cache_free(ipc_connection_cache, conn);
}

/** Close a handle to a connection. */
static void connection_object_close(object_handle_t *handle) {
    ipc_connection_close(handle->private);
}

/** Signal that a connection event is being waited for. */
static status_t connection_object_wait(object_handle_t *handle, object_event_t *event) {
    ipc_endpoint_t *endpoint = handle->private;
    status_t ret;

    mutex_lock(&endpoint->conn->lock);

    switch (event->event) {
        case CONNECTION_EVENT_HANGUP:
            if (!(event->flags & OBJECT_EVENT_EDGE) && endpoint->conn->state == IPC_CONNECTION_CLOSED) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&endpoint->hangup_notifier, object_event_notifier, event);
            }

            ret = STATUS_SUCCESS;
            break;
        case CONNECTION_EVENT_MESSAGE:
            if (!(event->flags & OBJECT_EVENT_EDGE) && endpoint->message_count) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&endpoint->message_notifier, object_event_notifier, event);
            }

            ret = STATUS_SUCCESS;
            break;
        default:
            ret = STATUS_INVALID_EVENT;
            break;
    }

    mutex_unlock(&endpoint->conn->lock);
    return ret;
}

/** Stop waiting for a connection event. */
static void connection_object_unwait(object_handle_t *handle, object_event_t *event) {
    ipc_endpoint_t *endpoint = handle->private;

    switch (event->event) {
        case CONNECTION_EVENT_HANGUP:
            notifier_unregister(&endpoint->hangup_notifier, object_event_notifier, event);
            break;
        case CONNECTION_EVENT_MESSAGE:
            notifier_unregister(&endpoint->message_notifier, object_event_notifier, event);
            break;
    }
}

/** Connection object type. */
static const object_type_t connection_object_type = {
    .id     = OBJECT_TYPE_CONNECTION,
    .close  = connection_object_close,
    .wait   = connection_object_wait,
    .unwait = connection_object_unwait,
};

/** Receive a message on an endpoint.
 * @param conn          Connection being received on (must be locked).
 * @param endpoint      Endpoint to receive from.
 * @param flags         Behaviour flags.
 * @param timeout       Timeout in nanoseconds.
 * @param _msg          Where to store pointer to received message.
 * @return              Status code describing result of the operation. */
static status_t receive_message(
    ipc_connection_t *conn, ipc_endpoint_t *endpoint, unsigned flags,
    nstime_t timeout, ipc_kmessage_t **_msg)
{
    assert(conn->state != IPC_CONNECTION_SETUP);
    assert(!(endpoint->flags & IPC_ENDPOINT_DROP));

    /* Clear any pending data left at our endpoint. */
    if (endpoint->pending) {
        ipc_kmessage_release(endpoint->pending);
        endpoint->pending = NULL;
    }

    /* If the connection is closed we should still return queued messages
     * until there is nothing left to receive, at which point we return an
     * error. */
    if (!endpoint->message_count && conn->state == IPC_CONNECTION_CLOSED)
        return STATUS_CONN_HUNGUP;

    /* Wait for a message to arrive. */
    nstime_t absolute = (timeout > 0) ? system_time() + timeout : timeout;
    unsigned sleep    = SLEEP_ABSOLUTE;

    if (flags & IPC_INTERRUPTIBLE)
        sleep |= SLEEP_INTERRUPTIBLE;

    while (!endpoint->message_count) {
        status_t ret = condvar_wait_etc(&endpoint->data_cvar, &conn->lock, absolute, sleep);

        /* Connection could have been closed while we were waiting (see
         * ipc_connection_close()). */
        if (conn->state == IPC_CONNECTION_CLOSED)
            return STATUS_CONN_HUNGUP;

        if (ret != STATUS_SUCCESS && !endpoint->message_count)
            return ret;
    }

    assert(!list_empty(&endpoint->messages));
    ipc_kmessage_t *msg = list_first(&endpoint->messages, ipc_kmessage_t, header);
    list_remove(&msg->header);

    if (--endpoint->message_count < IPC_QUEUE_MAX)
        condvar_signal(&endpoint->space_cvar);

    *_msg = msg;
    return STATUS_SUCCESS;
}

/**
 * Kernel interface.
 */

/**
 * Allocates a new, zeroed IPC message structure. To attach data to the message,
 * use ipc_kmessage_set_data(). To attach a handle to the message, use
 * ipc_kmessage_set_handle().
 */
ipc_kmessage_t *ipc_kmessage_alloc(void) {
    ipc_kmessage_t *msg = slab_cache_alloc(ipc_kmessage_cache, MM_KERNEL);
    memset(msg, 0, sizeof(*msg));
    refcount_set(&msg->count, 1);
    list_init(&msg->header);
    return msg;
}

/** Increase the reference count of a message structure. */
void ipc_kmessage_retain(ipc_kmessage_t *msg) {
    refcount_inc(&msg->count);
}

/**
 * Decreases the reference count of a message structure. If it is no longer
 * used, it will be freed, along with any attached data/handle.
 */
void ipc_kmessage_release(ipc_kmessage_t *msg) {
    if (refcount_dec(&msg->count) > 0)
        return;

    if (msg->handle)
        object_handle_release(msg->handle);

    kfree(msg->data);
    slab_cache_free(ipc_kmessage_cache, msg);
}

/**
 * Sets the data attached to a message to the specified buffer. The buffer
 * should be allocated with a kmalloc()-based function, and will become owned
 * by the message, i.e. when the message is destroyed, kfree() will be called
 * on the buffer.
 *
 * @param msg           Message to attach to.
 * @param data          Data buffer to attach (should be NULL if size is 0).
 * @param size          Size of the buffer. Must not exceed IPC_DATA_MAX.
 */
void ipc_kmessage_set_data(ipc_kmessage_t *msg, void *data, size_t size) {
    assert(!size == !data);
    assert(size <= IPC_DATA_MAX);

    if (msg->data)
        kfree(msg->data);

    msg->msg.size = size;
    msg->data = data;
}

/**
 * Attaches the specified object handle to a message. The handle must be to a
 * transferrable object. The handle will have a new reference added to it. If
 * the message already has a handle, it will be released.
 *
 * @param msg           Message to attach to.
 * @param handle        Handle to attach (NULL to remove handle).
 */
void ipc_kmessage_set_handle(ipc_kmessage_t *msg, object_handle_t *handle) {
    assert(!handle || handle->type->flags & OBJECT_TRANSFERRABLE);

    if (msg->handle)
        object_handle_release(msg->handle);

    if (handle) {
        object_handle_retain(handle);
        msg->msg.flags |= IPC_MESSAGE_HANDLE;
    } else {
        msg->msg.flags &= ~IPC_MESSAGE_HANDLE;
    }

    msg->handle = handle;
}

/**
 * Create an IPC connection for communication between the kernel and the
 * current usermode process.
 *
 * The returned endpoint is the kernel side of the connection, and a handle to
 * the user side of the connection will be created and written to the given
 * pointers.
 *
 * @param flags         Kernel endpoint flags (IPC_ENDPOINT_*).
 * @param ops           Kernel endpoint operations (can be null if the kernel
 *                      side will just communicate via regular send/receive).
 * @param private       Private data pointer for the operations to use.
 * @param _endpoint     Where to return kernel endpoint.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 *
 * @return              Status code describing result of the operation.
 */
status_t ipc_connection_create(
    unsigned flags, const ipc_endpoint_ops_t *ops, void *private,
    ipc_endpoint_t **_endpoint, handle_t *_id, handle_t *_uid)
{
    if (!_uid)
        return STATUS_INVALID_ARG;

    ipc_connection_t *conn = slab_cache_alloc(ipc_connection_cache, MM_KERNEL);

    conn->state = IPC_CONNECTION_ACTIVE;

    /* Set reference count to 2 to count both sides. */
    refcount_set(&conn->count, 2);

    ipc_endpoint_t *server = &conn->endpoints[SERVER_ENDPOINT];
    ipc_endpoint_t *client = &conn->endpoints[CLIENT_ENDPOINT];

    server->flags   = flags;
    server->ops     = ops;
    server->private = private;
    server->process = NULL;

    client->flags   = 0;
    client->ops     = NULL;
    client->private = NULL;
    client->process = curr_proc;

    status_t ret = object_handle_open(&connection_object_type, client, _id, _uid);
    if (ret != STATUS_SUCCESS) {
        /* Release both sides. */
        ipc_connection_close(server);
        ipc_connection_release(conn);
    }

    *_endpoint = server;
    return ret;
}

/**
 * Closes a connection. The endpoint must not be used after this function has
 * returned.
 *
 * @param endpoint      Endpoint of the connection.
 */
void ipc_connection_close(ipc_endpoint_t *endpoint) {
    ipc_connection_t *conn = endpoint->conn;

    mutex_lock(&conn->lock);

    if (conn->state == IPC_CONNECTION_ACTIVE) {
        /* The connection is active so the remote process could still have
         * threads waiting for space at this end or for messages at its end.
         * Wake these up and they will see that the connection is now closed
         * and return an error. */
        condvar_broadcast(&endpoint->space_cvar);
        condvar_broadcast(&endpoint->remote->data_cvar);
    }

    bool closing = conn->state != IPC_CONNECTION_CLOSED;
    if (closing) {
        conn->state = IPC_CONNECTION_CLOSED;
        notifier_run(&endpoint->remote->hangup_notifier, NULL, false);
    }

    /* Discard all currently queued messages. */
    list_foreach_safe(&endpoint->messages, iter) {
        ipc_kmessage_t *msg = list_entry(iter, ipc_kmessage_t, header);

        list_remove(&msg->header);
        ipc_kmessage_release(msg);
    }

    endpoint->message_count = 0;

    if (endpoint->pending) {
        ipc_kmessage_release(endpoint->pending);
        endpoint->pending = NULL;
    }

    /* Process could be dying and therefore could be freed, make sure the remote
     * can't open it. */
    endpoint->process = NULL;

    assert(notifier_empty(&endpoint->hangup_notifier));
    assert(notifier_empty(&endpoint->message_notifier));

    dprintf("ipc: closed endpoint %p (conn: %p)\n", endpoint, conn);

    mutex_unlock(&conn->lock);

    if (closing && endpoint->remote->ops && endpoint->remote->ops->close)
        endpoint->remote->ops->close(endpoint->remote);

    ipc_connection_release(conn);
}

/**
 * Queues a message at the remote end of a connection. The connection must be
 * in the active state. Messages are sent asynchronously. Message queues have a
 * finite length to prevent flooding when a process is not able to handle the
 * volume of incoming messages: if the remote message queue is full, this
 * function can block, unless the IPC_FORCE flag is set. This flag causes the
 * queue size limit to be ignored. If the IPC_INTERRUPTIBLE flag is set, the
 * thread will be interruptible while waiting for queue space.
 *
 * Kernel messages are sent with the security context of the current thread.
 *
 * @param endpoint      Caller's endpoint of the connection.
 * @param msg           Message to send. Will be referenced, caller must still
 *                      release it after sending.
 * @param flags         Behaviour flags.
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until space is available in the remote message queue.
 *                      A value of 0 will return an error immediately if there
 *                      is no space available in the message queue. Otherwise,
 *                      the function will return an error if space does not
 *                      become available within the given time period.
 *
 * @return              STATUS_SUCCESS if the message was sent successfully.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and no space is
 *                      available in the remote message queue.
 *                      STATUS_TIMED_OUT if timeout passes without space
 *                      becoming available in the message queue.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for space in the message queue.
 *                      STATUS_CONN_HUNGUP if the remote end has hung up the
 *                      connection.
 */
status_t ipc_connection_send(
    ipc_endpoint_t *endpoint, ipc_kmessage_t *msg, unsigned flags,
    nstime_t timeout)
{
    status_t ret;

    ipc_connection_t *conn = endpoint->conn;
    ipc_endpoint_t *remote = endpoint->remote;

    mutex_lock(&conn->lock);

    assert(conn->state != IPC_CONNECTION_SETUP);

    /* Clear any pending data left at our endpoint. */
    if (endpoint->pending) {
        ipc_kmessage_release(endpoint->pending);
        endpoint->pending = NULL;
    }

    if (conn->state == IPC_CONNECTION_CLOSED) {
        ret = STATUS_CONN_HUNGUP;
        goto out;
    }

    if (remote->flags & IPC_ENDPOINT_DROP) {
        ret = STATUS_SUCCESS;
        goto out;
    }

    /* Save the message timestamp and security context. */
    msg->msg.timestamp = system_time();
    if (msg->msg.flags & IPC_MESSAGE_SECURITY)
        memcpy(&msg->security, security_current_context(), sizeof(msg->security));

    if (remote->ops && remote->ops->receive) {
        mutex_unlock(&conn->lock);
        return remote->ops->receive(remote, msg, flags, timeout);
    }

    /* Wait for queue space if we're not forcing the send. */
    if (!(flags & IPC_FORCE)) {
        nstime_t absolute = (timeout > 0) ? system_time() + timeout : timeout;
        unsigned sleep    = SLEEP_ABSOLUTE;

        if (flags & IPC_INTERRUPTIBLE)
            sleep |= SLEEP_INTERRUPTIBLE;

        while (remote->message_count >= IPC_QUEUE_MAX) {
            ret = condvar_wait_etc(&remote->space_cvar, &conn->lock, absolute, sleep);

            /* Connection could have been closed while we were waiting (see
             * ipc_connection_close()). */
            if (conn->state == IPC_CONNECTION_CLOSED) {
                ret = STATUS_CONN_HUNGUP;
                goto out;
            }

            if (ret != STATUS_SUCCESS) {
                if (remote->message_count >= IPC_QUEUE_MAX)
                    goto out;
            }
        }
    }

    /* Queue the message. */
    ipc_kmessage_retain(msg);
    list_append(&remote->messages, &msg->header);
    remote->message_count++;
    condvar_signal(&remote->data_cvar);
    notifier_run(&remote->message_notifier, NULL, false);

    ret = STATUS_SUCCESS;

out:
    mutex_unlock(&conn->lock);
    return ret;
}

/**
 * Waits until a message arrives on a connection. Data or handles attached to
 * the message will be available in the returned message structure. If the
 * IPC_INTERRUPTIBLE flag is set, the calling thread will be interruptible
 * while waiting for queue space.
 *
 * @param endpoint      Endpoint of the connection.
 * @param flags         Behaviour flags.
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until a message is received. A value of 0 will return
 *                      an error immediately if there are no messages queued.
 *                      Otherwise, the function will return an error if a
 *                      message does not arrive within the given time period.
 * @param _msg          Where to store pointer to received message. Will be
 *                      referenced, must be released when no longer needed.
 *
 * @return              STATUS_SUCCESS if a message was received.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and no messages are
 *                      queued.
 *                      STATUS_TIMED_OUT if timeout passes without a message
 *                      being received.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for messages.
 *                      STATUS_CONN_HUNGUP if the remote end has hung up the
 *                      connection.
 */
status_t ipc_connection_receive(
    ipc_endpoint_t *endpoint, unsigned flags, nstime_t timeout,
    ipc_kmessage_t **_msg)
{
    assert(!endpoint->ops || !endpoint->ops->receive);

    ipc_connection_t *conn = endpoint->conn;

    mutex_lock(&conn->lock);
    status_t ret = receive_message(conn, endpoint, flags, timeout, _msg);
    mutex_unlock(&conn->lock);

    return ret;
}

/** Increase the reference count of a port. */
void ipc_port_retain(ipc_port_t *port) {
    refcount_inc(&port->count);
}

/** Decrease the reference count of a port. */
void ipc_port_release(ipc_port_t *port) {
    if (refcount_dec(&port->count) > 0)
        return;

    assert(list_empty(&port->waiting));
    assert(notifier_empty(&port->connection_notifier));

    dprintf("ipc: destroying port %p\n", port);

    slab_cache_free(ipc_port_cache, port);
}

/**
 * Creates a handle to an IPC port and publishes it in the current process'
 * handle table.
 *
 * @param port          Port to publish.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 */
status_t ipc_port_publish(ipc_port_t *port, handle_t *_id, handle_t *_uid) {
    ipc_port_retain(port);

    object_handle_t *handle = object_handle_create(&port_object_type, port);
    status_t ret = object_handle_attach(handle, _id, _uid);
    object_handle_release(handle);
    return ret;
}

/** Initialize the IPC system. */
__init_text void ipc_init(void) {
    ipc_port_cache = object_cache_create(
        "ipc_port_cache",
        ipc_port_t, ipc_port_ctor, NULL, NULL, 0, MM_BOOT);
    ipc_connection_cache = object_cache_create(
        "ipc_connection_cache",
        ipc_connection_t, ipc_connection_ctor, NULL, NULL, 0, MM_BOOT);
    ipc_kmessage_cache = object_cache_create(
        "ipc_kmessage_cache",
        ipc_kmessage_t, NULL, NULL, NULL, 0, MM_BOOT);
}

/**
 * Userspace interface.
 */

/**
 * Creates a new IPC port. A port is a point of connection to a process. Only
 * the process that creates a port can listen for connections on the port. Any
 * process with a handle to a port is able to open a connection to it. The
 * calling process can transfer the returned handle to other processes to
 * allow them to connect to it, then listen on that handle to receive
 * connection attempts. Connections made on a port have no relation to the port
 * after they are set up: when a port's owner closes its handle to it, all
 * connections that were made on the port remain active.
 *
 * @param _handle       Where to store handle to port.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_port_create(handle_t *_handle) {
    if (!_handle)
        return STATUS_INVALID_ARG;

    ipc_port_t *port = slab_cache_alloc(ipc_port_cache, MM_KERNEL);
    refcount_set(&port->count, 1);
    port->owner       = curr_proc;
    port->owner_count = 0;

    /* This handle takes over the reference we've just set, and increments
     * owner_count upon attach. */
    object_handle_t *handle = object_handle_create(&port_object_type, port);
    status_t ret            = object_handle_attach(handle, NULL, _handle);
    if (ret == STATUS_SUCCESS)
        dprintf("ipc: process %" PRId32 " created port %p\n", curr_proc->id, port);

    object_handle_release(handle);
    return ret;
}

/**
 * Listens for a connection on the given port. Only the process that created
 * a port may listen on it. When a connection is received, a handle to the
 * server side of the connection is returned.
 *
 * Once created, connection objects have no relation to the port they were
 * opened on. If the port is destroyed, any active connections remain open.
 *
 * @param handle        Handle to port to listen on.
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until a connection attempt is received. A value of 0
 *                      will return an error immediately if no connection
 *                      attempts are pending. Otherwise, the function will
 *                      return an error if no connection attempts are received
 *                      within the given time period.
 * @param _handle       Where to store handle to server side of the connection.
 *
 * @return              STATUS_SUCCESS if a connection attempt was received.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and no connections
 *                      are currently pending.
 *                      STATUS_TIMED_OUT if timeout passes without a connection
 *                      attempt.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for a connection.
 */
status_t kern_port_listen(handle_t handle, nstime_t timeout, handle_t *_handle) {
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_PORT, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_port_t *port = khandle->private;
    mutex_lock(&port->lock);

    if (curr_proc != port->owner) {
        ret = STATUS_ACCESS_DENIED;
        goto out_unlock_port;
    }

    /* Try to get a connection. We have to handle the case where the connection
     * attempt is pulled off the list (e.g. if it times out) between getting
     * woken and retaking the lock. */
    nstime_t absolute = (timeout > 0) ? system_time() + timeout : timeout;
    while (list_empty(&port->waiting)) {
        ret = condvar_wait_etc(
            &port->listen_cvar, &port->lock, absolute,
            SLEEP_INTERRUPTIBLE | SLEEP_ABSOLUTE);
        if (ret != STATUS_SUCCESS && list_empty(&port->waiting))
            goto out_unlock_port;
    }

    ipc_connection_t *conn = list_first(&port->waiting, ipc_connection_t, header);

    mutex_lock(&conn->lock);
    assert(conn->state == IPC_CONNECTION_SETUP);

    ipc_endpoint_t *endpoint = &conn->endpoints[SERVER_ENDPOINT];

    refcount_inc(&conn->count);
    ret = object_handle_open(&connection_object_type, endpoint, NULL, _handle);
    if (ret != STATUS_SUCCESS) {
        /* We do not want the close callback to be called if this fails, just
         * leave the connection waiting on the port. */
        refcount_dec(&conn->count);
        goto out_unlock_conn;
    }

    endpoint->process = curr_proc;

    /* Activate the connection and wake the connecting thread. */
    conn->state = IPC_CONNECTION_ACTIVE;
    condvar_broadcast(&conn->open_cvar);
    list_remove(&conn->header);

    dprintf(
        "ipc: process %" PRId32 " received connection on port %p (conn: %p, endpoint: %p)\n",
        curr_proc->id, port, conn, endpoint);

    ret = STATUS_SUCCESS;

out_unlock_conn:
    mutex_unlock(&conn->lock);

out_unlock_port:
    mutex_unlock(&port->lock);
    object_handle_release(khandle);
    return ret;
}

/**
 * Opens a connection to another process. This can be via a port handle, or a
 * special port identifier. The function will remain blocked until either the
 * server receives the connection, or until the given timeout expires.
 *
 * A number of per-process/per-thread special ports are defined, which can be
 * given as the port argument to this function:
 *
 *  - PROCESS_ROOT_PORT connects to the current process' root port, which is
 *    typically a port owned by a service manager process that can be used by
 *    processes to reach other system services.
 *
 * @param port          Handle to port or special port ID to connect to.
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until the connection is received. A value of 0 will
 *                      return an error immediately if the connection cannot be
 *                      made without delay (i.e. if the server process is not
 *                      currently listening on the port). Otherwise, the
 *                      function will return an error if the connection is not
 *                      received within the given time period.
 * @param _handle       Where to store handle to client side of the connection.
 *
 * @return              STATUS_SUCCESS if connection is successfully opened.
 *                      STATUS_CONN_HUNGUP if the port is dead.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and the connection
 *                      cannot be accepted immediately.
 *                      STATUS_TIMED_OUT if timeout passes without the
 *                      connection being received.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for a connection.
 */
status_t kern_connection_open(handle_t port, nstime_t timeout, handle_t *_handle) {
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    ipc_port_t *kport;
    if (port < 0) {
        switch (port) {
            case PROCESS_ROOT_PORT:
                kport = curr_proc->root_port;
                break;
            default:
                return STATUS_INVALID_ARG;
        }

        if (!kport)
            return STATUS_NOT_FOUND;

        ipc_port_retain(kport);
    } else {
        object_handle_t *khandle;
        ret = object_handle_lookup(port, OBJECT_TYPE_PORT, &khandle);
        if (ret != STATUS_SUCCESS)
            return ret;

        kport = khandle->private;
        ipc_port_retain(kport);
        object_handle_release(khandle);
    }

    mutex_lock(&kport->lock);

    if (!kport->owner) {
        ret = STATUS_CONN_HUNGUP;
        goto out_unlock_port;
    }

    ipc_connection_t *conn = slab_cache_alloc(ipc_connection_cache, MM_KERNEL);

    conn->state = IPC_CONNECTION_SETUP;

    for (size_t i = 0; i < array_size(conn->endpoints); i++) {
        conn->endpoints[i].flags   = 0;
        conn->endpoints[i].ops     = NULL;
        conn->endpoints[i].private = NULL;
        conn->endpoints[i].process = NULL;
    }

    ipc_endpoint_t *endpoint = &conn->endpoints[CLIENT_ENDPOINT];

    endpoint->process = curr_proc;

    /* We initially set the reference count to 1 for the client. If connection
     * succeeds, the kern_port_listen() call will add a reference for the
     * server. */
    refcount_set(&conn->count, 1);

    /* Queue the connection on the port. */
    list_append(&kport->waiting, &conn->header);
    condvar_signal(&kport->listen_cvar);
    notifier_run(&kport->connection_notifier, NULL, false);

    /* Wait for the connection to be accepted. */
    ret = condvar_wait_etc(&conn->open_cvar, &kport->lock, timeout, SLEEP_INTERRUPTIBLE);

    mutex_lock(&conn->lock);

    if (ret != STATUS_SUCCESS) {
        /* Even if the wait failed, the connection could have been accepted
         * while we were trying to take the locks. */
        if (conn->state == IPC_CONNECTION_ACTIVE) {
            assert(list_empty(&conn->header));
            ret = STATUS_SUCCESS;
        } else {
            /* The connection can still be on the list. */
            list_remove(&conn->header);
        }
    } else {
        assert(conn->state != IPC_CONNECTION_SETUP);
    }

    /* Similarly, the connection could have been closed or the port could have
     * been disowned (see port_object_detach()). */
    if (conn->state == IPC_CONNECTION_CLOSED)
        ret = STATUS_CONN_HUNGUP;

    mutex_unlock(&conn->lock);

    if (ret == STATUS_SUCCESS) {
        dprintf(
            "ipc: process %" PRId32 " connected to port %p (conn: %p, endpoint: %p)\n",
            curr_proc->id, kport, conn, endpoint);

        ret = object_handle_open(&connection_object_type, endpoint, NULL, _handle);
    }

    if (ret != STATUS_SUCCESS)
        ipc_connection_close(endpoint);

out_unlock_port:
    mutex_unlock(&kport->lock);
    ipc_port_release(kport);
    return ret;
}

/** Opens a handle to the process at the remote end of a connection.
 * @param handle        Handle to connection.
 * @param _process      Where to store handle to remote process.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NOT_FOUND if the remote process cannot be opened. */
status_t kern_connection_open_remote(handle_t handle, handle_t *_process) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;

    {
        MUTEX_SCOPED_LOCK(lock, &endpoint->conn->lock);

        if (endpoint->remote->process) {
            ret = process_publish(endpoint->remote->process, NULL, _process);
        } else {
            ret = STATUS_NOT_FOUND;
        }
    }

    object_handle_release(khandle);
    return ret;
}

/** Copy a message from userspace.
 * @param umsg          User message pointer.
 * @param data          Data pointer.
 * @param handle        Attached handle.
 * @param _kmsg         Where to store pointer to kernel message.
 * @return              Status code describing result of the operation. */
static status_t copy_message_from_user(
    const ipc_message_t *umsg, const void *data, handle_t handle,
    ipc_kmessage_t **_kmsg)
{
    status_t ret;

    if (!umsg)
        return STATUS_INVALID_ARG;

    ipc_kmessage_t *kmsg = ipc_kmessage_alloc();

    ret = memcpy_from_user(&kmsg->msg, umsg, sizeof(kmsg->msg));
    if (ret != STATUS_SUCCESS)
        goto err;

    if (kmsg->msg.size) {
        if (kmsg->msg.size > IPC_DATA_MAX) {
            ret = STATUS_TOO_LARGE;
            goto err;
        } else if (!data) {
            ret = STATUS_INVALID_ARG;
            goto err;
        }

        kmsg->data = kmalloc(kmsg->msg.size, MM_USER);
        if (!kmsg->data) {
            ret = STATUS_NO_MEMORY;
            goto err;
        }

        ret = memcpy_from_user(kmsg->data, data, kmsg->msg.size);
        if (ret != STATUS_SUCCESS)
            goto err;
    } else if (data) {
        ret = STATUS_INVALID_ARG;
        goto err;
    }

    if (kmsg->msg.flags & IPC_MESSAGE_HANDLE) {
        ret = object_handle_lookup(handle, -1, &kmsg->handle);
        if (ret != STATUS_SUCCESS) {
            goto err;
        } else if (!(kmsg->handle->type->flags & OBJECT_TRANSFERRABLE)) {
            ret = STATUS_NOT_SUPPORTED;
            goto err;
        }
    }

    *_kmsg = kmsg;
    return STATUS_SUCCESS;

err:
    ipc_kmessage_release(kmsg);
    return ret;
}

/**
 * Checks whether a connection is still active or whether the remote end has
 * hung up.
 *
 * @param handle        Handle to connection.
 *
 * @return              STATUS_SUCCESS if the connection is still active.
 *                      STATUS_CONN_HUNGUP if the remote end has hung up.
 */
status_t kern_connection_status(handle_t handle) {
    status_t ret;
    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;
    ipc_connection_t *conn   = endpoint->conn;

    mutex_lock(&conn->lock);

    assert(conn->state != IPC_CONNECTION_SETUP);
    ret = (conn->state == IPC_CONNECTION_CLOSED) ? STATUS_CONN_HUNGUP : STATUS_SUCCESS;

    mutex_unlock(&conn->lock);
    return ret;
}

/**
 * Queues a message at the remote end of a connection. Messages are sent
 * asynchronously. Message queues have a finite length to prevent flooding when
 * a process is not able to handle the volume of incoming messages: if the
 * remote message queue is full, this function can block.
 *
 * Handles to transferrable objects can be attached to the message by setting
 * IPC_MESSAGE_HANDLE in the message flags, and passing an object handle in the
 * attached parameter.
 *
 * The calling thread's current security context can be attached to the message
 * by setting IPC_MESSAGE_SECURITY in the message flags.
 *
 * If attachments to a previous message are currently pending from a previous
 * call to kern_connection_receive(), they will be discarded.
 *
 * @param handle        Handle to connection.
 * @param msg           Message to send.
 * @param data          Data to attach to message (must not be NULL if message
 *                      has a non-zero size, ignored otherwise).
 * @param attached      Attached handle (must be a valid handle to a
 *                      transferrable object if message has IPC_MESSAGE_HANDLE
 *                      flag set, ignored otherwise).
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until space is available in the remote message queue.
 *                      A value of 0 will return an error immediately if there
 *                      is no space available in the message queue. Otherwise,
 *                      the function will return an error if space does not
 *                      become available within the given time period.
 *
 * @return              STATUS_SUCCESS if the message was sent successfully.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and no space is
 *                      available in the remote message queue.
 *                      STATUS_TIMED_OUT if timeout passes without space
 *                      becoming available in the message queue.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for space in the message queue.
 *                      STATUS_CONN_HUNGUP if the remote end has hung up the
 *                      connection.
 */
status_t kern_connection_send(
    handle_t handle, const ipc_message_t *msg, const void *data,
    handle_t attached, nstime_t timeout)
{
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;

    ipc_kmessage_t *kmsg;
    ret = copy_message_from_user(msg, data, attached, &kmsg);
    if (ret != STATUS_SUCCESS)
        goto out_release_conn;

    ret = ipc_connection_send(endpoint, kmsg, IPC_INTERRUPTIBLE, timeout);
    ipc_kmessage_release(kmsg);

out_release_conn:
    object_handle_release(khandle);
    return ret;
}

/**
 * Receives a message on connection. This will waits until a message arrives on
 * the connection and copy it into the supplied buffer.
 *
 * If the message has a data buffer attached, indicated by a non-zero size in
 * the returned message, it can be retrieved by calling
 * kern_connection_receive_data().
 *
 * If it has a handle attached, indicated by the IPC_MESSAGE_HANDLE flag in the
 * returned message, it can be retrieved by calling
 * kern_connection_receive_handle().
 *
 * If it has the sending thread's security context (at the time the message was
 * sent) attached, indicated by the IPC_MESSAGE_SECURITY flag in the returned
 * message, it will be returned in the buffer given in the security parameter.
 * If it does not, the supplied buffer (if any) will be zeroed. The caller
 * should check for the flag before attempting to use the context.
 *
 * Any attachments will be available until the next call to
 * kern_connection_send() or kern_connection_receive() on the connection, at
 * which point attachments that have not been retrieved will be dropped.
 *
 * @param handle        Handle to connection.
 * @param msg           Where to store received message.
 * @param security      Where to store the security context of the sending
 *                      thread, if attached (can be NULL).
 * @param timeout       Timeout in nanoseconds. A negative value will block
 *                      until a message is received. A value of 0 will return
 *                      an error immediately if there are no messages queued.
 *                      Otherwise, the function will return an error if a
 *                      message does not arrive within the given time period.
 *
 * @return              STATUS_SUCCESS if a message was received.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and no messages are
 *                      queued.
 *                      STATUS_TIMED_OUT if timeout passes without a message
 *                      being received.
 *                      STATUS_INTERRUPTED if the calling thread is interrupted
 *                      while waiting for messages.
 *                      STATUS_CONN_HUNGUP if the remote end has hung up the
 *                      connection.
 */
status_t kern_connection_receive(
    handle_t handle, ipc_message_t *msg, security_context_t *security,
    nstime_t timeout)
{
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;
    ipc_connection_t *conn   = endpoint->conn;

    mutex_lock(&conn->lock);

    ipc_kmessage_t *kmsg;
    ret = receive_message(conn, endpoint, IPC_INTERRUPTIBLE, timeout, &kmsg);
    if (ret != STATUS_SUCCESS)
        goto out_unlock_conn;

    ret = memcpy_to_user(msg, &kmsg->msg, sizeof(*msg));
    if (ret != STATUS_SUCCESS) {
        /* The message is lost in this case, but they shouldn't have given us
         * a bad pointer... */
        ipc_kmessage_release(kmsg);
        goto out_unlock_conn;
    }

    if (security) {
        if (kmsg->msg.flags & IPC_MESSAGE_SECURITY) {
            ret = memcpy_to_user(security, &kmsg->security, sizeof(*security));
        } else {
            ret = memset_user(security, 0, sizeof(*security));
        }

        if (ret != STATUS_SUCCESS) {
            /* Same as above. */
            ipc_kmessage_release(kmsg);
            goto out_unlock_conn;
        }
    }

    /* Save the message if there is data or a handle to retrieve, otherwise
     * free it. */
    if (ipc_kmessage_has_attachment(kmsg)) {
        /* Hmm, not sure whether this is actually necessary. */
        if (endpoint->pending)
            ipc_kmessage_release(endpoint->pending);

        endpoint->pending = kmsg;
    } else {
        ipc_kmessage_release(kmsg);
    }

out_unlock_conn:
    mutex_unlock(&conn->lock);
    object_handle_release(khandle);
    return ret;
}

/**
 * Receives data attached to the last received message on a connection. The
 * data will be copied to the specified buffer. Upon successful completion, the
 * stored copy of the data will be dropped and will not be available again by a
 * subsequent call to this function.
 *
 * @param handle        Handle to connection.
 * @param data          Buffer to copy into. This should be at least the size
 *                      indicated in the received message. If NULL, the
 *                      pending data will be dropped without being copied.
 *
 * @return              STATUS_SUCCESS if data is copied successfully.
 *                      STATUS_NOT_FOUND if no pending data is available.
 */
status_t kern_connection_receive_data(handle_t handle, void *data) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;

    mutex_lock(&endpoint->conn->lock);

    ipc_kmessage_t *msg = endpoint->pending;
    if (msg && msg->data) {
        /* Just drop the data if the pointer is NULL. */
        ret = (data)
            ? memcpy_to_user(data, msg->data, msg->msg.size)
            : STATUS_SUCCESS;
        if (ret == STATUS_SUCCESS) {
            ipc_kmessage_set_data(msg, NULL, 0);

            /* Discard if now empty. */
            if (!msg->handle) {
                ipc_kmessage_release(msg);
                endpoint->pending = NULL;
            }
        }
    } else {
        ret = STATUS_NOT_FOUND;
    }

    mutex_unlock(&endpoint->conn->lock);
    object_handle_release(khandle);
    return ret;
}

/**
 * Receives the handle attached to the last received message on a connection.
 * Upon successful completion, the stored handle will be dropped and will not
 * be available again by a subsequent call to this function.
 *
 * @param handle        Handle to connection.
 * @param _attached     Where to store handle received. If NULL, the pending
 *                      handle will be dropped.
 *
 * @return              STATUS_SUCCESS if data is copied successfully.
 *                      STATUS_NOT_FOUND if no pending data is available.
 *                      STATUS_NO_HANDLES if the calling process' handle table
 *                      is full.
 */
status_t kern_connection_receive_handle(handle_t handle, handle_t *_attached) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ipc_endpoint_t *endpoint = khandle->private;

    mutex_lock(&endpoint->conn->lock);

    ipc_kmessage_t *msg = endpoint->pending;
    if (msg && msg->handle) {
        /* Just drop the handle if the pointer is NULL. */
        ret = (_attached)
            ? object_handle_attach(msg->handle, NULL, _attached)
            : STATUS_SUCCESS;
        if (ret == STATUS_SUCCESS) {
            ipc_kmessage_set_handle(msg, NULL);

            /* Discard if now empty. */
            if (!msg->data) {
                ipc_kmessage_release(msg);
                endpoint->pending = NULL;
            }
        }
    } else {
        ret = STATUS_NOT_FOUND;
    }

    mutex_unlock(&endpoint->conn->lock);
    object_handle_release(khandle);
    return ret;
}
