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
 * @brief		IPC port class.
 */

#include <kernel/ipc.h>
#include <kiwi/IPCPort.h>

using namespace kiwi;

/** Constructor for IPCPort.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
IPCPort::IPCPort(handle_t handle) {
	SetHandle(handle);
}

/** Create a new port.
 *
 * Creates a new IPC port. If the object currently refers to a port, the old
 * port will be closed upon success, and the object will refer to the new port.
 * Upon failure, the old port will remain open.
 *
 * @throw IPCError	If the port could not be created.
 */
void IPCPort::Create() {
	handle_t handle;
	status_t ret = ipc_port_create(NULL, PORT_LISTEN, &handle);
	if(ret != STATUS_SUCCESS) {
		throw IPCError(ret);
	}

	SetHandle(handle);
}

/** Open an existing port.
 *
 * Opens an existing IPC port. If the object currently refers to a port, the
 * old port will be closed upon success, and the object will refer to the new
 * port. Upon failure, the old port will remain open.
 *
 * @param id		Port ID to open.
 *
 * @throw IPCError	If the port could not be opened.
 */
void IPCPort::Open(port_id_t id) {
	handle_t handle;
	status_t ret = ipc_port_open(id, PORT_LISTEN, &handle);
	if(ret != STATUS_SUCCESS) {
		throw IPCError(ret);
	}

	SetHandle(handle);
}

/** Block until a connection is made to the port.
 * @param conn		Where to store pointer to connection object.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		True if connection made within the timeout, false if
 *			the timeout expired.
 * @throw IPCError	If any error other than timing out occurred. */
bool IPCPort::Listen(IPCConnection *&conn, useconds_t timeout) const {
	handle_t handle = Listen(0, timeout);
	if(handle == -1) {
		return false;
	}

	conn = new IPCConnection(handle);
	return true;
}

/** Block until a connection is made to the port.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		Handle to connection if made within the timeout, -1 if
 *			the timeout expired.
 * @throw IPCError	If any error other than timing out occurred. */
handle_t IPCPort::Listen(ipc_client_info_t *infop, useconds_t timeout) const {
	handle_t handle;
	status_t ret = ipc_port_listen(m_handle, timeout, &handle, infop);
	if(ret != STATUS_SUCCESS) {
		if(ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK) {
			return -1;
		}
		throw IPCError(ret);
	}

	return handle;
}

/** Get the ID of a port.
 * @return		Port ID, or -1 if an error occurs. */
port_id_t IPCPort::GetID() const {
	return ipc_port_id(m_handle);
}

/** Register events with the event loop. */
void IPCPort::RegisterEvents() {
	RegisterEvent(PORT_EVENT_CONNECTION);
}

/** Handle an event on the port.
 * @param id		Event ID. */
void IPCPort::EventReceived(int id) {
	switch(id) {
	case PORT_EVENT_CONNECTION:
		OnConnection();
		break;
	}
}
