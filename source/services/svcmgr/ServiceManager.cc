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

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "Connection.h"
#include "ServiceManager.h"

using namespace kiwi;
using namespace std;

/** Instance of the service manager. */
ServiceManager *ServiceManager::s_instance = NULL;

/** Service manager constructor. */
ServiceManager::ServiceManager() : IPCServer(-1) {
	s_instance = this;
	if(GetPortID() != 1) {
		ostringstream msg;
		msg << "Created port (" << GetPortID() << ") is not port 1.";
		throw std::runtime_error(msg.str());
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
 * @param name		Name to look up.
 * @return		Pointer to port object if found, 0 if not. */
Port *ServiceManager::LookupPort(const string &name) {
	PortMap::iterator it = m_ports.find(name);
	return (it != m_ports.end()) ? it->second : 0;
}

/** Handle a connection on the service manager port. */
void ServiceManager::HandleConnection(handle_t handle) {
	new Connection(handle);
}

/** Main function for the service manager.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Should not return. */
int main(int argc, char **argv) {
	ServiceManager svcmgr;

	/* Add services. TODO: These should be in configuration files. */
	svcmgr.AddService(new Service(
		"console",
		"Service providing a graphical console.",
		"/system/services/console"
	));
	svcmgr.AddService(new Service(
		"pong",
		"Service that pongs pings.",
		"/system/services/pong",
		Service::kOnDemand,
		"org.kiwi.Pong"
	));
	svcmgr.AddService(new Service(
		"shmserver",
		"Shared memory test server.",
		"/system/services/shmserver",
		Service::kOnDemand,
		"org.kiwi.SHMServer"
	));
	svcmgr.AddService(new Service(
		"kittenserver",
		"Kitten server.",
		"/system/services/kittenserver",
		Service::kOnDemand,
		"org.kiwi.KittenServer"
	));

	svcmgr.Run();
	return 0;
}
