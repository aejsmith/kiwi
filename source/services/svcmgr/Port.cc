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
 * @brief		Service manager port class.
 *
 * @todo		Remove port from the event loop while the service is
 *			running so that we don't get flooded with events if
 *			the service doesn't accept the connection for some
 *			reason.
 */

#include <kiwi/EventLoop.h>
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
	/* TODO: If a session instance, configure the ACL to only allow
	 * connections from the session. */
	m_port.Create();
	m_port.OnConnection.Connect(this, &Port::HandleConnection);
}

/** Start listening for connections on the port. */
void Port::StartListening() {
	m_port.InhibitEvents(false);
}

/** Stop listening for connections on the port. */
void Port::StopListening() {
	m_port.InhibitEvents(true);
}

/** Handle a connection on the port. */
void Port::HandleConnection() {
	/* If the service is not running, we must start it. */
	if(m_service->GetState() != Service::kRunning) {
		m_service->Start();
	}
}
