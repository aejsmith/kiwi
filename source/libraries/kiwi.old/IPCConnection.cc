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

#include <cstdlib>

#include "org.kiwi.ServiceManager.h"

using namespace kiwi;
using namespace org::kiwi::ServiceManager;

/** Constructor for IPCConnection.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
IPCConnection::IPCConnection(handle_t handle) {
	SetHandle(handle);
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param id		Port ID to connect to.

 * @throw IPCError	Thrown if unable to connect.
 */
void IPCConnection::Connect(port_id_t id) {
	handle_t handle;
	status_t ret = ipc_connection_open(id, &handle);
	if(ret != STATUS_SUCCESS) {
		throw IPCError(ret);
	}

	SetHandle(handle);
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param name		Port name to connect to.
 *
 * @throw IPCError	Thrown if unable to connect.
 */
void IPCConnection::Connect(const char *name) {
	port_id_t id;

	/* Work out the service manager port ID. The ID of the session's
	 * service manager (if any) is set in the environment. */
	const char *pstr = getenv("SVCMGR_PORT");
	if(pstr) {
		id = strtol(pstr, NULL, 10);
	} else {
		id = 1;
	}

	/* Look up the port ID. */
	{
		ServerConnection svcmgr(id);
		status_t ret = svcmgr.LookupPort(name, id);
		if(ret != STATUS_SUCCESS) {
			throw IPCError(ret);
		}
	}

	return Connect(id);
}

/** Send a message on a port.
 * @param type		Type ID of message to send.
 * @param buf		Data buffer to send.
 * @param size		Size of data buffer.
 * @throw IPCError	Thrown if unable to send. */
void IPCConnection::Send(uint32_t type, const void *buf, size_t size) {
	status_t ret = ipc_message_send(m_handle, type, buf, size);
	if(ret != STATUS_SUCCESS) {
		throw IPCError(ret);
	}
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
 * @return		True if message received within the timeout, false if
 *			the timeout expired.
 * @throw IPCError	If any error other than timing out occurred. */
bool IPCConnection::Receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout) {
	status_t ret = ipc_message_peek(m_handle, timeout, &type, &size);
	if(ret != STATUS_SUCCESS) {
		if(ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK) {
			return false;
		}
		throw IPCError(ret);
	}

	data = new char[size];
	ret = ipc_message_receive(m_handle, 0, 0, data, size);
	if(ret != STATUS_SUCCESS) {
		delete[] data;
		throw IPCError(ret);
	}

	return true;
}

/** Wait for the remote end to hang up the connection.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until the connection is hung up, and a timeout of 0
 *			will return immediately if the connection is not
 *			already hung up.
 * @return		True if successful, false if the timeout expired. */
bool IPCConnection::WaitForHangup(useconds_t timeout) const {
	return Wait(CONNECTION_EVENT_HANGUP, timeout);
}

/** Register events with the event loop. */
void IPCConnection::RegisterEvents() {
	RegisterEvent(CONNECTION_EVENT_HANGUP);
	RegisterEvent(CONNECTION_EVENT_MESSAGE);
}

/** Handle an event on the connection.
 * @param id		Event ID. */
void IPCConnection::EventReceived(int id) {
	switch(id) {
	case CONNECTION_EVENT_HANGUP:
		OnHangup();
		break;
	case CONNECTION_EVENT_MESSAGE:
		OnMessage();
		break;
	}
}
