/* Kiwi IPC system
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
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
 */

#include <console/kprintf.h>

#include <ipc/ipc.h>

#include <lib/notifier.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vmem.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/sched.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <types/avl.h>
#include <types/refcount.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <kdbg.h>

#if CONFIG_IPC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

struct ipc_connection;

/** IPC port ACL entry structure. */
typedef struct ipc_port_acl_entry {
	list_t header;			/**< Link to port ACL. */
	ipc_port_accessor_t type;	/**< What this entry applies to. */
	struct process *process;	/**< Process for process entries. */
	uint32_t rights;		/**< Bitmap of rights that this accessor has. */
} ipc_port_acl_entry_t;

/** IPC port structure. */
typedef struct ipc_port {
	mutex_t lock;			/**< Lock to protect data in structure. */
	identifier_t id;		/**< ID of the port. */
	refcount_t count;		/**< Number of handles open to the port. */
	list_t acl;			/**< Access Control List. */

	list_t connections;		/**< List of currently open connections. */
	list_t waiting;			/**< List of in-progress connection attempts. */
	semaphore_t conn_sem;		/**< IPC connection semaphore. */
	notifier_t conn_notifier;	/**< Notifier for connection attempts. */
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
	list_t header;			/**< Link to port connection list. */

	mutex_t lock;			/**< Lock covering connection. */
	ipc_port_t *port;		/**< Port that the connection is on. */
	refcount_t count;		/**< Handles to either end of the connection. */
	ipc_endpoint_t client;		/**< Endpoint of process that opened. */
	ipc_endpoint_t server;		/**< Endpoint of process that received. */

	semaphore_t *sem;		/**< Pointer to semaphore used during connection setup. */
} ipc_connection_t;

/** In-kernel IPC message structure. */
typedef struct ipc_message {
	list_t header;			/**< Link to message queue. */
	uint32_t type;			/**< Type of message. */
	size_t size;			/**< Size of message data. */
	char data[];			/**< Message data. */
} ipc_message_t;

/** Cache for port/connection structures. */
static slab_cache_t *ipc_port_cache;
static slab_cache_t *ipc_connection_cache;

/** Vmem arena for port ID allocations. */
static vmem_t *ipc_port_id_arena;

/** Tree of all open ports. */
static AVL_TREE_DECLARE(ipc_port_tree);
static MUTEX_DECLARE(ipc_port_tree_lock, 0);

#if 0
# pragma mark Object cache functions.
#endif

/** Port object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int ipc_port_cache_ctor(void *obj, void *data, int mmflag) {
	ipc_port_t *port = obj;

	mutex_init(&port->lock, "ipc_port_lock", 0);
	refcount_set(&port->count, 0);
	list_init(&port->acl);
	list_init(&port->connections);
	list_init(&port->waiting);
	semaphore_init(&port->conn_sem, "ipc_port_conn", 0);
	notifier_init(&port->conn_notifier, port);
	return 0;
}

/** Initialise an endpoint structure.
 * @param endpoint	Endpoint to initialise. */
static void ipc_endpoint_init(ipc_endpoint_t *endpoint) {
	list_init(&endpoint->messages);
	semaphore_init(&endpoint->space_sem, "ipc_endpoint_space", IPC_QUEUE_MAX);
	semaphore_init(&endpoint->data_sem, "ipc_endpoint_data", 0);
	notifier_init(&endpoint->msg_notifier, endpoint);
	notifier_init(&endpoint->hangup_notifier, endpoint);
}

/** Connection object constructor.
 * @param obj		Object to construct.
 * @param data		Cache data (unused).
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int ipc_connection_cache_ctor(void *obj, void *data, int mmflag) {
	ipc_connection_t *conn = obj;

	list_init(&conn->header);
	mutex_init(&conn->lock, "ipc_connection_lock", 0);
	ipc_endpoint_init(&conn->client);
	ipc_endpoint_init(&conn->server);
	return 0;
}

#if 0
# pragma mark ACL functions.
#endif

/** Callback function for process death.
 * @param _process	Process that has died.
 * @param arg2		Unused.
 * @param _port		Port that has an ACL entry for the process. */
static void ipc_process_death_notifier(void *_process, void *arg2, void *_port) {
	process_t *process = _process;
	ipc_port_acl_entry_t *entry;
	ipc_port_t *port = _port;

	mutex_lock(&port->lock, 0);

	LIST_FOREACH(&port->acl, iter) {
		entry = list_entry(iter, ipc_port_acl_entry_t, header);

		if(entry->type == IPC_PORT_ACCESSOR_PROCESS && entry->process == process) {
			list_remove(&entry->header);
			kfree(entry);

			mutex_unlock(&port->lock);
			return;
		}
	}

	fatal("Death notifier called for %d which isn't on ACL", process->id);
}

