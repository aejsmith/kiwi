/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		IPC system.
 *
 * This system implements a bi-directional communication mechanism for local
 * processes. An overview of the system is available in documentation/ipc.txt.
 * Some implementation notes are included below.
 *
 * Firstly, connections have a single lock protecting both ends of the
 * connection. This vastly simplifies locking, as with a lock for each end it
 * becomes easy to cause a deadlock when performing an operation that requires
 * locking of both endpoints.
 *
 * Secondly, neither endpoint is freed until both ends of the connection are
 * closed. This makes it simpler to handle one end of a connection being
 * closed.
 *
 * @todo		This is all a bit naff.
 */

#include <ipc/ipc.h>

#include <lib/avl_tree.h>
#include <lib/id_alloc.h>
#include <lib/notifier.h>
#include <lib/refcount.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <kernel.h>
#include <object.h>
#include <kdb.h>
#include <status.h>

#if CONFIG_IPC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

struct ipc_connection;

/** Structure used for synchronisation during connection. */
typedef struct ipc_connect_sync {
	semaphore_t sem;		/**< Semaphore for client to wait on. */
	port_client_t info;		/**< Information about the client. */
} ipc_connect_sync_t;

/** IPC port structure. */
typedef struct ipc_port {
	object_t obj;			/**< Object header. */

	mutex_t lock;			/**< Lock to protect data in structure. */
	port_id_t id;			/**< ID of the port. */
	refcount_t count;		/**< Number of handles open to the port. */
	list_t connections;		/**< List of currently open connections. */
	list_t waiting;			/**< List of in-progress connection attempts. */
	semaphore_t conn_sem;		/**< Semaphore counting connection attempts. */
	notifier_t conn_notifier;	/**< Notifier for connection attempts. */

	avl_tree_node_t tree_link;	/**< Link to node tree. */
} ipc_port_t;

/** IPC endpoint structure. */
typedef struct ipc_endpoint {
	list_t messages;		/**< List of queued messages. */
	semaphore_t space_sem;		/**< Semaphore counting space in message queue. */
	semaphore_t data_sem;		/**< Semaphore counting messages in message queue. */
	notifier_t msg_notifier;	/**< Notifier for message arrival. */
	notifier_t hangup_notifier;	/**< Notifier for remote end being closed. */
	struct ipc_endpoint *remote;	/**< Other end of the connection. */
	struct ipc_connection *conn;	/**< Connection structure. */
} ipc_endpoint_t;

/** IPC connection structure. */
typedef struct ipc_connection {
	object_t obj;			/**< Object header. */
	mutex_t lock;			/**< Lock protecting connection. */
	ipc_port_t *port;		/**< Port that the connection is on. */
	refcount_t count;		/**< Count of handles to either end of the connection. */
	ipc_endpoint_t endpoints[2];	/**< Endpoints for each end of the connection. */
	ipc_connect_sync_t *sync;	/**< Pointer to connection synchronisation structure. */
	list_t header;			/**< Link to port connection list. */
} ipc_connection_t;

/** In-kernel IPC message structure. */
typedef struct ipc_message {
	list_t header;			/**< Link to message queue. */
	uint32_t type;			/**< Type of message. */
	size_t size;			/**< Size of message data. */
	char data[];			/**< Message data. */
} ipc_message_t;

/** Definitions for endpoint IDs. */
#define SERVER_ENDPOINT		0	/**< Endpoint for the listener. */
#define CLIENT_ENDPOINT		1	/**< Endpoint for the opener. */

/** Cache for port/connection structures. */
static slab_cache_t *ipc_port_cache;
static slab_cache_t *ipc_connection_cache;

/** Port ID allocator. */
static id_alloc_t port_id_allocator;

/** Tree of all open ports. */
static AVL_TREE_DECLARE(port_tree);
static MUTEX_DECLARE(port_tree_lock, 0);

/** Port object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused). */
static void ipc_port_ctor(void *obj, void *data) {
	ipc_port_t *port = obj;

	mutex_init(&port->lock, "ipc_port_lock", 0);
	list_init(&port->connections);
	list_init(&port->waiting);
	semaphore_init(&port->conn_sem, "ipc_listen_sem", 0);
	semaphore_init(&port->conn_sem, "ipc_conn_sem", 0);
	notifier_init(&port->conn_notifier, port);
}

/** Release an IPC port object.
 * @param port		Port to release. */
