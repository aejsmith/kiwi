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

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	uint32_t type, data;
	identifier_t id;
	handle_t handle;
	size_t size;
	int ret;

	id = strtoul(getenv("PORT"), NULL, 10);
	if((handle = ipc_port_open(id)) < 0) {
		printf("Pong: Failed to open port %d: %d\n", id, handle);
		return handle;
	} else if((ret = ipc_port_acl_add(handle, IPC_PORT_ACCESSOR_PROCESS, process_id(-1), IPC_PORT_RIGHT_CONNECT)) != 0) {
		printf("Pong: Failed to modify ACL: %d\n", ret);
		return ret;
	}
	handle_close(handle);

	if((handle = ipc_connection_open(id, -1)) < 0) {
		printf("Pong: Failed to connect to port %d: %d\n", id, handle);
		return handle;
	}

	while(true) {
		if((ret = ipc_message_receive(handle, -1, &type, &data, &size)) != 0) {
			return ret;
		}

		printf("Pong: Received message type %u: %u (size: %zu)\n", type, data, size);

		if((ret = ipc_message_send(handle, 2, &data, sizeof(uint32_t))) != 0) {
			return ret;
		}
	}
}