/** Check if the current process has a right on a port.
 * @param port		Port to check (should be locked).
 * @param right		Right to check for.
 * @return		Whether the process has the right. */
static bool ipc_port_acl_check(ipc_port_t *port, uint32_t right) {
	ipc_port_acl_entry_t *entry;

	LIST_FOREACH(&port->acl, iter) {
		entry = list_entry(iter, ipc_port_acl_entry_t, header);

		if(entry->rights & right) {
			switch(entry->type) {
			case IPC_PORT_ACCESSOR_ALL:
				return true;
			case IPC_PORT_ACCESSOR_PROCESS:
				if(entry->process == curr_proc) {
					return true;
				}
				break;
			}
		}
	}

	return false;
}

#if 0
# pragma mark Port handle functions.
#endif

/** Signal that a port handle event is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_port_handle_wait(handle_wait_t *wait) {
	ipc_port_t *port = wait->info->data;
	int ret = 0;

	mutex_lock(&port->lock, 0);

	switch(wait->event) {
	case IPC_PORT_EVENT_CONNECTION:
		if(port->conn_sem.queue.missed) {
			wait->cb(wait);
		} else {
			notifier_register(&port->conn_notifier, handle_wait_notifier, wait);
		}
		break;
	default:
		ret = -ERR_PARAM_INVAL;
		break;
	}

	mutex_unlock(&port->lock);
	return ret;
}

/** Stop waiting for a port handle event.
 * @param wait		Wait information structure. */
static void ipc_port_handle_unwait(handle_wait_t *wait) {
	ipc_port_t *port = wait->info->data;

	switch(wait->event) {
	case IPC_PORT_EVENT_CONNECTION:
		notifier_unregister(&port->conn_notifier, handle_wait_notifier, wait);
		break;
	}
}

/** Closes a handle to a port.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_port_handle_close(handle_info_t *info) {
	ipc_port_t *port = info->data;
	ipc_port_acl_entry_t *entry;
	ipc_connection_t *conn;

	if(refcount_dec(&port->count) > 0) {
		return 0;
	}

	mutex_lock(&ipc_port_tree_lock, 0);
	mutex_lock(&port->lock, 0);

	/* Cancel all in-progress connection attempts. */
	LIST_FOREACH(&port->waiting, iter) {
		conn = list_entry(iter, ipc_connection_t, header);

		list_remove(&conn->header);
		conn->port = NULL;
		semaphore_up(conn->sem, 1);
	}

	/* Terminate all currently open connections. We do this by
	 * disconnecting both ends of the connection from each other. */
	LIST_FOREACH_SAFE(&port->connections, iter) {
		conn = list_entry(iter, ipc_connection_t, header);

		mutex_lock(&conn->lock, 0);

		waitq_wake(&conn->client.space_sem.queue, true);
		waitq_wake(&conn->client.data_sem.queue, true);
		notifier_run(&conn->client.hangup_notifier, NULL, false);
		conn->client.remote = NULL;

		waitq_wake(&conn->server.space_sem.queue, true);
		waitq_wake(&conn->server.data_sem.queue, true);
		notifier_run(&conn->server.hangup_notifier, NULL, false);
		conn->server.remote = NULL;

		list_remove(&conn->header);
		conn->port = NULL;

		mutex_unlock(&conn->lock);
	}

	/* Clear up ACL entries. */
	LIST_FOREACH_SAFE(&port->acl, iter) {
		entry = list_entry(iter, ipc_port_acl_entry_t, header);

		if(entry->type == IPC_PORT_ACCESSOR_PROCESS) {
			notifier_unregister(&entry->process->death_notifier, ipc_process_death_notifier, port);
		}
		list_remove(&entry->header);
		kfree(entry);
	}

	avl_tree_remove(&ipc_port_tree, port->id);
	mutex_unlock(&ipc_port_tree_lock);
	mutex_unlock(&port->lock);

	dprintf("ipc: destroyed port %d (%p)\n", port->id, port);
	vmem_free(ipc_port_id_arena, (vmem_resource_t)port->id, 1);
	slab_cache_free(ipc_port_cache, port);
	return 0;
}

/** IPC port handle operations. */
static handle_type_t ipc_port_handle_type = {
	.id = HANDLE_TYPE_PORT,
	.wait = ipc_port_handle_wait,
	.unwait = ipc_port_handle_unwait,
	.close = ipc_port_handle_close,
};

#if 0
# pragma mark Connection handle functions.
#endif

