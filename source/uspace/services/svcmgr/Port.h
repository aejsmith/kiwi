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

#ifndef __PORT_H
#define __PORT_H

#include <kiwi/IPCConnection.h>

#include <list>

#include "Service.h"

/** Class containing details of a port. */
class Port {
public:
	Port(Service *service);
	bool SetID(identifier_t id);
	void SendID(kiwi::IPCConnection *conn);
private:
	void _ServiceStopped();

	identifier_t m_id;		/**< Port ID. */
	Service *m_service;		/**< Service managing the port. */

	/** List of connections waiting for the port to be registered. */
	std::list<kiwi::IPCConnection *> m_waiting;
};

#endif /* __PORT_H */
