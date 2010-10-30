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

#include <kiwi/Service/Service.h>
#include <kiwi/IPCPort.h>

#include "org.kiwi.ServiceManager.h"
#include "../Internal.h"

using namespace kiwi;
using namespace org::kiwi::ServiceManager;

/** Internal data for Service. */
struct kiwi::ServicePrivate {
	ServicePrivate() : svcmgr(0), port(0) {}
	ServerConnection *svcmgr;	/**< Connection to service manager. */
	IPCPort *port;			/**< Port for single port services. */
};

/** Construct the service. */
Service::Service() :
	m_priv(new ServicePrivate)
{
	/* Set up the connection to the service manager. The service manager
	 * maps handle 3 to our connection to it when it spawns us. */
	m_priv->svcmgr = new ServerConnection(3);
	m_priv->svcmgr->AddPort.Connect(this, &Service::_AddPort);
}

/** Destroy the service. */
Service::~Service() {
	if(m_priv->port) {
		delete m_priv->port;
	}
	if(m_priv->svcmgr) {
		delete m_priv->svcmgr;
	}
	delete m_priv;
}

/** Handle a connection on the service's port.
 * @param handle	Handle to the connection.
 * @param info		Information about connecting thread. */
void Service::HandleConnection(handle_t handle, ipc_client_info_t &info) {
	libkiwi_fatal("Service::HandleConnection: Must be reimplemented for services with a port.");
}

/** Signal handler for port addition.
 * @param name		Name of the port.
 * @param id		ID of the port. */
void Service::_AddPort(const std::string &name, port_id_t id) {
	m_priv->port = new IPCPort();
	m_priv->port->Open(id);
	m_priv->port->OnConnection.Connect(this, &Service::_HandleConnection);
}

/** Signal handler for port connections. */
void Service::_HandleConnection() {
	ipc_client_info_t info;
	handle_t handle;

	handle = m_priv->port->Listen(&info);
	if(unlikely(handle < 0)) {
		return;
	}

	HandleConnection(handle, info);
}