/** Signal that a connection handle event is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_connection_handle_wait(handle_wait_t *wait) {
	ipc_endpoint_t *endpoint = wait->info->data;
	int ret = 0;

	mutex_lock(&endpoint->conn->lock, 0);

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		if(endpoint->data_sem.queue.missed) {
			wait->cb(wait);
		} else {
			notifier_register(&endpoint->msg_notifier, handle_wait_notifier, wait);
		}
		break;
	case IPC_CONNECTION_EVENT_HANGUP:
		if(!endpoint->remote) {
			wait->cb(wait);
		} else {
			notifier_register(&endpoint->hangup_notifier, handle_wait_notifier, wait);
		}
		break;
	default:
		ret = -ERR_PARAM_INVAL;
		break;
	}

	mutex_unlock(&endpoint->conn->lock);
	return ret;
}

/** Stop waiting for a connection handle event.
 * @param wait		Wait information structure. */
static void ipc_connection_handle_unwait(handle_wait_t *wait) {
	ipc_endpoint_t *endpoint = wait->info->data;

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		notifier_unregister(&endpoint->msg_notifier, handle_wait_notifier, wait);
		break;
	case IPC_CONNECTION_EVENT_HANGUP:
		notifier_unregister(&endpoint->hangup_notifier, handle_wait_notifier, wait);
		break;
	}
}

/** Closes a handle to a connection.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_connection_handle_close(handle_info_t *info) {
	ipc_endpoint_t *endpoint = info->data;
	ipc_message_t *message;
	int ret;

	mutex_lock(&endpoint->conn->lock, 0);

	/* If the remote is open, detach it from this end, and wake all threads
	 * all threads waiting for space on this end or messages on the remote
	 * end. They will detect that we have set remote to NULL and return an
	 * error. */
	if(endpoint->remote) {
		waitq_wake(&endpoint->space_sem.queue, true);
		waitq_wake(&endpoint->remote->data_sem.queue, true);
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
		ret = semaphore_down(&endpoint->data_sem, SYNC_NONBLOCK);
		assert(ret == 0);
		semaphore_up(&endpoint->space_sem, 1);

		list_remove(&message->header);
		kfree(message);
	}

	assert(endpoint->data_sem.queue.missed == 0);
	assert(endpoint->space_sem.queue.missed == IPC_QUEUE_MAX);
	assert(list_empty(&endpoint->msg_notifier.functions));
	assert(list_empty(&endpoint->hangup_notifier.functions));

	dprintf("ipc: destroyed endpoint %p (conn: %p, port: %d)\n", endpoint,
	        endpoint->conn, (endpoint->conn->port) ? endpoint->conn->port->id : -1);

	/* Free the structure if necessary. */
	mutex_unlock(&endpoint->conn->lock);
	if(refcount_dec(&endpoint->conn->count) == 0) {
		dprintf("ipc: destroyed connection %p (port: %d)\n", endpoint->conn,
		        (endpoint->conn->port) ? endpoint->conn->port->id : -1);
		slab_cache_free(ipc_connection_cache, endpoint->conn);
	}
	return 0;
}

/** IPC connection handle operations. */
static handle_type_t ipc_connection_handle_type = {
	.id = HANDLE_TYPE_CONNECTION,
	.wait = ipc_connection_handle_wait,
	.unwait = ipc_connection_handle_unwait,
	.close = ipc_connection_handle_close,
};

#if 0
# param System calls.
#endif

/** Create a new IPC port.
 *
 * Creates a new IPC port and returns a handle to it. The port's ACL will
 * initially have one entry granting full access to the calling process. The
 * port's global ID can be obtained by calling ipc_port_id() on the handle.
 *
 * @return		Handle to the port on success, negative error code on
 *			failure.
 */
handle_t sys_ipc_port_create(void) {
	ipc_port_acl_entry_t *entry;
	ipc_port_t *port;
	handle_t ret;

	port = slab_cache_alloc(ipc_port_cache, MM_SLEEP);
	if(!(port->id = (identifier_t)vmem_alloc(ipc_port_id_arena, 1, 0))) {
		slab_cache_free(ipc_port_cache, port);
		return -ERR_RESOURCE_UNAVAIL;
	}

	/* Create an ACL entry for the port. */
	entry = kmalloc(sizeof(ipc_port_acl_entry_t), MM_SLEEP);
	list_init(&entry->header);
	list_append(&port->acl, &entry->header);
	entry->type = IPC_PORT_ACCESSOR_PROCESS;
	entry->process = curr_proc;
	entry->rights = IPC_PORT_RIGHT_OPEN | IPC_PORT_RIGHT_MODIFY | IPC_PORT_RIGHT_CONNECT;
	notifier_register(&curr_proc->death_notifier, ipc_process_death_notifier, port);

	mutex_lock(&ipc_port_tree_lock, 0);

	if((ret = handle_create(&curr_proc->handles, &ipc_port_handle_type, port)) < 0) {
		vmem_free(ipc_port_id_arena, (vmem_resource_t)port->id, 1);
		slab_cache_free(ipc_port_cache, port);
		kfree(entry);
	} else {
		refcount_set(&port->count, 1);
		avl_tree_insert(&ipc_port_tree, port->id, port, NULL);
	}

	dprintf("ipc: created port %d(%p) (process: %d)\n", port->id, port, curr_proc->id);
	mutex_unlock(&ipc_port_tree_lock);
	return ret;
}

