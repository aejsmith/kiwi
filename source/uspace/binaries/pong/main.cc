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

#include <kiwi/private/svcmgr.h>
#include <kiwi/IPCConnection.h>
#include <kiwi/IPCPort.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define PORT_NAME "org.kiwi.Pong"

using namespace kiwi;
using namespace std;

extern "C" FILE *fopen_device(const char *path, FILE *stream);

int main(int argc, char **argv) {
	IPCConnection *conn;
	IPCPort port;

	/* Use the console for output. */
	fopen_device("/console/0/slave", stdout);

	/* Create the port. */
	port.Create();
	port.GrantAccess(IPC_PORT_ACCESSOR_ALL, 0, IPC_PORT_RIGHT_CONNECT);

	/* FIXME: Integrate this into IPCPort. */
	{
		svcmgr_register_port_t *msg;
		IPCConnection svcmgr;
		uint32_t type;
		size_t size;
		char *data;

		msg = reinterpret_cast<svcmgr_register_port_t *>(malloc(sizeof(*msg) + strlen(PORT_NAME)));
		msg->id = port.GetID();
		memcpy(msg->name, PORT_NAME, strlen(PORT_NAME));

		svcmgr.Connect(1);
		svcmgr.Send(SVCMGR_REGISTER_PORT, msg, sizeof(*msg) + strlen(PORT_NAME));
		svcmgr.Receive(type, data, size);
		delete[] data;
		free(msg);
	}

	while((conn = port.Listen())) {
		while(true) {
			uint32_t type, val;
			size_t size;
			char *data;

			if(!conn->Receive(type, data, size)) {
				break;
			}

			val = *(reinterpret_cast<uint32_t *>(data));
			cout << "Pong: Received message type " << type << ": " << val << " (size: " << size << ")" << endl;

			if(!conn->Send(2, data, size)) {
				break;
			}

			delete data;
		}

		delete conn;
	}
}
