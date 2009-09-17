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
 */

#ifndef __IPC_IPC_H
#define __IPC_IPC_H

#include <lib/notifier.h>

#include <proc/process.h>

#include <types/refcount.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

struct ipc_connection;

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
	mutex_t lock;			/**< Lock covering connection. */
	refcount_t count;		/**< Handles to either end of the connection. */
	ipc_endpoint_t client;		/**< Endpoint of process that opened. */
	ipc_endpoint_t server;		/**< Endpoint of process that received. */
} ipc_connection_t;

/** In-kernel IPC message structure. */
typedef struct ipc_message {
	list_t header;			/**< Link to message queue. */
	uint32_t type;			/**< Type of message. */
	size_t size;			/**< Size of message data. */
	char data[];			/**< Message data. */
} ipc_message_t;

/** Structure describing a message to send with ipc_message_send_vector(). */
typedef struct ipc_message_vector {
	uint32_t type;			/**< Type of message. */
	void *buf;			/**< Data buffer. */
	size_t size;			/**< Size of data buffer. */
} ipc_message_vector_t;

/** IPC limitations. */
#define IPC_QUEUE_MAX		256	/**< Maximum number of messages in a queue at a time. */
#define IPC_MESSAGE_MAX		16384	/**< Maximum size of a message data buffer. */

/** IPC handle event types. */
#define IPC_EVENT_MESSAGE	1	/**< Message available to receive. */
#define IPC_EVENT_HANGUP	2	/**< Remote end hung up the connection. */

extern void ipc_process_init(process_t *process);
extern void ipc_process_destroy(process_t *process);

extern handle_t sys_ipc_connection_open(identifier_t id, timeout_t timeout);
extern handle_t sys_ipc_connection_listen(timeout_t timeout, identifier_t *pidp);

extern int sys_ipc_message_send(handle_t handle, uint32_t type, void *buf, size_t size);
extern int sys_ipc_message_send_vector(handle_t handle, ipc_message_vector_t *vec, size_t count);
extern int sys_ipc_message_receive(handle_t handle, timeout_t timeout, uint32_t *type, void *buf, size_t *size);

extern int kdbg_cmd_endpoint(int argc, char **argv);

#endif /* __IPC_IPC_H */
