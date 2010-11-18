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

#include <kernel/system.h>

#include <kiwi/EventLoop.h>
#include <kiwi/IPCConnection.h>
#include <kiwi/IPCPort.h>

#include <map>
#include <list>
#include <string>

#include "org.kiwi.ServiceManager.h"
#include "Port.h"
#include "Service.h"

/** Class implementing the service manager. */
class ServiceManager : public kiwi::EventLoop {
	/** Type for the port map. */
	typedef std::map<std::string, Port *> PortMap;
public:
	ServiceManager();

	void AddService(Service *service);
	Port *LookupPort(const std::string &name);
	bool LookupPort(const std::string &name, port_id_t &id);

	/** Get the port.
	 * @return		Service manager's port. */
	kiwi::IPCPort *GetPort() { return &m_port; }

	/** Return whether the server is a session instance.
	 * @return		Whether the server is a session instance. */
	bool IsSessionInstance() const { return (m_parent != 0); }
private:
	void HandleConnection();

	kiwi::IPCPort m_port;			/**< Server port. */
	std::list<Service *> m_services;	/**< List of services. */
	PortMap m_ports;			/**< Map of port names to port objects. */

	/** Connection to global instance. */
	org::kiwi::ServiceManager::ServerConnection *m_parent;
};

#endif /* __SERVICEMANAGER_H */
