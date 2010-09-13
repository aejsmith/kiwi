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
 * @brief		Service main class.
 */

#include <kiwi/private/log.h>
#include <kiwi/IPCPort.h>
#include <kiwi/Service.h>

#include "../org.kiwi.ServiceManager.h"

using namespace kiwi;
using namespace org::kiwi::ServiceManager;

/** Macro to get the service manager connection. */
#define GET_SVCMGR()	(reinterpret_cast<ServerConnection *>(m_svcmgr))

/** Construct the service. */
Service::Service() : m_port(0) {
	/* Set up the connection to the service manager. The service manager
	 * maps handle 3 to our connection to it when it spawns us. */
	m_svcmgr = new ServerConnection(3, true);
	GET_SVCMGR()->AddPort.Connect(this, &Service::_AddPort);
}

/** Handle addition of a port.
 *
 * Handles the addition of a port. This only needs to be reimplemented for
 * services that have per-session ports under the same process. The default
 * implementation is for normal services, and just creates a port object
 * around the port and hooks its connection handler up to HandleConnection(),
 * which should be reimplemented.
 *
 * @param name		Name of the port.
 * @param id		ID of the port.
 * @param session	Session that the port is for.
 */
void Service::AddPort(const char *name, port_id_t id, session_id_t session) {
	if(m_port) {
		log::fatal("Service::AddPort must be reimplemented for multi-port services\n");
	}

	m_port = new IPCPort();
	m_port->Open(id);
	m_port->OnConnection.Connect(this, &Service::_HandleConnection);
}

/** Handle a connection on the service's port.
 * @param handle	Handle to the connection. */
void Service::HandleConnection(handle_t handle) {
	log::fatal("Service::HandleConnection must be reimplemented when using default AddPort\n");
}

/** Signal handler for port addition. */
void Service::_AddPort(const std::string &name, port_id_t id, session_id_t session) {
	AddPort(name.c_str(), id, session);
}

/** Signal handler for port connection. */
void Service::_HandleConnection() {
	handle_t handle;
	try {
		handle = m_port->Listen();
	} catch(...) {
		return;
	}
	HandleConnection(handle);
}
