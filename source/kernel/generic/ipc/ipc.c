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
 * This system implements a bi-directional communication channel between
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

#include <ipc/ipc.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/handle.h>
#include <proc/sched.h>

#include <sync/condvar.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <kdbg.h>

/** Structure containing information used when making connections. */
typedef struct ipc_open_data {
	list_t header;			/**< Link to destination's connection list. */
	ipc_connection_t *conn;		/**< Connection structure. */
	identifier_t pid;		/**< PID that made connection. */
	condvar_t cvar;			/**< Condition to wait for connection to be accepted. */
	int ret;			/**< Value signalling whether connection successful. */
} ipc_open_data_t;

/** Cache for connection structures. */
static slab_cache_t *ipc_connection_cache;

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

	mutex_init(&conn->lock, "ipc_connection_lock", 0);
	ipc_endpoint_init(&conn->client);
	ipc_endpoint_init(&conn->server);
	return 0;
}

/** Notifier function for IPC events.
 * @param arg1		Pointer to endpoint.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
static void ipc_handle_notifier(void *arg1, void *arg2, void *arg3) {
	handle_wait_t *wait = arg3;

	assert(wait->info->data == arg1);
	wait->cb(wait);
}

/** Signal that an IPC handle event is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_handle_wait(handle_wait_t *wait) {
	ipc_endpoint_t *endpoint = wait->info->data;

	switch(wait->event) {
	case IPC_EVENT_MESSAGE:
		notifier_register(&endpoint->msg_notifier, ipc_handle_notifier, wait);
		return 0;
	case IPC_EVENT_HANGUP:
		notifier_register(&endpoint->hangup_notifier, ipc_handle_notifier, wait);
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Stop waiting for an IPC handle event.
 * @param wait		Wait information structure. */
static void ipc_handle_unwait(handle_wait_t *wait) {
	ipc_endpoint_t *endpoint = wait->info->data;

	switch(wait->event) {
	case IPC_EVENT_MESSAGE:
		notifier_unregister(&endpoint->msg_notifier, ipc_handle_notifier, wait);
		break;
	case IPC_EVENT_HANGUP:
		notifier_unregister(&endpoint->hangup_notifier, ipc_handle_notifier, wait);
		break;
	}
}

/** Closes a handle to a connection.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int ipc_handle_close(handle_info_t *info) {
	ipc_endpoint_t *endpoint = info->data;
	ipc_message_t *message;
	int ret;

	mutex_lock(&endpoint->conn->lock, 0);

	/* If the remote is open, detach it from this end, and wake all threads
	 * all threads waiting for space on this end or messages on the remote
	 * end. They will detect that we have set remote to NULL and return an
	 * error. */
	if(endpoint->remote) {
		endpoint->remote->remote = NULL;
		waitq_wake(&endpoint->space_sem.queue, true);
		waitq_wake(&endpoint->remote->data_sem.queue, true);
		notifier_run(&endpoint->remote->hangup_notifier, NULL, false);
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

	/* Free the structure if necessary. */
	mutex_unlock(&endpoint->conn->lock);
	if(refcount_dec(&endpoint->conn->count) == 0) {
		slab_cache_free(ipc_connection_cache, endpoint->conn);
	}
	return 0;
}

/** IPC handle operations. */
static handle_type_t ipc_handle_type = {
	.id = HANDLE_TYPE_IPC,
	.wait = ipc_handle_wait,
	.unwait = ipc_handle_unwait,
	.close = ipc_handle_close,
};

/** Initialise process IPC data.
 *
 * Initialises IPC data in a process structure.
 *
 * @param process	Process to initialise.
 */
void ipc_process_init(process_t *process) {
	semaphore_init(&process->conn_sem, "ipc_conn_sem", 0);
	list_init(&process->connections);
}

/** Destroy process IPC data.
 *
 * Destroys process IPC data. All in-progress connection attempts to the
 * process will be made to return an error.
 *
 * @param process	Process being destroyed.
 */
