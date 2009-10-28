/* Kiwi IPC test
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
 * @brief		IPC test.
 */

#include <kernel/handle.h>
#include <kernel/ipc.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace kiwi;

extern char **environ;

int main(int argc, char **argv) {
	uint32_t val = 0, data, type;
	handle_t port, conn;
	char buf[12];
	size_t size;
	int ret;

	if((port = ipc_port_create()) < 0) {
		return port;
	}
	printf("Ping: Created port %d (handle: %d)\n", ipc_port_id(port), port);

	if((ret = ipc_port_acl_add(port, IPC_PORT_ACCESSOR_ALL, 0, IPC_PORT_RIGHT_OPEN | IPC_PORT_RIGHT_MODIFY)) != 0) {
		return ret;
	}

	sprintf(buf, "%d", ipc_port_id(port));
	setenv("PORT", buf, 1);
	{
		Process proc("pong");
		if(!proc.Initialised(&ret)) {
			return ret;
		}
		printf("Ping: Spawned pong (process: %d)\n", proc.GetID());
	}

	if((conn = ipc_port_listen(port, -1)) < 0) {
		return conn;
	}
	printf("Ping: Received connection on port %d (handle: %d)\n", ipc_port_id(port), conn);

	while(true) {
		if((ret = ipc_message_send(conn, 1, &val, sizeof(uint32_t))) != 0) {
			return ret;
		}
		val++;

		if((ret = ipc_message_receive(conn, -1, &type, &data, &size)) != 0) {
			return ret;
		}

		printf("Ping: Received message type %u: %u (size: %zu)\n", type, data, size);
	}
}
