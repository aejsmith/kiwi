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

#ifndef __SERVICEMANAGER_H
#define __SERVICEMANAGER_H

#include <kiwi/private/svcmgr.h>
#include <kiwi/EventLoop.h>
#include <kiwi/IPCConnection.h>
#include <kiwi/IPCPort.h>

#include <map>
#include <list>
#include <string>

#include "Port.h"
#include "Service.h"

/** Class implementing the service manager. */
class ServiceManager : public kiwi::EventLoop {
	/** Type for the port map. */
	typedef std::map<std::string, Port *> PortMap;

	/** Class representing a connection to the service manager. */
	class Connection {
	public:
		Connection(kiwi::IPCConnection *conn, ServiceManager *svcmgr);
	private:
		void _MessageReceived();
		void _ConnectionHangup();

		kiwi::IPCConnection *m_conn;	/**< Connection structure. */
		ServiceManager *m_svcmgr;	/**< Service manager. */
	};
public:
	ServiceManager();

	void AddService(Service *service);
	Port *LookupPort(const char *name);
private:
	void _HandleConnection();

	kiwi::IPCPort m_port;			/**< Service manager port. */
	std::list<Service *> m_services;	/**< List of services. */
	PortMap m_ports;			/**< Map of port names to port objects. */
};

#endif /* __SERVICEMANAGER_H */
