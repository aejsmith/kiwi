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
 * @brief		Service manager.
 */

#include <kiwi/Error.h>

#include <cassert>
#include <iostream>
#include <sstream>

#include "Connection.h"
#include "ServiceManager.h"

using namespace kiwi;
using namespace std;

/** Service manager constructor. */
ServiceManager::ServiceManager() :
	m_parent(0)
{
	/* Create the port. */
	m_port.Create();
	m_port.OnConnection.Connect(this, &ServiceManager::HandleConnection);

	/* If the port is port 1, then we are the global instance. Otherwise,
	 * we are a session instance, in which case we must connect to the
	 * global instance. */
	if(m_port.GetID() != 1) {
		m_parent = new org::kiwi::ServiceManager::ServerConnection(1);
	}
}

/** Add a service to the service manager.
 * @param service	Service to add. */
void ServiceManager::AddService(Service *service) {
	/* Register port. */
	Port *port = service->GetPort();
	if(port) {
		m_ports.insert(make_pair(port->GetName(), port));
	}

	/* Start the service if it is not on-demand. */
	if(!(service->GetFlags() & Service::kOnDemand)) {
		service->Start();
	}
}

/** Look up a port name in the port map.
 *
 * Looks up a port in this service manager instance. Does not look up in the
 * parent instance if not found, if this is desired use the other form of
 * LookupPort().
 *
 * @param name		Name to look up.
 *
 * @return		Pointer to port object if found, NULL if not.
 */
Port *ServiceManager::LookupPort(const string &name) {
	PortMap::iterator it = m_ports.find(name);
	return (it != m_ports.end()) ? it->second : 0;
}

/** Look up a port.
 *
 * Looks up a port, and attempts to look it up in the parent instance if not
 * found in this instance.
 *
 * @param name		Name to look up.
 * @param id		Where to store ID of port.
 *
 * @return		Whether port was found.
 */
bool ServiceManager::LookupPort(const std::string &name, port_id_t &id) {
	Port *port = LookupPort(name);
	if(port) {
		id = port->GetID();
		return true;
	}

	/* Look up in the parent. */
	if(m_parent && m_parent->LookupPort(name, id) == STATUS_SUCCESS) {
		return true;
	}
	return false;
}

/** Handle a connection on the service manager port. */
void ServiceManager::HandleConnection() {
	handle_t handle;
	try {
		handle = m_port.Listen();
	} catch(...) {
		return;
	}
	new Connection(handle, this);
}

/** Main function for the service manager.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Should not return. */
int main(int argc, char **argv) {
	ServiceManager svcmgr;

	/* Add services. TODO: These should be in configuration files. */
	if(!svcmgr.IsSessionInstance()) {
		svcmgr.AddService(new Service(
			&svcmgr,
			"sessmgr",
			"Session manager.",
			"/system/services/sessmgr",
			0,
			"org.kiwi.SessionManager"
		));
		svcmgr.AddService(new Service(&svcmgr, "console", "Console.", "/system/services/console"));
	}

	svcmgr.Run();
	return 0;
}
