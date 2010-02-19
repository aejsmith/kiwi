/*
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
 * @brief		IPC definitions.
 */

#ifndef __KERNEL_IPC_H
#define __KERNEL_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/types.h>

/** Enumeration of possible port ACL entry types. */
typedef enum ipc_port_accessor {
	IPC_PORT_ACCESSOR_ALL,			/**< Entry applies to everything in the system. */
	IPC_PORT_ACCESSOR_PROCESS,		/**< Entry applies to a certain process. */
	//IPC_PORT_ACCESSOR_USER,		/**< Entry applies to a certain user. */
} ipc_port_accessor_t;

/** Structure describing a message to send with ipc_message_send_vector(). */
typedef struct ipc_message_vector {
	uint32_t type;				/**< Type of message. */
	const void *buf;			/**< Data buffer. */
	size_t size;				/**< Size of data buffer. */
} ipc_message_vector_t;

/** Possible rights on an IPC port. */
#define IPC_PORT_RIGHT_OPEN		(1<<0)	/**< The right to open the port and listen for connections on it. */
#define IPC_PORT_RIGHT_MODIFY		(1<<1)	/**< The right to modify the port's ACL. */
#define IPC_PORT_RIGHT_CONNECT		(1<<2)	/**< The right to open a connection on the port. */

/** IPC port handle event types. */
#define IPC_PORT_EVENT_CONNECTION	16	/**< A connection is being made to the port. */

/** IPC connection event types. */
#define IPC_CONNECTION_EVENT_HANGUP	16	/**< Remote end hung up or port was deleted. */

extern handle_t ipc_port_create(void);
extern handle_t ipc_port_open(identifier_t id);
extern identifier_t ipc_port_id(handle_t handle);
extern handle_t ipc_port_listen(handle_t handle, useconds_t timeout);
extern int ipc_port_acl_add(handle_t handle, ipc_port_accessor_t type, identifier_t id,
                            uint32_t rights);
extern int ipc_port_acl_remove(handle_t handle, ipc_port_accessor_t type, identifier_t id,
                               uint32_t rights);

extern handle_t ipc_connection_open(identifier_t id, useconds_t timeout);

extern int ipc_message_send(handle_t handle, uint32_t type, const void *buf, size_t size);
extern int ipc_message_sendv(handle_t handle, ipc_message_vector_t *vec, size_t count);
extern int ipc_message_receive(handle_t handle, useconds_t timeout, uint32_t *type, void *buf, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_IPC_H */
