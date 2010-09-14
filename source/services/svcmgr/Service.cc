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
#include <kernel/thread.h>

#include <iostream>

#include "org.kiwi.ServiceManager.h"
#include "Connection.h"
#include "Service.h"

using namespace kiwi;
using namespace std;

/** Data used by Service::Start() and Service::StartHelper(). */
struct start_info {
	port_id_t port;
	handle_t handle;
	volatile bool done;
};

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
	if(m_state == kRunning) {
		return true;
	}

	/* Create the handle map for the service. */
	Process::HandleMap map;
	map.push_back(make_pair(0, 0));
	map.push_back(make_pair(1, 1));
	map.push_back(make_pair(2, 2));

	/* Pass a connection to us as handle 3, which we use to communicate
	 * with the service. This is a pain to set up, we must connect to
	 * ourself. FIXME: OH GOD THIS IS AWFUL. */
	{
		IPCPort *server = m_svcmgr->GetPort();

		/* Create a thread that will connect to us. */
		start_info info = { server->GetID(), -1, false };
		handle_t handle;
		status_t ret = thread_create("svcinit", NULL, 0, &Service::StartHelper, &info, &handle);
		if(ret != STATUS_SUCCESS) {
			cerr << "svcmgr: failed to create helper thread (" << ret << ")" << endl;
			return false;
		}
		handle_close(handle);

		/* Wait for the connection. */
		handle = server->Listen();
		m_conn = new Connection(handle, m_svcmgr);
		while(!info.done) {}

		/* Add the connection handle to the port map. */
		map.push_back(make_pair(info.handle, 3));
	}

	/* If the service has a port, pass information about it to it. */
	if(m_port) {
		m_conn->AddPort(m_port->GetName(), m_port->GetID());
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

/** Helper for starting a service.
 * @param data		Data argument. */
void Service::StartHelper(void *data) {
	start_info *info = reinterpret_cast<start_info *>(data);
	ipc_connection_open(info->port, &info->handle);
	info->done = true;
	thread_exit(0);
}

/** Slot for the process exiting.
 * @param status	Exit status of the process. */
void Service::ProcessExited(int status) {
	m_conn = 0;
	cout << "svcmgr: service '" << m_name << "' exited with status " << status << endl;
	m_process.Close();
	m_state = kStopped;
}
