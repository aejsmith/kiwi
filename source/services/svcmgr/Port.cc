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
	if(!m_port.create()) {
		throw runtime_error("Failed to register new port");
	}
	m_port.onConnection.connect(this, &Port::handleConnection);
}

/** Handle a connection on the port. */
void Port::handleConnection() {
	/* If the service is not running, we must start it. */
	if(m_service->getState() != Service::Running) {
		m_service->start();
	}
}