/** Open a handle to an IPC port.
 *
 * Opens a handle to an IPC port which can be used to listen for messages, and,
 * if the port's ACL allows it, modify the ACL. The caller must have the
 * IPC_PORT_RIGHT_OPEN right on the port.
 *
 * @param id		ID of the port to open.
 *
 * @return		Handle to the port on success, negative error code on
 *			failure.
 */
handle_t sys_ipc_port_open(identifier_t id) {
	ipc_port_t *port;
	handle_t ret;

	mutex_lock(&ipc_port_tree_lock, 0);

	if(!(port = avl_tree_lookup(&ipc_port_tree, id))) {
		mutex_unlock(&ipc_port_tree_lock);
		return -ERR_NOT_FOUND;
	}
	mutex_lock(&port->lock, 0);
	mutex_unlock(&ipc_port_tree_lock);

	if(!ipc_port_acl_check(port, IPC_PORT_RIGHT_OPEN)) {
		mutex_unlock(&port->lock);
		return -ERR_PERM_DENIED;
	}

	if((ret = handle_create(&curr_proc->handles, &ipc_port_handle_type, port)) >= 0) {
		refcount_inc(&port->count);
	}
	mutex_unlock(&port->lock);
	return ret;
}

/** Get the ID of a port.
 *
 * Gets the global ID of a port referred to by a handle.
 *
 * @param handle	Handle to port.
 *
 * @return		ID of port on success, negative error code on failure.
 */
identifier_t sys_ipc_port_id(handle_t handle) {
	handle_info_t *info;
	ipc_port_t *port;
	identifier_t ret;

	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_PORT, &info)) != 0) {
		return ret;
	}
	port = info->data;

	ret = port->id;
	handle_release(info);
	return ret;
}

/** Wait for a connection attempt on a port.
 *
 * Waits for a connection attempt to be made on a port.
 *
 * @param handle	Handle to port.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if nothing is currently attempting to
 *			connect to the port. If -1, the function will block
 *			indefinitely until a connection is made.
 *
 * @return		Handle to the caller's end of the connection on
 *			success, negative error code on failure.
 */
handle_t sys_ipc_port_listen(handle_t handle, timeout_t timeout) {
	ipc_connection_t *conn = NULL;
	handle_info_t *info;
	ipc_port_t *port;
	handle_t ret;

	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_PORT, &info)) != 0) {
		return ret;
	}
	port = info->data;

	/* Try to get a connection. FIXME: This does not handle timeout
	 * properly! */
	while(!conn) {
		if((ret = semaphore_down_timeout(&port->conn_sem, timeout, SYNC_INTERRUPTIBLE)) != 0) {
			handle_release(info);
			return ret;
		}

		mutex_lock(&port->lock, 0);
		if(!list_empty(&port->waiting)) {
			conn = list_entry(port->waiting.next, ipc_connection_t, header);
			break;
		}
		mutex_unlock(&port->lock);
	}

	/* Reference the connection to account for the handle we create. */
	refcount_inc(&conn->count);

	/* Create a handle for it. */
	if((ret = handle_create(&curr_proc->handles, &ipc_connection_handle_type, &conn->server)) < 0) {
		refcount_dec(&conn->count);
		semaphore_up(&port->conn_sem, 1);
		mutex_unlock(&port->lock);
		handle_release(info);
		return ret;
	}

	list_append(&port->connections, &conn->header);
	conn->port = port;

	/* Wake the thread that made the connection. */
	semaphore_up(conn->sem, 1);
	mutex_unlock(&port->lock);
	handle_release(info);
	return ret;
}

