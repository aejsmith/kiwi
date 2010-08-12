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
	status_t ret;
	shm_id_t id;
	size_t size;
	char *data;

	/* Connect to the server. */
	conn.Connect("org.kiwi.SHMServer");

	/* Receive the area ID from the server. */
	conn.Receive(type, data, size);
	if(size != sizeof(id)) {
		cerr << "Incorrect data size received: " << size << endl;
		return 1;
	}
	id = *reinterpret_cast<shm_id_t *>(data);
	delete[] data;
	conn.Close();
	cout << "Received area ID " << id << " from server" << endl;

	/* Open the shared memory area. */
	ret = shm_open(id, &handle);
	if(ret != STATUS_SUCCESS) {
		cerr << "Failed to open area: " << ret << endl;
		return 1;
	}

	/* Map it in and read the data. */
	ret = vm_map(NULL, 0x1000, VM_MAP_READ, handle, 0, &mapping);
	if(ret != STATUS_SUCCESS) {
		cerr << "Failed to map area: " << ret << endl;
		return 1;
	}
	cout << "String in area: " << reinterpret_cast<char *>(mapping) << endl;
	return 0;
}
