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
 * @brief		Shared memory test.
 */

#include <kernel/shm.h>
#include <kernel/vm.h>

#include <kiwi/IPCConnection.h>
#include <kiwi/IPCPort.h>

#include <cstring>
#include <iostream>

using namespace kiwi;
using namespace std;

int main(int argc, char **argv) {
	IPCConnection *conn;
	IPCPort port(3);
	handle_t handle;
	void *mapping;
	status_t ret;
	shm_id_t id;

	/* Create the shared memory area. */
	ret = shm_create(0x1000, &handle);
	if(ret < 0) {
		cerr << "Failed to create area: " << ret << endl;
		return 1;
	}
	id = shm_id(handle);

	/* Map it in and stick some data in it. */
	ret = vm_map(NULL, 0x1000, VM_MAP_READ | VM_MAP_WRITE, handle, 0, &mapping);
	if(ret != STATUS_SUCCESS) {
		cerr << "Failed to map area: " << ret << endl;
		return 1;
	}
	strcpy(reinterpret_cast<char *>(mapping), "This is some data in shared memory!");

	/* Send the area ID to any client that connects. */
	while(port.Listen(conn)) {
		conn->Send(0, &id, sizeof(id));
		conn->WaitForHangup();
	}
}
