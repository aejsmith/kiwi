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

#ifndef __SERVICEMANAGER_H
#define __SERVICEMANAGER_H

#include <kiwi/IPCServer.h>
#include <kiwi/IPCConnection.h>

#include <map>
#include <list>
#include <string>

#include "Port.h"
#include "Service.h"

/** Class implementing the service manager. */
class ServiceManager : public kiwi::IPCServer {
	/** Type for the port map. */
	typedef std::map<std::string, Port *> PortMap;
public:
	ServiceManager();

	void addService(Service *service);
	Port *lookupPort(const std::string &name);

	/** Get the instance of the service manager.
	 * @return		Reference to service manager instance. */
	static ServiceManager &instance() { return *s_instance; }
private:
	void handleConnection(handle_t handle);

	std::list<Service *> m_services;	/**< List of services. */
	PortMap m_ports;			/**< Map of port names to port objects. */

	static ServiceManager *s_instance;
};

#endif /* __SERVICEMANAGER_H */
