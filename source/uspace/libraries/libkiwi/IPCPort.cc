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
IPCPort::IPCPort(handle_id_t handle) : Handle(handle) {
	if(m_handle >= 0) {
		_RegisterEvent(PORT_EVENT_CONNECTION);
	}
}

/** Create a new port.
 *
 * Creates a new IPC port, closing any existing port the object refers to.
 * The object may or may not refer to a port upon failure, depending on whether
 * the failure occurred when closing the old port or creating the new one.
 *
 * @return		Whether creation was successful.
 */
bool IPCPort::Create() {
	if(!Close()) {
		return false;
	} else if((m_handle = ipc_port_create()) < 0) {
		return false;
	}

	_RegisterEvent(PORT_EVENT_CONNECTION);
	return true;
}

/** Open an existing port.
 *
 * Opens an existing IPC port, closing any existing port the object refers to.
 * You must have the open right on the port. The object may or may not refer to
 * a port upon failure, depending on whether the failure occurred when closing
 * the old port or creating the new one.
 *
 * @param id		Port ID to open.
 *
 * @return		Whether creation was successful.
 */
bool IPCPort::Open(port_id_t id) {
	if(!Close()) {
		return false;
	} else if((m_handle = ipc_port_open(id)) >= 0) {
		_RegisterEvent(PORT_EVENT_CONNECTION);
		return true;
	} else {
		return false;
	}
}

/** Register the port with the service manager.
 * @param name		Port name to register with.
 * @return		Whether registration was successful. */
bool IPCPort::Register(const char *name) {
	svcmgr_register_port_t *msg;
	IPCConnection svcmgr;
	uint32_t type;
	size_t size;
	char *data;

	/* Send the message. */
	size = sizeof(*msg) + strlen(name);
	data = new char[size];
	msg = reinterpret_cast<svcmgr_register_port_t *>(data);
	msg->id = GetID();
	memcpy(msg->name, name, size - sizeof(*msg));
	if(!svcmgr.Connect(1) || !svcmgr.Send(SVCMGR_REGISTER_PORT, msg, size)) {
		Close();
		return false;
	}
	delete[] data;

	/* Await the reply. */
	if(!svcmgr.Receive(type, data, size) || *(reinterpret_cast<int *>(data)) != 0) {
		Close();
		return false;
	}
	delete[] data;
	return true;
}

/** Block until a connection is made to the port.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a connection is made, and a timeout of 0 will
 *			return immediately if no connection attempts are in
 *			progress.
 * @return		Pointer to connection on success, NULL on failure. */
IPCConnection *IPCPort::Listen(useconds_t timeout) const {
	handle_id_t handle;

	if((handle = ipc_port_listen(m_handle, timeout)) < 0) {
		return 0;
	}

	return new IPCConnection(handle);
}

/** Get the ID of a port.
 * @return		Port ID, or -1 if an error occurs. */
port_id_t IPCPort::GetID() const {
	port_id_t ret;

	ret = ipc_port_id(m_handle);
	return ((ret >= 0) ? ret : -1);
}

/** Handle an event on the port.
 * @param id		Event ID. */
void IPCPort::_EventReceived(int id) {
	switch(id) {
	case PORT_EVENT_CONNECTION:
		OnConnection(this);
		break;
	}
}