static void ipc_port_release(ipc_port_t *port) {
	ipc_connection_t *conn;
	size_t i;

	if(refcount_dec(&port->count) == 0) {
		/* Take the port tree lock across the operation to prevent
		 * any threads from trying to open/connect to the port. */
		mutex_lock(&port_tree_lock);
		mutex_lock(&port->lock);

		/* Cancel all in-progress connection attempts. */
		LIST_FOREACH_SAFE(&port->waiting, iter) {
			conn = list_entry(iter, ipc_connection_t, header);

			list_remove(&conn->header);
			conn->port = NULL;
			semaphore_up(&conn->sync->sem, 1);
		}

		/* Terminate all currently open connections. We do this by
		 * disconnecting both ends of the connection from each other. */
		LIST_FOREACH_SAFE(&port->connections, iter) {
			conn = list_entry(iter, ipc_connection_t, header);

			mutex_lock(&conn->lock);

			for(i = 0; i < 2; i++) {
				waitq_wake_all(&conn->endpoints[i].space_sem.queue);
				waitq_wake_all(&conn->endpoints[i].data_sem.queue);
				notifier_run(&conn->endpoints[i].hangup_notifier, NULL, false);
				conn->endpoints[i].remote = NULL;
			}

			list_remove(&conn->header);
			conn->port = NULL;

			mutex_unlock(&conn->lock);
		}

		avl_tree_remove(&port_tree, &port->tree_link);
		mutex_unlock(&port_tree_lock);
		mutex_unlock(&port->lock);

		dprintf("ipc: destroyed port %d (%p)\n", port->id, port);
		id_alloc_release(&port_id_allocator, port->id);
		slab_cache_free(ipc_port_cache, port);
	}
}

/** Connection object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused). */
static void ipc_connection_ctor(void *obj, void *data) {
	ipc_connection_t *conn = obj;
	size_t i;

	list_init(&conn->header);
	mutex_init(&conn->lock, "ipc_connection_lock", 0);
	for(i = 0; i < 2; i++) {
		list_init(&conn->endpoints[i].messages);
		semaphore_init(&conn->endpoints[i].space_sem, "ipc_space_sem", IPC_QUEUE_MAX);
		semaphore_init(&conn->endpoints[i].data_sem, "ipc_data_sem", 0);
		notifier_init(&conn->endpoints[i].msg_notifier, &conn->endpoints[i]);
		notifier_init(&conn->endpoints[i].hangup_notifier, &conn->endpoints[i]);
	}
}

/** Closes a handle to an IPC port.
 * @param handle	Handle being closed. */
static void port_object_close(object_handle_t *handle) {
	ipc_port_release((ipc_port_t *)handle->object);
}

/** Signal that a port event is being waited for.
 * @param handle	Handle to port.
 * @param event		Event being waited for.
 * @param sync		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t port_object_wait(object_handle_t *handle, int event, void *sync) {
	ipc_port_t *port = (ipc_port_t *)handle->object;
	status_t ret = STATUS_SUCCESS;

	mutex_lock(&port->lock);

	switch(event) {
	case PORT_EVENT_CONNECTION:
		if(!object_handle_rights(handle, PORT_RIGHT_LISTEN)) {
			ret = STATUS_ACCESS_DENIED;
		}

		if(semaphore_count(&port->conn_sem)) {
			object_wait_signal(sync);
		} else {
			notifier_register(&port->conn_notifier, object_wait_notifier, sync);
		}
		break;
	default:
		ret = STATUS_INVALID_EVENT;
		break;
	}

	mutex_unlock(&port->lock);
	return ret;
}

/** Stop waiting for a port event.
 * @param handle	Handle to port.
 * @param event		Event being waited for.
 * @param sync		Internal data pointer. */
static void port_object_unwait(object_handle_t *handle, int event, void *sync) {
	ipc_port_t *port = (ipc_port_t *)handle->object;

	switch(event) {
	case PORT_EVENT_CONNECTION:
		notifier_unregister(&port->conn_notifier, object_wait_notifier, sync);
		break;
	}
}

/** IPC port object type. */
static object_type_t port_object_type = {
	.id = OBJECT_TYPE_PORT,
	.close = port_object_close,
	.wait = port_object_wait,
	.unwait = port_object_unwait,
};

/** Closes a handle to a connection.
 * @param handle	Handle being closed. */