/** Add rights to a port's ACL.
 *
 * Gives rights to the specified accessor in a port's access control list. The
 * caller must have the IPC_PORT_RIGHT_MODIFY right on the port.
 *
 * @todo		IPC_PORT_ACCESSOR_USER.
 *
 * @param handle	Handle to port.
 * @param type		Type of accessor to add.
 * @param id		Process ID for IPC_PORT_ACCESSOR_PROCESS, ignored for
 *			IPC_PORT_ACCESSOR_ALL.
 * @param rights	Bitmap of rights to give to the specified accessor.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_port_acl_add(handle_t handle, ipc_port_accessor_t type, identifier_t id,
                         uint32_t rights) {
	ipc_port_acl_entry_t *entry;
	process_t *process = NULL;
	handle_info_t *info;
	ipc_port_t *port;
	int ret;

	if(type != IPC_PORT_ACCESSOR_ALL && type != IPC_PORT_ACCESSOR_PROCESS) {
		return -ERR_PARAM_INVAL;
	} else if(rights & ~(IPC_PORT_RIGHT_OPEN | IPC_PORT_RIGHT_MODIFY | IPC_PORT_RIGHT_CONNECT)) {
		return -ERR_PARAM_INVAL;
	}

	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_PORT, &info)) != 0) {
		return ret;
	}
	port = info->data;

	mutex_lock(&port->lock, 0);

	if(!ipc_port_acl_check(port, IPC_PORT_RIGHT_MODIFY)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	}

	/* Get the process to use if necessary. FIXME: Race condition possible,
	 * process could be deleted before we lock it. */
	if(type == IPC_PORT_ACCESSOR_PROCESS) {
		if(!(process = process_lookup(id))) {
			ret = -ERR_NOT_FOUND;
			goto out;
		}
		mutex_lock(&process->lock, 0);
	}

	/* Look for an existing entry to modify. */
	LIST_FOREACH(&port->acl, iter) {
		entry = list_entry(iter, ipc_port_acl_entry_t, header);

		if(entry->type == type && entry->process == process) {
			entry->rights |= rights;
			goto out;
		}
	}

	/* Create a new entry. */
	entry = kmalloc(sizeof(ipc_port_acl_entry_t), MM_SLEEP);
	list_init(&entry->header);
	list_append(&port->acl, &entry->header);
	entry->type = type;
	entry->process = process;
	entry->rights = rights;
	if(process) {
		notifier_register(&process->death_notifier, ipc_process_death_notifier, port);
	}
out:
	if(process) {
		mutex_unlock(&process->lock);
	}
	mutex_unlock(&port->lock);
	handle_release(info);
	return ret;
}

/** Remove rights from a port's ACL.
 *
 * Removes rights from the specified accessor in a port's access control list.
 * The caller must have the IPC_PORT_RIGHT_MODIFY right on the port.
 *
 * @param handle	Handle to port.
 * @param type		Type of accessor.
 * @param id		Process ID for IPC_PORT_ACCESSOR_PROCESS, ignored for
 *			IPC_PORT_ACCESSOR_ALL.
 * @param right		Bitmap of rights to remove from the specified accessor.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_port_acl_remove(handle_t handle, ipc_port_accessor_t type, identifier_t id,
                            uint32_t rights) {
	ipc_port_acl_entry_t *entry;
	process_t *process = NULL;
	handle_info_t *info;
	ipc_port_t *port;
	int ret;

	if(type != IPC_PORT_ACCESSOR_ALL && type != IPC_PORT_ACCESSOR_PROCESS) {
		return -ERR_PARAM_INVAL;
	} else if(rights & ~(IPC_PORT_RIGHT_OPEN | IPC_PORT_RIGHT_MODIFY | IPC_PORT_RIGHT_CONNECT)) {
		return -ERR_PARAM_INVAL;
	}

	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_PORT, &info)) != 0) {
		return ret;
	}
	port = info->data;

	mutex_lock(&port->lock, 0);

	if(!ipc_port_acl_check(port, IPC_PORT_RIGHT_MODIFY)) {
		ret = -ERR_PERM_DENIED;
		goto out;
	}

	/* Get the process to use if necessary. FIXME: Race condition possible,
	 * process could be deleted before we lock it. */
	if(type == IPC_PORT_ACCESSOR_PROCESS) {
		if(!(process = process_lookup(id))) {
			ret = -ERR_NOT_FOUND;
			goto out;
		}
		mutex_lock(&process->lock, 0);
	}

	LIST_FOREACH(&port->acl, iter) {
		entry = list_entry(iter, ipc_port_acl_entry_t, header);

		if(entry->type == type && entry->process == process) {
			entry->rights &= ~rights;
			goto out;
		}
	}

	ret = -ERR_NOT_FOUND;
out:
	if(process) {
		mutex_unlock(&process->lock);
	}
	mutex_unlock(&port->lock);
	handle_release(info);
	return ret;
}

