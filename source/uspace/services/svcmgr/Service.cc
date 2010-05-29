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

#include <iostream>

#include "Service.h"

using namespace kiwi;
using namespace std;

/** Constructor for Service. */
Service::Service(const char *name, const char *description, const char *cmdline, int flags) :
	m_name(name), m_description(description), m_cmdline(cmdline),
	m_flags(flags), m_state(Stopped)
{
	m_process.onExit.connect(this, &Service::processExited);
}

/** Add a port name to the service.
 * @note		Ports should not be added after the service has been
 *			added to the service manager.
 * @param name		Port name. */
void Service::addPort(const char *name) {
	m_ports.push_back(name);
}

/** Start the service.
 * @return		Whether successful. */
bool Service::start() {
	if(m_state == Running) {
		return true;
	} else if(!m_process.create(m_cmdline.c_str())) {
		cerr << "svcmgr: failed to start service '" << m_name << "'" << endl;
		return false;
	}

	m_state = Running;
	return true;
}

/** Slot for the process exiting.
 * @param status	Exit status of the process. */
void Service::processExited(Process *, int status) {
	cout << "svcmgr: service '" << m_name << "' exited with status " << status << endl;
	m_process.close();
	m_state = Stopped;
	onStop();
}
