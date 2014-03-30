/*
 * Copyright (C) 2013-2014 Alex Smith
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
 * @brief		Kernel IPC interface.
 */

#ifndef __IPC_IPC_H
#define __IPC_IPC_H

#include <kernel/ipc.h>

#include <lib/list.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <proc/process.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <object.h>

struct ipc_connection;
struct ipc_endpoint;
struct ipc_port;

/** Kernel-internal IPC message structure. */
typedef struct ipc_kmessage {
	list_t header;			/**< Link to message queue. */
	refcount_t count;		/**< Reference count. */
	ipc_message_t msg;		/**< Wrapped user message structure. */
	security_context_t security;	/**< Security context that the message was sent with. */
	void *data;			/**< Attached data (NULL if size is 0). */
	object_handle_t *handle;	/**< Attached handle (can be NULL). */
} ipc_kmessage_t;

/** IPC endpoint structure. */
typedef struct ipc_endpoint {
	struct ipc_endpoint *remote;	/**< Other end of the connection. */
	struct ipc_connection *conn;	/**< Connection structure. */
	unsigned flags;			/**< Behaviour flags for the endpoint. */

	list_t messages;		/**< List of queued messages. */
	size_t message_count;		/**< Number of messages in message queue. */
	condvar_t space_cvar;		/**< Condition variable to wait for space. */
	condvar_t data_cvar;		/**< Condition variable to wait for data. */
	ipc_kmessage_t *pending;	/**< Message with pending data/handle. */

	notifier_t hangup_notifier;	/**< Notifier for remote end being closed. */
	notifier_t message_notifier;	/**< Notifier for message arrival. */
} ipc_endpoint_t;

/** IPC endpoint behaviour flags. */
#define IPC_ENDPOINT_DROP	(1<<0)	/**< Drop all received messages. */

/** IPC connection structure. */
typedef struct ipc_connection {
	refcount_t count;		/**< Reference count. */
	mutex_t lock;			/**< Lock for the connection. */

	/** State of the connection. */
	enum {
		/** Connection is being set up. */
		IPC_CONNECTION_SETUP,

		/** Connection is active. */
		IPC_CONNECTION_ACTIVE,

		/** At least one side has closed the connection. */
		IPC_CONNECTION_CLOSED,
	} state;

	/** Endpoints for each end of the connection. */
	ipc_endpoint_t endpoints[2];

	/** Data used during connection setup. */
	list_t header;			/**< Link to connection list. */
	condvar_t open_cvar;		/**< Condition for connecting thread to wait on. */
	ipc_client_t *client;		/**< Pointer to client information structure. */
} ipc_connection_t;

/** Definitions for endpoint IDs. */
#define SERVER_ENDPOINT		0	/**< Endpoint for the server (port owner). */
#define CLIENT_ENDPOINT		1	/**< Endpoint for the client (connection opener). */

/** IPC port operations. */
typedef struct ipc_port_ops {
	/**
	 * Handle a connection on a port.
	 *
	 * This function is called when a connection attempt is made to this
	 * port. The given endpoint refers to the server side of the connection.
	 * This function is called from the context of the thread making the
	 * connection.
	 *
	 * Returning an error from this function has the effect of rejecting
	 * the connection and causing that error to be returned to the client's
	 * connect call. Returning STATUS_SUCCESS causes the connection to
	 * become active.
	 *
	 * @param port		Port being connected to (locked).
	 * @param endpoint	Endpoint for server side of the connection.
	 * @param timeout	Timeout in nanoseconds. 0 should return
	 *			immediately if unable to connect without delay,
	 *			-1 should block forever.
	 *
	 * @return		Status code describing result of the operation.
	 */
	status_t (*connect)(struct ipc_port *port, ipc_endpoint_t *endpoint,
		nstime_t timeout);
} ipc_port_ops_t;

/** IPC port structure. */
typedef struct ipc_port {
	mutex_t lock;			/**< Lock for structure. */
	refcount_t count;		/**< References to the port. */
	ipc_port_ops_t *ops;		/**< Operations for the port. */
	void *private;			/**< Private data pointer. */
	process_t *owner;		/**< Owning process. */
	size_t owner_count;		/**< References from the owner. */
	list_t waiting;			/**< List of in-progress connection attempts. */
	condvar_t listen_cvar;		/**< Connection condition variable. */
	notifier_t connection_notifier;	/**< Notifier for connection attempts. */
} ipc_port_t;

/** Kernel internal IPC flags. */
#define IPC_INTERRUPTIBLE	(1<<0)	/**< Operation can be interrupted. */
#define IPC_FORCE		(1<<1)	/**< Ignore queue size limit. */

extern ipc_kmessage_t *ipc_kmessage_alloc(void);
extern void ipc_kmessage_retain(ipc_kmessage_t *msg);
extern void ipc_kmessage_release(ipc_kmessage_t *msg);
extern void ipc_kmessage_set_data(ipc_kmessage_t *msg, void *data, size_t size);
extern void ipc_kmessage_set_handle(ipc_kmessage_t *msg,
	object_handle_t *handle);

/** Check whether a message has attached data.
 * @param msg		Message to check.
 * @return		Whether data is attached. */
static inline bool ipc_kmessage_has_attachment(ipc_kmessage_t *msg) {
	return (msg->data || msg->handle);
}

extern void ipc_connection_close(ipc_endpoint_t *endpoint);
extern status_t ipc_connection_send(ipc_endpoint_t *endpoint,
	ipc_kmessage_t *msg, unsigned flags, nstime_t timeout);
extern status_t ipc_connection_receive(ipc_endpoint_t *endpoint, unsigned flags,
	nstime_t timeout, ipc_kmessage_t **msgp);

extern ipc_port_t *ipc_port_create(ipc_port_ops_t *ops, void *private);
extern void ipc_port_destroy(ipc_port_t *port);
extern void ipc_port_retain(ipc_port_t *port);
extern void ipc_port_release(ipc_port_t *port);
extern status_t ipc_port_publish(ipc_port_t *port, handle_t *idp,
	handle_t *uidp);

extern void ipc_init(void);

#endif /* __IPC_IPC_H */