/** Open an IPC connection to a port.
 *
 * Opens an IPC connection to a port. The function will block until either
 * the connection is accepted, or until the timeout expires, in which case it
 * will return an error.
 *
 * @param id		Port ID to connect to.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if a connection is not being listened for.
 *			If -1, the function will block indefinitely until the
 *			connection is accepted.
 *
 * @return		Handle referring to caller's end of connection on
 *			success, negative error code on failure.
 */
handle_t sys_ipc_connection_open(identifier_t id, timeout_t timeout) {
	ipc_connection_t *conn;
	ipc_port_t *port;
	semaphore_t sem;
	handle_t handle;
	int ret;

	/* FIXME: Handle this. I r lazy. */
	if(timeout == 0) {
		return -ERR_NOT_IMPLEMENTED;
	}

	/* Look up the port. */
	mutex_lock(&ipc_port_tree_lock, 0);
	if(!(port = avl_tree_lookup(&ipc_port_tree, id))) {
		return -ERR_NOT_FOUND;
	}
	mutex_lock(&port->lock, 0);
	mutex_unlock(&ipc_port_tree_lock);

	if(!ipc_port_acl_check(port, IPC_PORT_RIGHT_CONNECT)) {
		mutex_unlock(&port->lock);
		return -ERR_PERM_DENIED;
	}

	/* Create a connection structure. */
	conn = slab_cache_alloc(ipc_connection_cache, MM_SLEEP);
	refcount_set(&conn->count, 1);
	conn->client.conn = conn;
	conn->client.remote = &conn->server;
	conn->server.conn = conn;
	conn->server.remote = &conn->client;
	conn->port = port;
	conn->sem = &sem;

	semaphore_init(&sem, "ipc_open_sem", 0);

	/* Create a handle now, as we do not want to find that we cannot create
	 * the handle after the connection has been accepted. */
	if((handle = handle_create(&curr_proc->handles, &ipc_connection_handle_type, &conn->client)) < 0) {
		slab_cache_free(ipc_connection_cache, conn);
		mutex_unlock(&port->lock);
		return handle;
	}

	/* Place the connection in the port's waiting list. */
	list_append(&port->waiting, &conn->header);
	semaphore_up(&port->conn_sem, 1);
	notifier_run(&port->conn_notifier, NULL, false);
	mutex_unlock(&port->lock);

	/* Wait for the connection to be accepted. FIXME: This won't work with
	 * timeout = 0, it doesn't give waiting listens to get here! */
	if((ret = semaphore_down_timeout(&sem, timeout, SYNC_INTERRUPTIBLE)) != 0) {
		/* Take the port tree lock to ensure that the port doesn't get
		 * freed. This is a bit naff, but oh well. */
		mutex_lock(&ipc_port_tree_lock, 0);
		if(conn->port) {
			mutex_lock(&conn->port->lock, 0);
			list_remove(&conn->header);
			mutex_unlock(&conn->port->lock);
		}
		mutex_unlock(&ipc_port_tree_lock);
		return ret;
	} else if(conn->port == NULL) {
		handle_close(&curr_proc->handles, handle);
		return -ERR_NOT_FOUND;
	} else {
		return handle;
	}
}

/** Send a message on a connection.
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
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_send(handle_t handle, uint32_t type, void *buf, size_t size) {
	ipc_endpoint_t *endpoint = NULL;
	handle_info_t *info = NULL;
	ipc_message_t *message;
	int ret;

	if((!buf && size) || size > IPC_MESSAGE_MAX) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a message structure, and copy the data buffer into it. */
	message = kmalloc(sizeof(ipc_message_t) + size, MM_SLEEP);
	list_init(&message->header);
	message->type = type;
	message->size = size;
	if(size) {
		if((ret = memcpy_from_user(message->data, buf, size)) != 0) {
			goto fail;
		}
	}

	/* Look up the IPC handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_CONNECTION, &info)) != 0) {
		goto fail;
	}
	endpoint = info->data;
	mutex_lock(&endpoint->conn->lock, 0);

	/* Wait for space in the remote message queue. The unlock/wait needs to
	 * be atomic in order to interact properly with
	 * ipc_connection_handle_close(). FIXME: Should integrate this in the
	 * semaphore API. */
	if(endpoint->remote) {
		if((ret = waitq_sleep(&endpoint->remote->space_sem.queue, &endpoint->conn->lock,
		                      NULL, -1, SYNC_INTERRUPTIBLE)) != 0) {
			goto fail;
		}
	}

	/* If remote is now NULL the remote process has hung up or the port
	 * has disappeared */
	if(!endpoint->remote) {
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	}

	/* Queue the message. */
	list_append(&endpoint->remote->messages, &message->header);
	semaphore_up(&endpoint->remote->data_sem, 1);
	notifier_run(&endpoint->remote->msg_notifier, NULL, false);

	mutex_unlock(&endpoint->conn->lock);
	handle_release(info);
	return 0;
