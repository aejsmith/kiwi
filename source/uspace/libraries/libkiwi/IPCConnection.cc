/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IPC connection class.
 */

#include <kernel/object.h>
#include <kernel/ipc.h>

#include <kiwi/private/svcmgr.h>
#include <kiwi/IPCConnection.h>

#include <cstring>

using namespace kiwi;
using namespace std;

/** Constructor for IPCConnection.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
IPCConnection::IPCConnection(handle_t handle) : Handle(handle) {
	if(m_handle >= 0) {
		registerEvent(CONNECTION_EVENT_HANGUP);
		registerEvent(CONNECTION_EVENT_MESSAGE);
	}
}

/** Connect to a port.
 *
 * Connects to an IPC port, closing any existing connection the object refers
 * to. The object may or may not refer to a connection upon failure, depending
 * on whether the failure occurred when closing the old port or creating the
 * new one.
 *
 * @param id		Port ID to connect to.

 * @return		Whether creation was successful.
 */
bool IPCConnection::connect(port_id_t id) {
	if(!close()) {
		return false;
	} else if((m_handle = ipc_connection_open(id)) >= 0) {
		registerEvent(CONNECTION_EVENT_HANGUP);
		registerEvent(CONNECTION_EVENT_MESSAGE);
		return true;
	} else {
		return false;
	}
}

/** Connect to a port.
 *
 * Connects to an IPC port, closing any existing connection the object refers
 * to. The object may or may not refer to a connection upon failure, depending
 * on whether the failure occurred when closing the old port or creating the
 * new one.
 *
 * @param name		Port name to connect to.
 *
 * @return		Whether creation was successful.
 */
bool IPCConnection::connect(const char *name) {
	IPCConnection svcmgr;
	uint32_t type;
	port_id_t id;
	size_t size;
	char *data;

	if(!svcmgr.connect(1)) {
		return false;
	} else if(!svcmgr.send(SVCMGR_LOOKUP_PORT, name, strlen(name))) {
		return false;
	} else if(!svcmgr.receive(type, data, size)) {
		return false;
	} else if((id = *(reinterpret_cast<port_id_t *>(data))) < 0) {
		return false;
	}

	delete[] data;
	return connect(id);
}

/** Send a message on a port.
 * @param type		Type ID of message to send.
 * @param buf		Data buffer to send.
 * @param size		Size of data buffer.
 * @return		Whether send was successful. */
bool IPCConnection::send(uint32_t type, const void *buf, size_t size) {
	return (ipc_message_send(m_handle, type, buf, size) == 0);
}

/** Receive a message from a port.
 * @param type		Where to store type of message.
 * @param buf		Where to store pointer to data buffer (should be freed
 *			with delete[] when no longer needed).
 * @param size		Where to store size of buffer.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a message is received, and a timeout of 0 will
 *			return immediately if no messages are waiting to be
 *			received.
 * @return		Whether received successfully. */
bool IPCConnection::receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout) {
	if(ipc_message_peek(m_handle, timeout, &type, &size) != 0) {
		return false;
	}

	data = new char[size];
	if(ipc_message_receive(m_handle, 0, 0, data, size) != 0) {
		delete[] data;
		return false;
	}

	return true;
}

/** Handle an event on the connection.
 * @param id		Event ID. */
void IPCConnection::eventReceived(int id) {
	switch(id) {
	case CONNECTION_EVENT_HANGUP:
		onHangup(this);
		break;
	case CONNECTION_EVENT_MESSAGE:
		onMessage(this);
		break;
	}
}