void ipc_process_destroy(process_t *process) {
	ipc_open_data_t *data;

	LIST_FOREACH(&process->connections, iter) {
		data = list_entry(iter, ipc_open_data_t, header);
		list_remove(&data->header);
		data->ret = -ERR_NOT_FOUND;
		condvar_signal(&data->cvar);
	}
}

/** Open an IPC connection to a process.
 *
 * Opens an IPC connection to a process. The function will block until either
 * the connection is accepted, until the timeout expires, in which case it will
 * return an error.
 *
 * @param id		Process ID to connect to.
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if the specified process is not listening
 *			for a connection. If -1, the function will block
 *			indefinitely until the connection is accepted.
 *
 * @return		Handle referring to caller's end of connection on
 *			success, negative error code on failure.
 */
handle_t sys_ipc_connection_open(identifier_t id, timeout_t timeout) {
	ipc_open_data_t data;
	process_t *process;
	handle_t handle;
	int ret;

	/* Initialise the data structure used to synchronise between this
	 * function and the target process. */
	list_init(&data.header);
	condvar_init(&data.cvar, "ipc_open_cvar");
	data.pid = curr_proc->id;
	data.ret = 0;

	/* Create a connection structure. */
	data.conn = slab_cache_alloc(ipc_connection_cache, MM_SLEEP);
	refcount_set(&data.conn->count, 1);
	data.conn->client.conn = data.conn;
	data.conn->client.remote = &data.conn->server;
	data.conn->server.conn = data.conn;
	data.conn->server.remote = &data.conn->client;

	/* Create a handle now, as we do not want to find that we cannot create
	 * the handle after the destination has accepted the connection. */
	if((handle = handle_create(&curr_proc->handles, &ipc_handle_type, &data.conn->client)) < 0) {
		slab_cache_free(ipc_connection_cache, data.conn);
		return handle;
	}

	/* Find the process and place the structure in its connections list. */
	if(!(process = process_lookup(id))) {
		handle_close(&curr_proc->handles, handle);
		return -ERR_NOT_FOUND;
	}
	spinlock_lock(&process->lock, 0);
	list_append(&process->connections, &data.header);
	semaphore_up(&process->conn_sem, 1);

	/* Wait for the connection to be accepted. FIXME: What if dest is
	 * already waiting. */
	if((ret = condvar_wait_timeout(&data.cvar, NULL, &process->lock, timeout, SYNC_INTERRUPTIBLE)) != 0) {
		/* Only need to remove structure ourselves if wait errored. */
		list_remove(&data.header);
		spinlock_unlock(&process->lock);

		handle_close(&curr_proc->handles, handle);
		return ret;
	}

	spinlock_unlock(&process->lock);

	/* If the process died while we were waiting for it, the return value
	 * field in the structure will be non-zero. */
	if(data.ret != 0) {
		handle_close(&curr_proc->handles, handle);
		return data.ret;
	}
	return handle;
}

/** Wait for an IPC connection.
 *
 * Waits for an IPC connection to be made to the calling process.
 *
 * @param timeout	Timeout in microseconds. If 0, the function will return
 *			immediately if no processes are currently attempting to
 *			connect to the caller. If -1, the function will block
 *			indefinitely until a connection is made to the caller.
 * @param pidp		Optional location to store ID of connecting process in.
 *
 * @return		Handle referring to caller's end of connection on
 *			success, negative error code on failure.
 */
