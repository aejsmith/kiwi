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

#include <assert.h>

#include "Port.h"
#include "ServiceManager.h"

using namespace kiwi;
using namespace std;

/** Constructor for a port. */
Port::Port(Service *service) : m_id(-1), m_service(service) {}

/** Set the ID of a port.
 * @param id		New ID. */
void Port::SetID(identifier_t id) {
	list<IPCConnection *>::iterator it;
	IPCConnection *conn;

	assert(m_service->GetState() == Service::Running);

	m_id = id;

	/* Send a message for all waiting connections. FIXME: If any of these
	 * connections have hung up then the pointer will be invalid. */
	while((it = m_waiting.begin()) != m_waiting.end()) {
		conn = *it;
		m_waiting.erase(it);
		SendID(conn);
	}
}

/** Send the ID of a port on a connection.
 * @note		The ID may not be sent immediately if the service
 *			needs to be started. */
void Port::SendID(IPCConnection *conn) {
	if(m_service->GetState() == Service::Running) {
		conn->Send(SVCMGR_LOOKUP_PORT, reinterpret_cast<char *>(&m_id), sizeof(m_id));
	} else {
		/* Start it and wait for the port to be registered. */
		m_service->Start();
		m_waiting.push_back(conn);
	}
}
