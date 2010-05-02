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
 * @brief		IPC test.
 */

#include <kiwi/IPCConnection.h>
#include <iostream>

using namespace kiwi;
using namespace std;

int main(int argc, char **argv) {
	IPCConnection conn;
	uint32_t val = 0;

	if(!conn.Connect("org.kiwi.Pong")) {
		return 1;
	}

	while(true) {
		uint32_t type;
		size_t size;
		char *data;

		if(!conn.Send(1, &val, sizeof(uint32_t))) {
			break;
		} else if(!conn.Receive(type, data, size)) {
			break;
		}

		val = *(reinterpret_cast<uint32_t *>(data));
		cout << "Ping: Received message type " << type << ": " << val << " (size: " << size << ")" << endl;
		delete[] data;

		val++;
	}
}