handle_t sys_ipc_connection_listen(timeout_t timeout, identifier_t *pidp) {
	ipc_open_data_t *data;
	handle_t ret;

	/* Wait for a connection. FIXME: Timeout. */
	if((ret = semaphore_down(&curr_proc->conn_sem, (timeout == 0) ? SYNC_NONBLOCK : SYNC_INTERRUPTIBLE)) != 0) {
		return ret;
	}

	/* Retreive the connection from the list. */
	spinlock_lock(&curr_proc->lock, 0);
	assert(!list_empty(&curr_proc->connections));
	data = list_entry(curr_proc->connections.next, ipc_open_data_t, header);
	list_remove(&data->header);
	spinlock_unlock(&curr_proc->lock);

	/* Reference the connection to account for the handle we create. */
	refcount_inc(&data->conn->count);

	/* Store the PID. */
	if(pidp && (ret = memcpy_to_user(pidp, &data->pid, sizeof(identifier_t))) != 0) {
		goto fail;
	}

	/* Create a handle for it. */
	if((ret = handle_create(&curr_proc->handles, &ipc_handle_type, &data->conn->server)) < 0) {
		goto fail;
	}

	/* Wake the process that made the connection. */
	condvar_signal(&data->cvar);
	return ret;
fail:
	refcount_dec(&data->conn->count);

	/* Return connection to the list. */
	spinlock_lock(&curr_proc->lock, 0);
	list_append(&curr_proc->connections, &data->header);
	semaphore_up(&curr_proc->conn_sem, 1);
	spinlock_unlock(&curr_proc->lock);

	return ret;
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
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_IPC, &info)) != 0) {
		goto fail;
	}
	endpoint = info->data;
	mutex_lock(&endpoint->conn->lock, 0);

	/* Wait for space in the remote message queue. The unlock/wait needs to
	 * be atomic in order to interact properly with ipc_handle_close().
	 * FIXME: Should integrate this in the semaphore API. */
	if(endpoint->remote) {
		if((ret = waitq_sleep(&endpoint->remote->space_sem.queue, &endpoint->conn->lock,
		                      NULL, -1, SYNC_INTERRUPTIBLE)) != 0) {
			goto fail;
		}
	}

	/* If remote is now NULL the remote process has hung up. */
	if(!endpoint->remote) {
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	}

	/* Queue the message. */
	list_append(&endpoint->remote->messages, &message->header);
	semaphore_up(&endpoint->remote->data_sem, 1);

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
int sys_ipc_message_send_vector(handle_t handle, ipc_message_vector_t *vec, size_t count) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Receive a message from a connection.
 *
 * Waits for a message to be queued at the caller's end of a connection and
 * returns it. Since the caller may not know the size of the message it is
 * receiving, the function can be called with a NULL buffer pointer. When this
 * is done, the type and size of the message will be returned, but the message
 * will remain queued on the connection. This allows a process to wait for a
 * message, allocate a buffer large enough to store it, and then receive it. It
 * is highly recommended that this be done, rather than just passing a buffer
 * and hoping that it is the correct size - you cannot be certain that the
 * process you are connected to will send a message the size you expect.
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
int sys_ipc_message_receive(handle_t handle, timeout_t timeout, uint32_t *type, void *buf, size_t *size) {
	ipc_message_t *message = NULL;
	handle_info_t *info = NULL;
	ipc_endpoint_t *endpoint;
	int ret;

	/* Look up the IPC handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_IPC, &info)) != 0) {
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

	/* Recheck that we have a remote end, as it may have hung up. */
	if(!endpoint->remote) {
		ret = -ERR_DEST_UNREACHABLE;
		goto fail;
	}

	assert(!list_empty(&endpoint->messages));
	message = list_entry(endpoint->messages.next, ipc_message_t, header);

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

/** Print information about an IPC endpoint.
 *
 * Prints information about the IPC endpoint at a certain addres in memory.
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
		kprintf(LOG_NONE, "Usage: %s [<addr>]\n\n", argv[0]);

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

/** Initialise the IPC slab cache. */
static void __init_text ipc_init(void) {
	ipc_connection_cache = slab_cache_create("ipc_connection_cache", sizeof(ipc_connection_t),
	                                         0, ipc_connection_cache_ctor, NULL,
	                                         NULL, NULL, NULL, 0, MM_FATAL);
}
INITCALL(ipc_init);
