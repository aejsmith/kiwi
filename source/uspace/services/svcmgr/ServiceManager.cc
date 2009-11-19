/* Kiwi service manager
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

/** Service manager connection constructor. */
ServiceManager::Connection::Connection(IPCConnection *conn, ServiceManager *svcmgr) :
	m_conn(conn), m_svcmgr(svcmgr)
{
	conn->OnMessage.Connect(this, &ServiceManager::Connection::_MessageReceived);
	conn->OnHangup.Connect(this, &ServiceManager::Connection::_ConnectionHangup);
}

/** Handle a message on a connection to the service manager. */
void ServiceManager::Connection::_MessageReceived() {
	uint32_t type;
	size_t size;
	Port *port;
	char *data;

	if(!m_conn->Receive(type, data, size)) {
		return;
	}

	switch(type) {
	case SVCMGR_LOOKUP_PORT:
	{
		string name(data, size);
		if((port = m_svcmgr->LookupPort(name.c_str()))) {
			port->SendID(m_conn);
		} else {
			identifier_t ret = -ERR_NOT_FOUND;
			m_conn->Send(type, &ret, sizeof(ret));
		}
		break;
	}
	case SVCMGR_REGISTER_PORT:
	{
		svcmgr_register_port_t *args = reinterpret_cast<svcmgr_register_port_t *>(data);
		int ret = 0;

		if(size > sizeof(svcmgr_register_port_t) && args->id > 0) {
			string name(args->name, size - sizeof(svcmgr_register_port_t));
			if((port = m_svcmgr->LookupPort(name.c_str()))) {
				port->SetID(args->id);
			} else {
				ret = -ERR_NOT_FOUND;
			}
		} else {
			ret = -ERR_PARAM_INVAL;
		}

		m_conn->Send(type, &ret, sizeof(ret));
		break;
	}
	default:
		/* Just ignore it. */
		break;
	}

	delete[] data;
}

/** Handle the connection being hung up. */
void ServiceManager::Connection::_ConnectionHangup() {
	/* FIXME: AUGH. */
	m_conn->Close();
	//delete m_conn;
	//delete this;
}

/** Service manager constructor. */
ServiceManager::ServiceManager() {
	Process proc;

	m_port.OnConnection.Connect(this, &ServiceManager::_HandleConnection);

	/* Create the port. TODO: Per-user instance. */
	if(!m_port.Create()) {
		cerr << "svcmgr: could not register port" << endl;
		throw exception();
	} else if(m_port.GetID() != 1) {
		cerr << "svcmgr: registered port (" << m_port.GetID() << ") is not port 1" << endl;
		throw exception();
	}
	m_port.GrantAccess(IPC_PORT_ACCESSOR_ALL, 0, IPC_PORT_RIGHT_CONNECT);
}

/** Add a service to the service manager.
 * @param service	Service to add. */
void ServiceManager::AddService(Service *service) {
	Service::PortList::const_iterator it;

	/* Register ports. */
	for(it = service->GetPorts().begin(); it != service->GetPorts().end(); ++it) {
		m_ports.insert(make_pair(*it, new Port(service)));
	}

	if(!(service->GetFlags() & Service::OnDemand)) {
		service->Start();
	}
}

/** Look up a port name in the port map.
 * @param name		Name to look up.
 * @return		Pointer to port object if found, 0 if not. */
Port *ServiceManager::LookupPort(const char *name) {
	PortMap::iterator it = m_ports.find(name);
	return (it != m_ports.end()) ? it->second : 0;
}

/** Handle a connection on the service manager port. */
void ServiceManager::_HandleConnection() {
	IPCConnection *conn;

	if((conn = m_port.Listen())) {
		new Connection(conn, this);
	}
}
