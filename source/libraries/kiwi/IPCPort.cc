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

#include <kernel/object.h>
#include <kiwi/IPCPort.h>
#include "Internal.h"

using namespace kiwi;

/** Constructor for IPCPort.
 * @param handle	If not negative, an existing port handle to make the
 *			object use. Must refer to a port object. */
IPCPort::IPCPort(handle_t handle) {
	if(handle >= 0) {
		if(unlikely(kern_object_type(handle) != OBJECT_TYPE_PORT)) {
			libkiwi_fatal("IPCPort::IPCPort: Handle must refer to a port object.");
		}

		SetHandle(handle);
	}
}

/** Create a new port.
 *
 * Creates a new IPC port. If the object currently refers to a port, the old
 * port will be closed upon success, and the object will refer to the new port.
 * Upon failure, the old port will remain open.
 *
 * @return		True if succeeded, false if not.
 */
bool IPCPort::Create() {
	handle_t handle;
	status_t ret = ipc_port_create(NULL, PORT_RIGHT_LISTEN, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Open an existing port.
 *
 * Opens an existing IPC port. If the object currently refers to a port, the
 * old port will be closed upon success, and the object will refer to the new
 * port. Upon failure, the old port will remain open.
 *
 * @param id		Port ID to open.
 *
 * @return		True if succeeded, false if not.
 */
bool IPCPort::Open(port_id_t id) {
	handle_t handle;
	status_t ret = ipc_port_open(id, PORT_RIGHT_LISTEN, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Block until a connection is made to the port.
 * @param conn		Where to store pointer to connection object.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		True on success, false if an error occurred. */
bool IPCPort::Listen(IPCConnection *&conn, useconds_t timeout) {
	handle_t handle = Listen(0, timeout);
	if(unlikely(handle < 0)) {
		return false;
	}

	conn = new IPCConnection(handle);
	return true;
}

/** Block until a connection is made to the port.
 * @param infop		If not NULL, information about the client will be
 *			stored in the structure this points to.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		Handle to connection, or -1 if an error occurred. */
handle_t IPCPort::Listen(ipc_client_info_t *infop, useconds_t timeout) {
	handle_t handle;
	status_t ret = ipc_port_listen(m_handle, timeout, &handle, infop);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return -1;
	}

	return handle;
}

/** Get the ID of a port.
 * @return		Port ID. */
port_id_t IPCPort::GetID() const {
	return ipc_port_id(m_handle);
}

/** Register events with the event loop. */
void IPCPort::RegisterEvents() {
	RegisterEvent(PORT_EVENT_CONNECTION);
}

/** Handle an event on the port.
 * @param event		Event ID. */
void IPCPort::HandleEvent(int event) {
	switch(event) {
	case PORT_EVENT_CONNECTION:
		OnConnection();
		break;
	}
}
