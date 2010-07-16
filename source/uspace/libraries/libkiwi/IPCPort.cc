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

#include <kiwi/private/svcmgr.h>
#include <kiwi/IPCPort.h>

#include <cstdlib>
#include <cstring>

using namespace kiwi;
using namespace std;

/** Constructor for IPCPort.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
IPCPort::IPCPort(handle_t handle) {
	setHandle(handle);
}

/** Create a new port.
 *
 * Creates a new IPC port. If the object currently refers to a port, the old
 * port will be closed upon success, and the object will refer to the new port.
 * Upon failure, the old port will remain open.
 *
 * @return		Whether creation was successful.
 */
bool IPCPort::create() {
	handle_t handle = ipc_port_create();
	if(handle < 0) {
		return false;
	}

	setHandle(handle);
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
 * @return		Whether creation was successful.
 */
bool IPCPort::open(port_id_t id) {
	handle_t handle = ipc_port_open(id);
	if(handle < 0) {
		return false;
	}

	setHandle(handle);
	return true;
}

/** Register the port with the service manager.
 * @param name		Port name to register with.
 * @return		Whether registration was successful. */
bool IPCPort::registerName(const char *name) {
	svcmgr_register_port_t *msg;
	IPCConnection svcmgr;
	uint32_t type;
	size_t size;
	char *data;

	/* Send the message. */
	size = sizeof(*msg) + strlen(name);
	data = new char[size];
	msg = reinterpret_cast<svcmgr_register_port_t *>(data);
	msg->id = getID();
	memcpy(msg->name, name, size - sizeof(*msg));
	if(!svcmgr.connect(1) || !svcmgr.send(SVCMGR_REGISTER_PORT, msg, size)) {
		return false;
	}
	delete[] data;

	/* Await the reply. */
	if(!svcmgr.receive(type, data, size) || *(reinterpret_cast<int *>(data)) != 0) {
		return false;
	}
	delete[] data;
	return true;
}

/** Block until a connection is made to the port.
 * @param conn		Where to store pointer to connection object.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		Whether successful. */
bool IPCPort::listen(IPCConnection *&conn, useconds_t timeout) const {
	handle_t handle = ipc_port_listen(m_handle, timeout);
	if(handle < 0) {
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
 * @return		Handle to connection on success, -1 on failure. */
handle_t IPCPort::listen(useconds_t timeout) const {
	handle_t handle = ipc_port_listen(m_handle, timeout);
	return (handle < 0) ? -1 : handle;
}

/** Get the ID of a port.
 * @return		Port ID, or -1 if an error occurs. */
port_id_t IPCPort::getID() const {
	port_id_t ret;

	ret = ipc_port_id(m_handle);
	return ((ret >= 0) ? ret : -1);
}

/** Register events with the event loop. */
void IPCPort::registerEvents() {
	registerEvent(PORT_EVENT_CONNECTION);
}

/** Handle an event on the port.
 * @param id		Event ID. */
void IPCPort::eventReceived(int id) {
	switch(id) {
	case PORT_EVENT_CONNECTION:
		onConnection(this);
		break;
	}
}
