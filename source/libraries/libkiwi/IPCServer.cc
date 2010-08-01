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
 * @brief		IPC server class.
 */

#include <kiwi/IPCServer.h>
#include <cstring>
#include <stdexcept>

using namespace kiwi;

/** Initialise the server from a port handle.
 * @param handle	Handle to the port. This defaults to 3, which is the
 *			handle ID that the service manager passes the port
 *			handle as. If < 0, a new unnamed port will be created. */
IPCServer::IPCServer(handle_t handle) : m_port(handle) {
	if(handle < 0) {
		if(!m_port.create()) {
			throw std::runtime_error("Failed to create port");
		}
	}
	m_port.onConnection.connect(this, &IPCServer::_handleConnection);
}

/** Signal handler for a connection. */
void IPCServer::_handleConnection() {
	handle_t handle = m_port.listen();
	if(handle >= 0) {
		handleConnection(handle);
	}
}
