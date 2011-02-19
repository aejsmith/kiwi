/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Service manager service class.
 */

#include <kernel/ipc.h>
#include <kernel/object.h>

#include <iostream>
#include <sstream>

#include "org.kiwi.ServiceManager.h"
#include "Connection.h"
#include "Service.h"

using namespace kiwi;
using namespace std;

/** Constructor for a service object.
 * @param svcmgr	Service manager the service is for.
 * @param name		Name of the service.
 * @param desc		Description of the service.
 * @param cmdline	How to invoke the service.
 * @param flags		Flags for the service.
 * @param port		Name of the service's port (if any). */
Service::Service(ServiceManager *svcmgr, const char *name, const char *desc, const char *cmdline,
                 int flags, const char *port) :
	m_svcmgr(svcmgr), m_name(name), m_description(desc), m_cmdline(cmdline),
	m_flags(flags), m_port(0), m_state(kStopped)
{
	if(port) {
		m_port = new Port(port, this);
	}
	m_process.OnExit.Connect(this, &Service::ProcessExited);
}

/** Start the service.
 * @return		Whether successful. */
bool Service::Start() {
	handle_t handles[2];
	status_t ret;

	if(m_state == kRunning) {
		return true;
	}

	/* Create the handle map for the service. */
	Process::HandleMap map;
	map.push_back(make_pair(0, 0));
	map.push_back(make_pair(1, 1));
	map.push_back(make_pair(2, 2));

	/* Create the service control connection. */
	ret = kern_port_loopback(m_svcmgr->GetPort()->GetHandle(), handles);
	if(ret != STATUS_SUCCESS) {
		cerr << "svcmgr: failed to create service control connection (" << ret << ")" << endl;
		return false;
	}

	m_conn = new Connection(handles[0], m_svcmgr);

	/* Give a handle to this connection as handle 3 in the service. */
	map.push_back(make_pair(handles[1], 3));

	/* If the service has a port, send information about it to it. */
	if(m_port) {
		m_conn->AddPort(m_port->GetName(), m_port->GetID());
	}

	/* Create the process. TODO: Drop capabilities that services should not
	 * have, particularly CAP_FATAL. */
	if(!m_process.Create(m_cmdline.c_str(), environ, &map)) {
		cerr << "svcmgr: failed to start service '" << m_name << "': ";
		cerr << m_process.GetError().GetDescription() << endl;
		return false;
	}

	/* No longer need the client end of the connection. */
	kern_handle_close(handles[1]);

	if(m_port) {
		m_port->StopListening();
	}
	m_state = kRunning;
	return true;
}

/** Slot for the process exiting.
 * @param status	Exit status of the process. */
void Service::ProcessExited(int status) {
	/* TODO: Re-enable this when we have a proper shutdown implemented. */
	//if(m_flags & kCritical) {
	//	std::stringstream str;
	//	str << "Critical service '" << m_name << "' exited with status " << status;
	//	system_fatal(str.str().c_str());
	//}

	m_conn = 0;
	cout << "svcmgr: service '" << m_name << "' exited with status " << status << endl;
	m_process.Close();
	m_state = kStopped;

	if(m_port) {
		m_port->StartListening();
	}
}
