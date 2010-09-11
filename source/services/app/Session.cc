/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Session management.
 */

#include <kernel/object.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Application.h"
#include "AppServer.h"
#include "Session.h"

using namespace kiwi;
using namespace std;

extern char **environ;

/** Whether the initial session has been created. */
bool Session::s_initial_created = false;

/** Session constructor.
 * @param server	Server that the session belongs to.
 * @param path		Path to binary to run as initial session process. */
Session::Session(AppServer *server, const char *path) :
	m_server(server), m_id(-1)
{
	/* Attempt to create the session port. */
	m_port.Create();
	m_port.OnConnection.Connect(this, &Session::HandleConnection);

	/* Set the port number for the app to use. */
	char str[16];
	sprintf(str, "%d", m_port.GetID());
	setenv("APPSERVER_PORT", str, 1);

	/* Execute the process. */
	handle_t handle;
	const char *const args[] = { path, NULL };
	status_t ret = process_create(path, args, environ, (s_initial_created) ? PROCESS_CREATE_SESSION : 0,
	                              NULL, -1, &handle);
	unsetenv("APPSERVER_PORT");
	if(ret != STATUS_SUCCESS) {
		throw ProcessError(ret);
	}

	/* Save the session ID. */
	m_id = process_session(handle);
	handle_close(handle);

	s_initial_created = true;
}

/** Session destructor. */
Session::~Session() {
	m_server->RemoveSession(this);
}

/** Remove an application from the session.
 * @param app		Application to remove. */
void Session::RemoveApplication(Application *app) {
	clog << "AppServer: Application disconnected!" << endl;
	m_apps.remove(app);

	if(m_apps.empty()) {
		clog << "AppServer: No applications remaining in session " << m_id << ", destroying" << endl;
		m_port.Close();
		DeleteLater();
	}
}

/** Add a surface to the session.
 * @param surface	Surface to add. */
void Session::AddSurface(Surface *surface) {
	m_surfaces.insert(make_pair(surface->GetID(), surface));
}

/** Find a surface.
 * @param id		ID of surface to find.
 * @return		Pointer to surface if found, NULL if not. */
Surface *Session::FindSurface(area_id_t id) {
	SurfaceMap::iterator it = m_surfaces.find(id);
	return (it != m_surfaces.end()) ? it->second : 0;
}

/** Handle a connection to a session. */
void Session::HandleConnection() {
	handle_t handle;
	try {
		handle = m_port.Listen();
	} catch(...) {
		return;
	}
	clog << "AppServer: Connection from an application!" << endl;
	m_apps.push_back(new Application(this, handle));
}
