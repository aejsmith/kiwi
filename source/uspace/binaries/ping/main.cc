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
#include <string.h>

extern char **environ;

int main(int argc, char **argv) {
	char buf[12];
	char *args[] = { (char *)"/system/binaries/pong", buf, NULL };
	identifier_t id = process_id(-1);
	handle_t handle;
	uint32_t val = 0, data, type;
	size_t size;
	int ret;

	sprintf(buf, "%d", id);
	if((handle = process_create(args[0], args, environ, 0)) < 0) {
		return handle;
	}
	printf("Ping: Spawned pong (handle: %d, process: %d)\n", handle, process_id(handle));
	handle_close(handle);

	if((handle = ipc_connection_listen(-1, &id)) < 0) {
		return handle;
	}
	printf("Ping: Received connection from process %d (handle: %d)\n", id, handle);

	while(true) {
		if((ret = ipc_message_send(handle, 1, &val, sizeof(uint32_t))) != 0) {
			return ret;
		}
		val++;

		if((ret = ipc_message_receive(handle, -1, &type, &data, &size)) != 0) {
			return ret;
		}

		printf("Ping: Received message type %u: %u (size: %zu)\n", type, data, size);
	}
}
