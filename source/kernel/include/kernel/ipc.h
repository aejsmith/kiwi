/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		IPC functions.
 */

#ifndef __KERNEL_IPC_H
#define __KERNEL_IPC_H

#include <kernel/object.h>
#include <kernel/security.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure containing details of a client on a port. */
typedef struct port_client {
	process_id_t pid;			/**< ID of client process. */
	session_id_t sid;			/**< ID of client process' session. */
} port_client_t;

/** IPC port rights. */
#define PORT_RIGHT_LISTEN		(1<<0)	/**< Listen for connections on the port. */
#define PORT_RIGHT_CONNECT		(1<<1)	/**< Connect to the port. */

/** IPC port handle event types. */
#define PORT_EVENT_CONNECTION		0	/**< A connection is being made to the port. */

/** IPC connection event types. */
#define CONNECTION_EVENT_HANGUP		0	/**< Remote end hung up or port was deleted. */
#define CONNECTION_EVENT_MESSAGE	1	/**< A message is available. */

/** IPC limitations. */
#define IPC_QUEUE_MAX			256	/**< Maximum number of messages in a queue at a time. */
#define IPC_MESSAGE_MAX			16384	/**< Maximum size of a message data buffer. */

extern status_t kern_port_create(const object_security_t *security, object_rights_t rights,
                                 handle_t *handlep);
extern status_t kern_port_open(port_id_t id, object_rights_t rights, handle_t *handlep);
extern port_id_t kern_port_id(handle_t handle);
extern status_t kern_port_listen(handle_t handle, useconds_t timeout, handle_t *connp,
                                 port_client_t *infop);
extern status_t kern_port_loopback(handle_t handle, handle_t connp[2]);

extern status_t kern_connection_open(port_id_t id, handle_t *handlep);
extern status_t kern_connection_send(handle_t handle, uint32_t type, const void *buf,
                                     size_t size);
extern status_t kern_connection_peek(handle_t handle, useconds_t timeout, uint32_t *typep,
                                     size_t *sizep);
extern status_t kern_connection_receive(handle_t handle, useconds_t timeout, uint32_t *typep,
                                        void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_IPC_H */