fail:
	if(info) {
		mutex_unlock(&endpoint->conn->lock);
		handle_release(info);
	}
	kfree(message);
	return ret;
}

/** Send multiple messages on a connection.
 *
 * Queues multiple messages at the other end of a connection. Messages are sent
 * asynchronously. They are queued in the order that they are found in the
 * array. The operation is atomic: the destination will not receive any of the
 * messages until all have been successfully queued, and if a failure occurs,
 * it will receive none of the messages. This function can block if the
 * recipient's message queue is full.
 *
 * @param handle	Handle to connection.
 * @param vec		Array of structures describing messages to send.
 * @param count		Number of messages in array.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_sendv(handle_t handle, ipc_message_vector_t *vec, size_t count) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Receive a message from a connection.
 *
 * Waits for a message to be queued at the caller's end of a connection and
 * returns it. Since the caller may not know the size of the message it is
 * receiving, the function can be called with a NULL buffer pointer. When this
 * is done, the type and size of the message will be returned, but the message
 * will remain queued on the connection. This allows a thread to wait for a
 * message, allocate a buffer large enough to store it, and then receive it. It
 * is highly recommended that this be done, rather than just passing a buffer
 * and hoping that it is the correct size - you cannot be certain that the
 * thing you are connected to will send a message the size you expect.
 *
 * If all three output arguments (type, buf, size) are specified as NULL, then
 * the next received message will be discarded.
 *
 * @param handle	Handle to connection.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no messages are queued to the caller.
 *			If -1, the function will block indefinitely until a
 *			message is received.
 * @param type		Where to store message type ID.
 * @param buf		Buffer to copy message data to (see above description
 *			for the effect passing a NULL value has).
 * @param size		Where to store message data buffer size.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_ipc_message_receive(handle_t handle, timeout_t timeout, uint32_t *type,
                            void *buf, size_t *size) {
	ipc_message_t *message = NULL;
	handle_info_t *info = NULL;
	ipc_endpoint_t *endpoint;
	int ret;

	/* Look up the IPC handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_CONNECTION, &info)) != 0) {
		return ret;
	}
	endpoint = info->data;
	mutex_lock(&endpoint->conn->lock, 0);

	/* Check if anything can send us a message. */
	if(!endpoint->remote) {
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	}

	/* Wait for data in our message queue. The unlock/wait needs to be
	 * atomic in order to interact properly with ipc_handle_close(). */
	if((ret = waitq_sleep(&endpoint->data_sem.queue, &endpoint->conn->lock,
	                      NULL, timeout, SYNC_INTERRUPTIBLE)) != 0) {
		goto fail;
	}

	/* Recheck that we have a remote end, as it may have hung up. If there
	 * is a message in this case we must re-up the semaphore. */
	if(!endpoint->remote) {
		if(!list_empty(&endpoint->messages)) {
			/* Failure code re-ups if message != NULL. */
			message = list_entry(endpoint->messages.next, ipc_message_t, header);
		}
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	} else {
		assert(!list_empty(&endpoint->messages));
		message = list_entry(endpoint->messages.next, ipc_message_t, header);
	}

	if(type && (ret = memcpy_to_user(type, &message->type, sizeof(uint32_t))) != 0) {
		goto fail;
	}
	if(size && (ret = memcpy_to_user(size, &message->size, sizeof(size_t))) != 0) {
		goto fail;
	}
	if(buf && (ret = memcpy_to_user(buf, message->data, message->size)) != 0) {
		goto fail;
	}

	/* Message is no longer needed if buffer copied or all 3 pointer
	 * arguments are NULL. */
	if(buf || (!size && !type)) {
		list_remove(&message->header);
		kfree(message);
		semaphore_up(&endpoint->space_sem, 1);
	} else {
		semaphore_up(&endpoint->data_sem, 1);
	}

	mutex_unlock(&endpoint->conn->lock);
	handle_release(info);
	return 0;
fail:
	if(message) {
		semaphore_up(&endpoint->data_sem, 1);
	}
	mutex_unlock(&endpoint->conn->lock);
	handle_release(info);
	return ret;
}

#if 0
# pragma mark Debugger functions.
#endif

/** Print information about IPC ports.
 *
 * Prints a list of all IPC ports, or shows information about a certain port.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG status code.
 */
