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

#include <iostream>

using namespace kiwi;
using namespace std;

int main(int argc, char **argv) {
	IPCConnection conn;
	handle_t handle;
	uint32_t type;
	void *mapping;
	shm_id_t id;
	size_t size;
	char *data;
	int ret;

	if(!conn.connect("org.kiwi.SHMServer")) {
		cerr << "Failed to connect to server" << endl;
		return 1;
	}

	/* Receive the area ID from the server. */
	if(!conn.receive(type, data, size)) {
		cerr << "Failed to receive message" << endl;
		return 1;
	} else if(size != sizeof(id)) {
		cerr << "Incorrect data size received: " << size << endl;
		return 1;
	}
	id = *reinterpret_cast<shm_id_t *>(data);
	delete[] data;
	conn.close();
	cout << "Received area ID " << id << " from server" << endl;

	/* Open the shared memory area. */
	handle = shm_open(id);
	if(handle < 0) {
		cerr << "Failed to open area: " << handle << endl;
		return 1;
	}

	/* Map it in and read the data. */
	ret = vm_map(NULL, 0x1000, VM_MAP_READ, handle, 0, &mapping);
	if(ret != 0) {
		cerr << "Failed to map area: " << ret << endl;
		return 1;
	}
	cout << "String in area: " << reinterpret_cast<char *>(mapping) << endl;
	return 0;
}