static void connection_object_close(object_handle_t *handle) {
	ipc_connection_t *conn = (ipc_connection_t *)handle->object;
	ipc_endpoint_t *endpoint = handle->data;
	ipc_message_t *message;
	status_t ret;

	assert(endpoint->conn == conn);

	mutex_lock(&conn->lock);

	/* If the remote is open, detach it from this end, and wake all threads
	 * waiting for space on this end or messages on the remote end. They
	 * will detect that we have set remote to NULL and return an error. */
	if(endpoint->remote) {
		waitq_wake_all(&endpoint->space_sem.queue);
		waitq_wake_all(&endpoint->remote->data_sem.queue);
		notifier_run(&endpoint->remote->hangup_notifier, NULL, false);
		endpoint->remote->remote = NULL;
		endpoint->remote = NULL;
	}

	/* Discard all currently queued messages. */
	LIST_FOREACH_SAFE(&endpoint->messages, iter) {
		message = list_entry(iter, ipc_message_t, header);

		/* We must change the semaphores even though the endpoint is
		 * being freed as they are initialised in the slab constructor
		 * rather than after being allocated. */
		ret = semaphore_down_etc(&endpoint->data_sem, 0, 0);
		assert(ret == STATUS_SUCCESS);
		semaphore_up(&endpoint->space_sem, 1);

		list_remove(&message->header);
		kfree(message);
	}

	assert(semaphore_count(&endpoint->data_sem) == 0);
	assert(semaphore_count(&endpoint->space_sem) == IPC_QUEUE_MAX);
	assert(notifier_empty(&endpoint->msg_notifier));
	assert(notifier_empty(&endpoint->hangup_notifier));

	dprintf("ipc: destroyed endpoint %p (conn: %p, port: %d)\n", endpoint,
	        conn, (conn->port) ? conn->port->id : -1);
	mutex_unlock(&conn->lock);

	/* Free the connection if necessary. */
	if(refcount_dec(&conn->count) == 0) {
		/* This is a bit crap: take the port tree lock to ensure that
		 * the port isn't closed while detaching the connection from
		 * it. */
		mutex_lock(&port_tree_lock);
		if(conn->port) {
			mutex_lock(&conn->port->lock);
			list_remove(&conn->header);
			mutex_unlock(&conn->port->lock);
		}
		mutex_unlock(&port_tree_lock);

		dprintf("ipc: destroyed connection %p (port: %d)\n", conn,
		        (conn->port) ? conn->port->id : -1);
		object_destroy(&conn->obj);
		slab_cache_free(ipc_connection_cache, conn);
	}
}

/** Signal that a connection event is being waited for.
 * @param handle	Handle to connection.
 * @param event		Event being waited for.
 * @param sync		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t connection_object_wait(object_handle_t *handle, int event, void *sync) {
	ipc_connection_t *conn = (ipc_connection_t *)handle->object;
	ipc_endpoint_t *endpoint = handle->data;
	status_t ret = STATUS_SUCCESS;

	mutex_lock(&conn->lock);

	switch(event) {
	case CONNECTION_EVENT_HANGUP:
		if(!endpoint->remote) {
			object_wait_signal(sync);
		} else {
			notifier_register(&endpoint->hangup_notifier, object_wait_notifier, sync);
		}
		break;
	case CONNECTION_EVENT_MESSAGE:
		if(semaphore_count(&endpoint->data_sem)) {
			object_wait_signal(sync);
		} else {
			notifier_register(&endpoint->msg_notifier, object_wait_notifier, sync);
		}
		break;

	default:
		ret = STATUS_INVALID_EVENT;
		break;
	}

	mutex_unlock(&conn->lock);
	return ret;
}

/** Stop waiting for a connection event.
 * @param handle	Handle to connection.
 * @param event		Event being waited for.
 * @param sync		Internal data pointer. */
static void connection_object_unwait(object_handle_t *handle, int event, void *sync) {
	ipc_endpoint_t *endpoint = handle->data;

	switch(event) {
	case CONNECTION_EVENT_HANGUP:
		notifier_unregister(&endpoint->hangup_notifier, object_wait_notifier, sync);
		break;
	case CONNECTION_EVENT_MESSAGE:
		notifier_unregister(&endpoint->msg_notifier, object_wait_notifier, sync);
		break;
	}
}

/** IPC connection object type. */
static object_type_t connection_object_type = {
	.id = OBJECT_TYPE_CONNECTION,
	.close = connection_object_close,
	.wait = connection_object_wait,
	.unwait = connection_object_unwait,
};

/** Create a new IPC port.
 * @todo		Change default attributes to what is said below, need a
 *			change to the service manager.
 * @param security	Security attributes for the object. If NULL, default
 *			security attributes will be used which sets the owning
 *			user and group to that of the calling process, grants
 *			listen access to nobody and allows connections from
 *			anyone. Note that when creating objects you can specify
 *			rights for the handle that the ACL doesn't grant to you.
 *			This means that you can still request listen access here
 *			here even though the default ACL disallows it.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to port.
 * @return		Status code describing result of the operation. */
