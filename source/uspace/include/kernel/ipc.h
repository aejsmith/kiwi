/* Kiwi IPC definitions
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

/** Structure describing a message to send with ipc_message_send_vector(). */
typedef struct ipc_message_vector {
	uint32_t type;			/**< Type of message. */
	void *buf;			/**< Data buffer. */
	size_t size;			/**< Size of data buffer. */
} ipc_message_vector_t;

/** IPC handle event types. */
#define IPC_EVENT_MESSAGE	16	/**< Message available to receive. */
#define IPC_EVENT_HANGUP	17	/**< Remote end hung up the connection. */

extern handle_t ipc_connection_open(identifier_t id, timeout_t timeout);
extern handle_t ipc_connection_listen(timeout_t timeout, identifier_t *pidp);

extern int ipc_message_send(handle_t handle, uint32_t type, void *buf, size_t size);
extern int ipc_message_send_vector(handle_t handle, ipc_message_vector_t *vec, size_t count);
extern int ipc_message_receive(handle_t handle, timeout_t timeout, uint32_t *type, void *buf, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_IPC_H */
