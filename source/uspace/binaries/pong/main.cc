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

#include <kernel/ipc.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	uint32_t type, data;
	identifier_t id;
	handle_t handle;
	size_t size;
	int ret;

	id = strtoul(argv[1], NULL, 10);

	if((handle = ipc_connection_open(id, -1)) < 0) {
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
	return 0;
}
