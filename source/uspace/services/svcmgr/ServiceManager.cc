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
 * @brief		Service manager.
 */

#include <kernel/errors.h>

#include <kiwi/Process.h>

#include "ServiceManager.h"

#include <exception>
#include <iostream>

using namespace kiwi;
using namespace std;

/** Service manager constructor. */
ServiceManager::ServiceManager() {
	m_port.onConnection.connect(this, &ServiceManager::handleConnection);

	/* Create the port. TODO: Per-user instance. */
	if(!m_port.create()) {
		cerr << "svcmgr: could not create port" << endl;
		throw exception();
	} else if(m_port.getID() != 1) {
		cerr << "svcmgr: created port (" << m_port.getID() << ") is not port 1" << endl;
		throw exception();
	}
}

/** Add a service to the service manager.
 * @param service	Service to add. */
void ServiceManager::addService(Service *service) {
	Service::PortList::const_iterator it;

	/* Register ports. */
	for(it = service->getPorts().begin(); it != service->getPorts().end(); ++it) {
		m_ports.insert(make_pair(*it, new Port(service)));
	}

	if(!(service->getFlags() & Service::OnDemand)) {
		service->start();
	}
}

/** Look up a port name in the port map.
 * @param name		Name to look up.
 * @return		Pointer to port object if found, 0 if not. */
Port *ServiceManager::lookupPort(const char *name) {
	PortMap::iterator it = m_ports.find(name);
	return (it != m_ports.end()) ? it->second : 0;
}

/** Handle a connection on the service manager port. */
void ServiceManager::handleConnection(IPCPort *) {
	IPCConnection *conn;

	if((conn = m_port.listen())) {
		conn->onMessage.connect(this, &ServiceManager::handleMessage); 
		conn->onHangup.connect(this, &ServiceManager::handleHangup);
	}
}

/** Handle a message on a connection to the service manager.
 * @param conn		Connection object. */
void ServiceManager::handleMessage(IPCConnection *conn) {
	uint32_t type;
	size_t size;
	Port *port;
	char *data;

	if(!conn->receive(type, data, size)) {
		return;
	}

	switch(type) {
	case SVCMGR_LOOKUP_PORT:
	{
		string name(data, size);
		if((port = lookupPort(name.c_str()))) {
			port->sendID(conn);
		} else {
			port_id_t ret = -ERR_NOT_FOUND;
			conn->send(type, &ret, sizeof(ret));
		}
		break;
	}
	case SVCMGR_REGISTER_PORT:
	{
		svcmgr_register_port_t *args = reinterpret_cast<svcmgr_register_port_t *>(data);
		int ret = 0;

		if(size > sizeof(svcmgr_register_port_t) && args->id > 0) {
			string name(args->name, size - sizeof(svcmgr_register_port_t));
			if((port = lookupPort(name.c_str()))) {
				if(!port->setID(args->id)) {
					ret = -ERR_PERM_DENIED;
				}
			} else {
				ret = -ERR_NOT_FOUND;
			}
		} else {
			ret = -ERR_PARAM_INVAL;
		}

		conn->send(type, &ret, sizeof(ret));
		break;
	}
	default:
		/* Just ignore it. */
		break;
	}

	delete[] data;
}

/** Handle the connection being hung up.
 * @param conn		Connection object. */
void ServiceManager::handleHangup(IPCConnection *conn) {
	conn->deleteLater();
}
