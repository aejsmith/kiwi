/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Service main class.
 */

#include <kiwi/Service/Service.h>
#include <kiwi/IPCPort.h>

#include <memory>

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
Service::Service() : m_priv(0) {
	std::unique_ptr<ServicePrivate> priv(new ServicePrivate);

	/* Set up the connection to the service manager. The service manager
	 * maps handle 3 to our connection to it when it spawns us. */
	priv->svcmgr = new ServerConnection(3);
	priv->svcmgr->AddPort.Connect(this, &Service::_AddPort);

	m_priv = priv.release();
}

/** Destroy the service. */
Service::~Service() {
	if(m_priv->port) {
		delete m_priv->port;
	}
	delete m_priv->svcmgr;
	delete m_priv;
}

/** Handle a connection on the service's port.
 * @param handle	Handle to the connection.
 * @param info		Information about connecting thread. */
void Service::HandleConnection(handle_t handle, port_client_t &info) {
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
	port_client_t info;
	handle_t handle;

	handle = m_priv->port->Listen(&info);
	if(unlikely(handle < 0)) {
		return;
	}

	HandleConnection(handle, info);
}
