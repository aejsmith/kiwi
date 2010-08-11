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
 * @brief		Service manager service class.
 */

#include <kernel/ipc.h>
#include <kernel/object.h>

#include <iostream>

#include "Service.h"

using namespace kiwi;
using namespace std;

/** Constructor for a service object.
 * @param name		Name of the service.
 * @param desc		Description of the service.
 * @param cmdline	How to invoke the service.
 * @param flags		Flags for the service.
 * @param port		Name of the service's port (if any). */
Service::Service(const char *name, const char *desc, const char *cmdline, int flags, const char *port) :
	m_name(name), m_description(desc), m_cmdline(cmdline), m_flags(flags),
	m_port(0), m_state(kStopped)
{
	if(port) {
		m_port = new Port(port, this);
	}
	m_process.OnExit.Connect(this, &Service::ProcessExited);
}

/** Start the service.
 * @return		Whether successful. */
bool Service::Start() {
	if(m_state == kRunning) {
		return true;
	}

	/* Create the handle map for the service. */
	Process::HandleMap map;
	map.push_back(make_pair(0, 0));
	map.push_back(make_pair(1, 1));
	map.push_back(make_pair(2, 2));

	/* If the service has a port, pass a handle to it as handle 3. */
	IPCPort port;
	if(m_port) {
		try {
			port.Open(m_port->GetID());
		} catch(Error &e) {
			cerr << "svcmgr: failed to open port for service '" << m_name << "': ";
			cerr << e.GetDescription() << endl;
			return false;
		}
		map.push_back(make_pair(port.GetHandle(), 3));
	}

	/* Create the process. */
	try {
		m_process.Create(m_cmdline.c_str(), environ, &map);
	} catch(Error &e) {
		cerr << "svcmgr: failed to start service '" << m_name << "': ";
		cerr << e.GetDescription() << endl;
		return false;
	}

	m_state = kRunning;
	return true;
}

/** Slot for the process exiting.
 * @param status	Exit status of the process. */
void Service::ProcessExited(int status) {
	cout << "svcmgr: service '" << m_name << "' exited with status " << status << endl;
	m_process.Close();
	m_state = kStopped;
}
