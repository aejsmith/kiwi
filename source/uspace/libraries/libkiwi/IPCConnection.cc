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

#include <kiwi/IPCConnection.h>

#include <cstring>

#include "svcmgr.h"

using namespace kiwi;
using namespace org::kiwi::ServiceManager;
using namespace std;

/** Constructor for IPCConnection.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
IPCConnection::IPCConnection(handle_t handle) {
	setHandle(handle);
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param id		Port ID to connect to.

 * @return		Whether connection was successful.
 */
bool IPCConnection::connect(port_id_t id) {
	handle_t handle = ipc_connection_open(id);
	if(handle < 0) {
		return false;
	}

	setHandle(handle);
	return true;
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param name		Port name to connect to.
 *
 * @return		Whether creation was successful.
 */
bool IPCConnection::connect(const char *name) {
	ServerConnection svcmgr;
	if(!svcmgr.connect(1)) {
		return false;
	}

	/* Look up the port ID. */
	port_id_t id;
	if(svcmgr.lookupPort(name, id) != 0) {
		return false;
	}

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

/** Wait for the remote end to hang up the connection.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until the connection is hung up, and a timeout of 0
 *			will return immediately if the connection is not
 *			already hung up.
 * @return		Whether successful. */
bool IPCConnection::waitHangup(useconds_t timeout) const {
	return wait(CONNECTION_EVENT_HANGUP, timeout);
}

/** Register events with the event loop. */
void IPCConnection::registerEvents() {
	registerEvent(CONNECTION_EVENT_HANGUP);
	registerEvent(CONNECTION_EVENT_MESSAGE);
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
