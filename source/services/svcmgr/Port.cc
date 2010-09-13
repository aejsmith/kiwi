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
 *
 * @todo		Remove port from the event loop while the service is
 *			running so that we don't get flooded with events if
 *			the service doesn't accept the connection for some
 *			reason.
 */

#include <stdexcept>
#include "Port.h"
#include "Service.h"

using namespace kiwi;
using namespace std;

/** Constructor for a port.
 * @param name		Name of the port.
 * @param service	Service that the port belongs to. */
Port::Port(const char *name, Service *service) :
	m_name(name), m_service(service)
{
	m_port.Create();
	m_port.OnConnection.Connect(this, &Port::HandleConnection);
}

/** Handle a connection on the port. */
void Port::HandleConnection() {
	/* If the service is not running, we must start it. */
	if(m_service->GetState() != Service::kRunning) {
		m_service->Start();
	}
}