status_t kern_port_create(const object_security_t *security, object_rights_t rights,
                          handle_t *handlep) {
	object_security_t ksecurity = { -1, -1, NULL };
	ipc_port_t *port;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	if(security) {
		ret = object_security_from_user(&ksecurity, security, true);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	/* Construct a default ACL if required. */
	if(!ksecurity.acl) {
		ksecurity.acl = kmalloc(sizeof(*ksecurity.acl), MM_SLEEP);
		object_acl_init(ksecurity.acl);
		object_acl_add_entry(ksecurity.acl, ACL_ENTRY_USER, -1,
		                     PORT_RIGHT_LISTEN | PORT_RIGHT_CONNECT);
		object_acl_add_entry(ksecurity.acl, ACL_ENTRY_OTHERS, 0,
		                     PORT_RIGHT_CONNECT);
	}

	port = slab_cache_alloc(ipc_port_cache, MM_SLEEP);
	port->id = id_alloc_get(&port_id_allocator);
	if(port->id < 0) {
		slab_cache_free(ipc_port_cache, port);
		object_security_destroy(&ksecurity);
		return STATUS_NO_PORTS;
	}

	object_init(&port->obj, &port_object_type, &ksecurity, NULL);
	object_security_destroy(&ksecurity);
	refcount_set(&port->count, 1);

	mutex_lock(&port_tree_lock);
	avl_tree_insert(&port_tree, &port->tree_link, port->id, port);
	dprintf("ipc: created port %d(%p) (process: %d)\n", port->id, port, curr_proc->id);
	mutex_unlock(&port_tree_lock);

	ret = object_handle_create(&port->obj, NULL, rights, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		ipc_port_release(port);
	}
	return ret;
}

/** Open a handle to an IPC port.
 * @param id		ID of the port to open.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to port.
 * @return		Status code describing result of the operation. */
status_t kern_port_open(port_id_t id, object_rights_t rights, handle_t *handlep) {
	ipc_port_t *port;
	status_t ret;

	if(!rights || !handlep) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&port_tree_lock);

	port = avl_tree_lookup(&port_tree, id);
	if(!port) {
		mutex_unlock(&port_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&port->count);
	mutex_unlock(&port_tree_lock);

	ret = object_handle_open(&port->obj, NULL, rights, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		ipc_port_release(port);
	}
	return ret;
}

/** Get the ID of a port.
 * @param handle	Handle to port to get ID of.
 * @return		ID of port on success, -1 if handle is invalid. */
port_id_t kern_port_id(handle_t handle) {
	object_handle_t *khandle;
	ipc_port_t *port;
	port_id_t ret;

	if(object_handle_lookup(handle, OBJECT_TYPE_PORT, 0, &khandle) != STATUS_SUCCESS) {
		return -1;
	}

	port = (ipc_port_t *)khandle->object;
	ret = port->id;
	object_handle_release(khandle);
	return ret;
}

/** Wait for a connection attempt on a port.
 * @param handle	Handle to port to listen on.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if nothing is currently attempting to
 *			connect to the port. If -1, the function will block
 *			indefinitely until a connection is made.
 * @param connp		Where to store handle to caller's end of the connection.
 * @param infop		Where to store information about the process that made
 *			the conection (can be NULL).
 * @return		Status code describing result of the operation. */
status_t kern_port_listen(handle_t handle, useconds_t timeout, handle_t *connp, port_client_t *infop) {
	ipc_connection_t *conn = NULL;
	object_handle_t *khandle;
	ipc_port_t *port;
	status_t ret;

	if(!connp) {
		return STATUS_INVALID_ARG;
	}

	ret = object_handle_lookup(handle, OBJECT_TYPE_PORT, PORT_RIGHT_LISTEN, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	port = (ipc_port_t *)khandle->object;

	/* Try to get a connection. FIXME: This does not handle timeout
	 * properly - implement SYNC_ABSOLUTE. */
	while(!conn) {
		ret = semaphore_down_etc(&port->conn_sem, timeout, SYNC_INTERRUPTIBLE);
		if(ret != STATUS_SUCCESS) {
			object_handle_release(khandle);
			return ret;
		}

		mutex_lock(&port->lock);
		if(!list_empty(&port->waiting)) {
			conn = list_first(&port->waiting, ipc_connection_t, header);
			break;
		}
		mutex_unlock(&port->lock);
	}

	/* Copy back information about the connection. */
	if(infop) {
		ret = memcpy_to_user(infop, &conn->sync->info, sizeof(*infop));
		if(ret != STATUS_SUCCESS) {
			semaphore_up(&port->conn_sem, 1);
			mutex_unlock(&port->lock);
			object_handle_release(khandle);
			return ret;
		}
	}

	/* Create a handle to the endpoint. */
	refcount_inc(&conn->count);
	ret = object_handle_create(&conn->obj, &conn->endpoints[SERVER_ENDPOINT],
	                           0, NULL, 0, NULL, NULL, connp);
	if(ret != STATUS_SUCCESS) {
		refcount_dec(&conn->count);
		semaphore_up(&port->conn_sem, 1);
		mutex_unlock(&port->lock);
		object_handle_release(khandle);
		return ret;
	}

	list_append(&port->connections, &conn->header);

	/* Wake the thread that made the connection. */
	semaphore_up(&conn->sync->sem, 1);
	conn->sync = NULL;
	mutex_unlock(&port->lock);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Create an IPC connection object.
 * @param port		Port that the connection is for.
 * @param count		Initial reference count.
 * @return		Pointer to connection object. */
static ipc_connection_t *ipc_connection_create(ipc_port_t *port, int count) {
	object_security_t security;
	ipc_connection_t *conn;
	object_acl_t acl;

	/* Create security attributes for the object. Since the object is a
	 * private object and can't actually be opened anywhere else, we just
	 * give it an empty ACL and don't ever do any access checks on the
	 * handle. */
	object_acl_init(&acl);
	security.uid = -1;
	security.gid = -1;
	security.acl = &acl;

	/* Create a connection structure. */
	conn = slab_cache_alloc(ipc_connection_cache, MM_SLEEP);
	object_init(&conn->obj, &connection_object_type, &security, NULL);
	refcount_set(&conn->count, count);
	conn->port = port;
	conn->endpoints[0].conn = conn;
	conn->endpoints[0].remote = &conn->endpoints[1];
	conn->endpoints[1].conn = conn;
	conn->endpoints[1].remote = &conn->endpoints[0];
	return conn;
}

/**
 * Open a connection to an already open IPC port.
 *
 * Opens a connection to an IPC port that the caller has a handle open to. This
 * allows, for example, a server to open a connection back to itself.
 *
 * @param handle	Handle to port (must have the PORT_RIGHT_LISTEN and
 *			PORT_RIGHT_CONNECT access rights).
 * @param connp		Where to store handles to each end of the connection.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_port_loopback(handle_t handle, handle_t connp[2]) {
	object_handle_t *khandle;
	ipc_connection_t *conn;
	handle_t kconn[2];
	ipc_port_t *port;
	status_t ret;
	int i;

	if(!connp) {
		return STATUS_INVALID_ARG;
	}

	ret = object_handle_lookup(handle, OBJECT_TYPE_PORT, PORT_RIGHT_LISTEN | PORT_RIGHT_CONNECT,
	                           &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	port = (ipc_port_t *)khandle->object;
	mutex_lock(&port->lock);

	/* Create a connection object. */
	conn = ipc_connection_create(port, 0);

	/* Create handles to each end of the connection. */
	for(i = 0; i < 2; i++) {
		refcount_inc(&conn->count);

		ret = object_handle_create(&conn->obj, &conn->endpoints[i], 0, NULL,
	                                   0, NULL, &kconn[i], &connp[i]);
		if(ret != STATUS_SUCCESS) {
			if(refcount_dec(&conn->count)) {
				object_handle_detach(NULL, kconn[0]);
			} else {
				object_destroy(&conn->obj);
				slab_cache_free(ipc_connection_cache, conn);
				mutex_unlock(&port->lock);
				object_handle_release(khandle);
			}
			return ret;
		}
	}

	list_append(&port->connections, &conn->header);
	mutex_unlock(&port->lock);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Open a connection to an IPC port.
 * @param id		Port ID to connect to.
 * @param handlep	Where to store handle to caller's end of the connection.
 * @return		Status code describing result of the operation. */
status_t kern_connection_open(port_id_t id, handle_t *handlep) {
	ipc_connect_sync_t sync;
	ipc_connection_t *conn;
	ipc_port_t *port;
	handle_t handle;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&port_tree_lock);

	port = avl_tree_lookup(&port_tree, id);
	if(!port) {
		mutex_unlock(&port_tree_lock);
		return STATUS_NOT_FOUND;
	} else if(!(object_rights(&port->obj, curr_proc) & PORT_RIGHT_CONNECT)) {
		mutex_unlock(&port_tree_lock);
		return STATUS_ACCESS_DENIED;
	}

	mutex_lock(&port->lock);
	mutex_unlock(&port_tree_lock);

	/* Create the connection object. */
	conn = ipc_connection_create(port, 1);

	/* Fill in the synchronisation structure. */
	semaphore_init(&sync.sem, "ipc_connect_sem", 0);
	sync.info.pid = curr_proc->id;
	sync.info.sid = curr_proc->session->id;
	conn->sync = &sync;

	/* Create a handle now, as we do not want to find that we cannot create
	 * the handle after the connection has been accepted. */
	ret = object_handle_create(&conn->obj, &conn->endpoints[CLIENT_ENDPOINT],
	                           0, NULL, 0, NULL, &handle, handlep);
	if(ret != STATUS_SUCCESS) {
		object_destroy(&conn->obj);
		slab_cache_free(ipc_connection_cache, conn);
		mutex_unlock(&port->lock);
		return ret;
	}

	/* Place the connection in the port's waiting list. */
	list_append(&port->waiting, &conn->header);
	semaphore_up(&port->conn_sem, 1);
	notifier_run(&port->conn_notifier, NULL, false);
	mutex_unlock(&port->lock);

	/* Wait for the connection to be accepted. */
	ret = semaphore_down_etc(&sync.sem, -1, SYNC_INTERRUPTIBLE);
	if(ret != STATUS_SUCCESS) {
		/* Take the port tree lock to ensure that the port doesn't get
		 * freed. This is a bit naff, but oh well. */
		mutex_lock(&port_tree_lock);
		if(conn->port) {
			mutex_lock(&conn->port->lock);
			list_remove(&conn->header);
			mutex_unlock(&conn->port->lock);
			conn->port = NULL;
		}
		mutex_unlock(&port_tree_lock);
		object_handle_detach(NULL, handle);
		return ret;
	} else if(!conn->port) {
		object_handle_detach(NULL, handle);
		return STATUS_NOT_FOUND;
	}

	dprintf("ipc: opened connection to port %" PRId32 "(%p) (client: %p, server: %p)\n",
	        id, conn->port, &conn->endpoints[CLIENT_ENDPOINT],
	        &conn->endpoints[SERVER_ENDPOINT]);
	return STATUS_SUCCESS;
}

/**
 * Send a message on an IPC connection.
 *
 * Queues a message at the other end of a connection. Messages are sent
 * asynchronously. This function can block if the recipient's message queue is
 * full.
 *
 * @param handle	Handle to connection.
 * @param type		Type of message.
 * @param buf		Message data buffer. Does not need to be specified if
 *			size is 0.
 * @param size		Size of data buffer (can be 0).
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_connection_send(handle_t handle, uint32_t type, const void *buf, size_t size) {
	object_handle_t *khandle = NULL;
	ipc_endpoint_t *endpoint = NULL;
	ipc_message_t *message;
	status_t ret;
	bool state;

	if((!buf && size) || size > IPC_MESSAGE_MAX) {
		return STATUS_INVALID_ARG;
	}

	/* Allocate a message structure, and copy the data buffer into it. */
	message = kmalloc(sizeof(ipc_message_t) + size, MM_SLEEP);
	list_init(&message->header);
	message->type = type;
	message->size = size;
	if(size) {
		ret = memcpy_from_user(message->data, buf, size);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Look up the handle. */
	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}
	endpoint = khandle->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for space in the remote message queue. The unlock/wait needs to
	 * be atomic in order to interact properly with connection_object_close().
	 * FIXME: Should integrate this in the semaphore API. */
	if(endpoint->remote) {
		state = waitq_sleep_prepare(&endpoint->remote->space_sem.queue);
		if(endpoint->remote->space_sem.count) {
			--endpoint->remote->space_sem.count;
			waitq_sleep_cancel(&endpoint->remote->space_sem.queue, state);
		} else {
			mutex_unlock(&endpoint->conn->lock);
			ret = waitq_sleep_unsafe(&endpoint->remote->space_sem.queue, -1, SYNC_INTERRUPTIBLE, state);
			mutex_lock(&endpoint->conn->lock);
			if(ret != STATUS_SUCCESS) {
				goto fail;
			}
		}
	}

	/* If remote is now NULL the remote process has hung up or the port
	 * has disappeared */
	if(!endpoint->remote) {
		ret = STATUS_DEST_UNREACHABLE;
		goto fail;
	}

	/* Queue the message. */
	list_append(&endpoint->remote->messages, &message->header);
	semaphore_up(&endpoint->remote->data_sem, 1);
	notifier_run(&endpoint->remote->msg_notifier, NULL, false);

	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
fail:
	if(khandle) {
		mutex_unlock(&endpoint->conn->lock);
		object_handle_release(khandle);
	}
	kfree(message);
	return ret;
}

/** Wait until a message arrives.
 * @param endpoint	Endpoint to wait on. Connection should be locked.
 * @param timeout	Timeout.
 * @param messagep	Where to store pointer to message structure.
 * @return		Status code describing result of the operation. */
static status_t wait_for_message(ipc_endpoint_t *endpoint, useconds_t timeout, ipc_message_t **messagep) {
	status_t ret;
	bool state;

	/* Check if anything can send us a message. */
	if(!endpoint->remote) {
		return STATUS_DEST_UNREACHABLE;
	}

	/* Wait for data in our message queue. The unlock/wait needs to be
	 * atomic in order to interact properly with connection_object_close().
	 * FIXME: Integrate this in semaphore API. */
	state = waitq_sleep_prepare(&endpoint->data_sem.queue);
	if(endpoint->data_sem.count) {
		--endpoint->data_sem.count;
		waitq_sleep_cancel(&endpoint->data_sem.queue, state);
	} else {
		mutex_unlock(&endpoint->conn->lock);
		ret = waitq_sleep_unsafe(&endpoint->data_sem.queue, timeout, SYNC_INTERRUPTIBLE, state);
		mutex_lock(&endpoint->conn->lock);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	/* Recheck that we have a remote end, as it may have hung up. If there
	 * is a message in this case we must re-up the semaphore. */
	if(!endpoint->remote) {
		if(!list_empty(&endpoint->messages)) {
			semaphore_up(&endpoint->data_sem, 1);
		}
		return STATUS_DEST_UNREACHABLE;
	}

	assert(!list_empty(&endpoint->messages));
	*messagep = list_first(&endpoint->messages, ipc_message_t, header);
	return STATUS_SUCCESS;
}

/**
 * Get details of the next message on a connection.
 *
 * Waits until a message arrives on a connection, and then returns the type and
 * size of the message, leaving the message on the queue.
 *
 * @param handle	Handle to connection.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no messages are queued to the caller.
 *			If -1, the function will block indefinitely until a
 *			message is received.
 * @param typep		Where to store message type ID.
 * @param sizep		Where to store message data size.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_connection_peek(handle_t handle, useconds_t timeout, uint32_t *typep, size_t *sizep) {
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	ipc_message_t *message;
	status_t ret;

	/* Look up the handle. */
	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	endpoint = khandle->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for a message. */
	ret = wait_for_message(endpoint, timeout, &message);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&endpoint->conn->lock);
		object_handle_release(khandle);
		return ret;
	}

	/* Copy requested information back to the caller. */
	if(typep) {
		ret = memcpy_to_user(typep, &message->type, sizeof(uint32_t));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
	if(sizep) {
		ret = memcpy_to_user(sizep, &message->size, sizeof(size_t));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}
out:
	semaphore_up(&endpoint->data_sem, 1);
	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return ret;
}

