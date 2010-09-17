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
 * @brief		Service manager port class.
 */

#ifndef __PORT_H
#define __PORT_H

#include <kiwi/IPCPort.h>
#include <string>

class Service;

/** Class containing details of a port. */
class Port {
public:
	Port(const char *name, Service *service);

	void StartListening();
	void StopListening();

	/** Get the name of the port.
	 * @return		Name of the port. */
	const std::string &GetName() const { return m_name; }

	/** Get the ID of the port.
	 * @return		ID of the port. */
	port_id_t GetID() const { return m_port.GetID(); }
private:
	void HandleConnection();

	std::string m_name;		/**< Name of the port. */
	kiwi::IPCPort m_port;		/**< Handle to the port. */
	Service *m_service;		/**< Service that the port belongs to. */
};

#endif /* __PORT_H */
