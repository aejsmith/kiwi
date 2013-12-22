/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		IPC service manager example code.
 *
 * Simple example to demonstrate the basic idea of how the IPC system works and
 * how the service manager is used to connect to other services. It omits
 * various things that the real service manager protocol would do for
 * simplicity's sake.
 *
 * For example, the real protocol would actually allow for the inclusion of a
 * payload message to direct to the service itself. This is not included here,
 * but would essentially involve wrapping the payload message in the data block
 * sent with the connection message. When forwarding the connection the service
 * manager would unwrap the message and send that on as payload.
 *
 * Also not included is that a session service manager instance would forward
 * on a connection request to the system instance if it does not know the
 * requested name.
 */

#include <kernel/ipc.h>
#include <kernel/status.h>

#include <string.h>

/** Service manager message IDs. */
#define SVCMGR_CONNECT		1

/** Maximum length of a service name. */
#define SVCMGR_NAME_MAX		128

/**
 * Client
 */

/**
 * Connect to a service by name.
 *
 * Sends a request to the service manager to connect to the requested named
 * service, and if successful returns a handle to this process' end of the
 * created connection.
 *
 * @param name		Name of the service to connect to.
 * @param connp		Where to store handle to connection.
 *
 * @return		Status code describing the result of the connection
 *			attempt.
 */
status_t connect_to_service(const char *name, handle_t *connp) {
	ipc_message_t msg;
	status_t ret;

	/* Create a connection request message. The name is attached as a data
	 * buffer. */
	memset(&msg, 0, sizeof(msg));
	msg.id = SVCMGR_CONNECT;
	msg.size = strlen(name);

	/* Make the connection by opening a connection to the service manager
	 * with the connection request message attached as the payload. A
	 * process' root port refers to its session's service manager instance.
	 * When the service manager receives this message it will forward the
	 * connection on to the requested service. */
	return kern_connection_open(PROCESS_ROOT_PORT, &msg, name, -1, connp);
}

/**
 * Service manager.
 */

extern status_t lookup_service(const char *name, handle_t *servicep);

/**
 * Handle a connection request.
 *
 * This function is called from the main event loop when a connection event is
 * indicated on the service manager's main port.
 *
 * @param port		Handle to the port.
 */
void handle_connection(handle_t port) {
	char name[SVCMGR_NAME_MAX + 1];
	ipc_message_t msg;
	ipc_client_t client;
	handle_t conn, service;
	status_t ret;

	ret = kern_port_listen(port, &msg, &client, 0, &conn);
	if(ret != STATUS_SUCCESS)
		return;

	/* We now have a handle to our end of the connection. The connection is
	 * not yet fully open - the client's kern_connection_open() call has
	 * not returned. We can now either accept, reject or forward the
	 * connection. We cannot send or receive at this point. */

	if(!msg.size || msg.size > SVCMGR_NAME_MAX) {
		/* Rejecting closes the handle and causes the client's call to
		 * kern_connection_open() to return the specified error code. */
		kern_connection_reject(conn, STATUS_INVALID_ARG);
		return;
	}

	/* Receive the name. This function gets the data attached to the last
	 * message received on a connection, which in this case is the payload
	 * message. The size of the data is given by msg.size in the received
	 * message. Pending data/handles are dropped when any operation takes
	 * place on this end of the connection other than receive_data() or
	 * receive_handle() (i.e. another receive(), or a send()). */
	ret = kern_connection_receive_data(conn, name);
	if(ret != STATUS_SUCCESS) {
		kern_connection_reject(conn, STATUS_TRY_AGAIN);
		return;
	}

	/* Look up a handle to the service's port. If the service is not yet
	 * running, this may cause it to be started. */
	name[msg.size] = 0;
	ret = lookup_service(name, &service);
	if(ret != STATUS_SUCCESS) {
		kern_connection_reject(conn, ret);
		return;
	}

	/* Forward the connection onto the service. This closes the handle on
	 * our end upon success. The only errors returned here are due to
	 * problems with what we pass to the function. Errors connecting to the
	 * target service are returned back to process making the connection,
	 * not here. */
	ret = kern_connection_forward(conn, service, NULL, NULL);
	if(ret != STATUS_SUCCESS) {
		kern_connection_reject(conn, STATUS_TRY_AGAIN);
		return;
	}
}
