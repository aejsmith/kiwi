/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