/**
 * Receive a message from a connection.
 *
 * Waits until a message arrives on a connection and then copies it's data into
 * the supplied buffers.
 *
 * Note that if the message being received is larger than the provided buffer,
 * the extra data will be discarded. This behaviour can be exploited to discard
 * an unwanted message, by giving a zero size.

 * @param handle	Handle to connection.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no messages are queued to the caller.
 *			If -1, the function will block indefinitely until a
 *			message is received.
 * @param typep		Where to store message type ID (can be NULL).
 * @param buf		Buffer to copy message data to (can be NULL if size is
 *			specified as 0).
 * @param size		Size of supplied buffer.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_connection_receive(handle_t handle, useconds_t timeout, uint32_t *typep,
                                 void *buf, size_t size) {
	ipc_message_t *message = NULL;
	object_handle_t *khandle;
	ipc_endpoint_t *endpoint;
	status_t ret;

	if(size > 0 && !buf) {
		return STATUS_INVALID_ARG;
	}

	/* Look up the handle. */
	ret = object_handle_lookup(handle, OBJECT_TYPE_CONNECTION, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	endpoint = khandle->data;
	mutex_lock(&endpoint->conn->lock);

	/* Wait for a message. */
	ret = wait_for_message(endpoint, timeout, &message);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	/* Copy the message back to the caller. */
	if(typep) {
		ret = memcpy_to_user(typep, &message->type, sizeof(uint32_t));
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}
	if(size && message->size) {
		ret = memcpy_to_user(buf, message->data, MIN(size, message->size));
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Remove the message from the queue. */
	list_remove(&message->header);
	kfree(message);
	semaphore_up(&endpoint->space_sem, 1);

	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
fail:
	if(message) {
		semaphore_up(&endpoint->data_sem, 1);
	}
	mutex_unlock(&endpoint->conn->lock);
	object_handle_release(khandle);
	return ret;
}

/** Print information about IPC ports.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_port(int argc, char **argv, kdb_filter_t *filter) {
	ipc_connection_t *conn;
	ipc_port_t *port;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<port ID>]\n\n", argv[0]);

		kdb_printf("Prints either a list of all IPC ports or information about a certain port.\n");
		return KDB_SUCCESS;
	} else if(argc == 1) {
		kdb_printf("ID    Count  Waiting\n");
		kdb_printf("==    =====  =======\n");

		AVL_TREE_FOREACH(&port_tree, iter) {
			port = avl_tree_entry(iter, ipc_port_t);

			kdb_printf("%-5" PRIu32 " %-6d %u\n", port->id,
			           refcount_get(&port->count),
			           semaphore_count(&port->conn_sem));
		}

		return KDB_SUCCESS;
	} else if(argc == 2) {
		if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS) {
			return KDB_FAILURE;
		} else if(!(port = avl_tree_lookup(&port_tree, val))) {
			kdb_printf("Invalid port ID.\n");
			return KDB_FAILURE;
		}

		kdb_printf("Port %p(%d)\n", port, port->id);
		kdb_printf("=================================================\n");

		kdb_printf("Locked:  %d (%" PRId32 ")\n", atomic_get(&port->lock.locked),
		        (port->lock.holder) ? port->lock.holder->id : -1);
		kdb_printf("Count:   %d\n", refcount_get(&port->count));
		kdb_printf("Waiting (%u):\n", semaphore_count(&port->conn_sem));
		LIST_FOREACH(&port->waiting, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kdb_printf(" %p: endpoint[0] = %p endpoint[1] = %p\n",
			           conn, &conn->endpoints[0], &conn->endpoints[1]);
		}
		kdb_printf("Connections:\n");
		LIST_FOREACH(&port->connections, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kdb_printf(" %p: endpoint[0] = %p endpoint[1] = %p\n",
			           conn, &conn->endpoints[0], &conn->endpoints[1]);
		}
		return KDB_SUCCESS;
	} else {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}
}

/** Print information about an IPC endpoint.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_endpoint(int argc, char **argv, kdb_filter_t *filter) {
	ipc_endpoint_t *endpoint;
	ipc_message_t *message;
	uint64_t val;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <addr>\n\n", argv[0]);

		kdb_printf("Shows information about an IPC endpoint. The address can be obtained by\n");
		kdb_printf("looking at the data field of an IPC handle.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS) {
		return KDB_FAILURE;
	}
	endpoint = (ipc_endpoint_t *)((ptr_t)val);

	kdb_printf("Endpoint %p\n", endpoint);
	kdb_printf("=================================================\n");

	kdb_printf("Locked: %d (%p) (%" PRId32 ")\n", atomic_get(&endpoint->conn->lock.locked),
	        (endpoint->conn->lock.holder) ? endpoint->conn->lock.holder->id : -1);
	kdb_printf("Space:  %u\n", semaphore_count(&endpoint->space_sem));
	kdb_printf("Data:   %u\n", semaphore_count(&endpoint->data_sem));
	kdb_printf("Remote: %p\n\n", endpoint->remote);

	kdb_printf("Messages:\n");
	LIST_FOREACH(&endpoint->messages, iter) {
		message = list_entry(iter, ipc_message_t, header);

		kdb_printf(" %p: type %" PRIu32 ", size: %zu, buffer: %p\n",
		           message, message->type, message->size, message->data);
	}

	return KDB_SUCCESS;
}

/** Initialise the IPC slab caches. */
static __init_text void ipc_init(void) {
	/* Initialise the port ID allocator. */
	id_alloc_init(&port_id_allocator, 65535);

	/* Create the IPC structure caches. */
	ipc_port_cache = slab_cache_create("ipc_port_cache", sizeof(ipc_port_t), 0,
	                                   ipc_port_ctor, NULL, NULL, 0,
	                                   MM_FATAL);
	ipc_connection_cache = slab_cache_create("ipc_connection_cache", sizeof(ipc_connection_t),
	                                         0, ipc_connection_ctor, NULL, NULL, 0,
	                                         MM_FATAL);

	/* Register the KDB commands. */
	kdb_register_command("port", "Obtain information about IPC ports.", kdb_cmd_port);
	kdb_register_command("endpoint", "Print information about an IPC endpoint.", kdb_cmd_endpoint);
}
INITCALL(ipc_init);