int kdbg_cmd_port(int argc, char **argv) {
	ipc_port_acl_entry_t *entry;
	ipc_connection_t *conn;
	ipc_port_t *port;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints either a list of all IPC ports or information about a certain port.\n");
		return KDBG_OK;
	} else if(argc == 1) {
		kprintf(LOG_NONE, "ID    Count  Waiting\n");
		kprintf(LOG_NONE, "==    =====  =======\n");

		AVL_TREE_FOREACH(&ipc_port_tree, iter) {
			port = avl_tree_entry(iter, ipc_port_t);

			kprintf(LOG_NONE, "%-5d %-6d %u\n", port->id, refcount_get(&port->count),
			        port->conn_sem.queue.missed);
		}

		return KDBG_OK;
	} else if(argc == 2) {
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(port = avl_tree_lookup(&ipc_port_tree, val))) {
			kprintf(LOG_NONE, "Invalid port ID.\n");
			return KDBG_FAIL;
		}

		kprintf(LOG_NONE, "Port %p(%d)\n", port, port->id);
		kprintf(LOG_NONE, "=================================================\n");

		kprintf(LOG_NONE, "Locked:  %d (%p) (%" PRId32 ")\n", port->lock.recursion,
		        port->lock.caller, (port->lock.holder) ? port->lock.holder->id : -1);
		kprintf(LOG_NONE, "Count:   %d\n\n", refcount_get(&port->count));

		kprintf(LOG_NONE, "Waiting (%u):\n", port->conn_sem.queue.missed);
		LIST_FOREACH(&port->waiting, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kprintf(LOG_NONE, "  Client(%p) Server(%p)\n", &conn->client, &conn->server);
		}
		kprintf(LOG_NONE, "\n");

		kprintf(LOG_NONE, "Connections:\n");
		LIST_FOREACH(&port->connections, iter) {
			conn = list_entry(iter, ipc_connection_t, header);
			kprintf(LOG_NONE, "  Client(%p) Server(%p)\n", &conn->client, &conn->server);
		}
		kprintf(LOG_NONE, "\n");

		kprintf(LOG_NONE, "ACL:\n");
		LIST_FOREACH(&port->acl, iter) {
			entry = list_entry(iter, ipc_port_acl_entry_t, header);

			kprintf(LOG_NONE, "  Type: %d  Process: %p(%d)  Rights: 0x%x\n",
			        entry->type, entry->process, (entry->process) ? entry->process->id : -1,
			        entry->rights);
		}
		return KDBG_OK;
	} else {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}
}

/** Print information about an IPC endpoint.
 *
 * Prints information about the IPC endpoint at a certain address in memory.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG status code.
 */
int kdbg_cmd_endpoint(int argc, char **argv) {
	ipc_endpoint_t *endpoint;
	ipc_message_t *message;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <addr>\n\n", argv[0]);

		kprintf(LOG_NONE, "Shows information about an IPC endpoint. The address can be obtained by\n");
		kprintf(LOG_NONE, "looking at the data field of an IPC handle.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}
	endpoint = (ipc_endpoint_t *)((ptr_t)val);

	kprintf(LOG_NONE, "Endpoint %p\n", endpoint);
	kprintf(LOG_NONE, "=================================================\n");

	kprintf(LOG_NONE, "Locked: %d (%p) (%" PRId32 ")\n", endpoint->conn->lock.recursion,
	        endpoint->conn->lock.caller,
	        (endpoint->conn->lock.holder) ? endpoint->conn->lock.holder->id : -1);
	kprintf(LOG_NONE, "Space:  %u\n", endpoint->space_sem.queue.missed);
	kprintf(LOG_NONE, "Data:   %u\n", endpoint->data_sem.queue.missed);
	kprintf(LOG_NONE, "Remote: %p\n\n", endpoint->remote);

	kprintf(LOG_NONE, "Messages:\n");
	LIST_FOREACH(&endpoint->messages, iter) {
		message = list_entry(iter, ipc_message_t, header);

		kprintf(LOG_NONE, "  %p: type %" PRIu32 ", size: %zu, buffer: %p\n",
		        message, message->type, message->size, message->data);
	}

	return KDBG_OK;
}

#if 0
# pragma mark -
#endif

/** Initialise the IPC slab cache. */
static void __init_text ipc_init(void) {
	ipc_port_id_arena = vmem_create("ipc_port_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	ipc_port_cache = slab_cache_create("ipc_port_cache", sizeof(ipc_port_t),
	                                   0, ipc_port_cache_ctor, NULL,
	                                   NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                                   NULL, 0, MM_FATAL);
	ipc_connection_cache = slab_cache_create("ipc_connection_cache", sizeof(ipc_connection_t),
	                                         0, ipc_connection_cache_ctor, NULL,
	                                         NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                                         NULL, 0, MM_FATAL);
}
INITCALL(ipc_init);
