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
 * @brief		Kiwi application server.
 */

#include <iostream>

#include "Connection.h"
#include "AppServer.h"

using namespace std;

/** Construct the application server. */
AppServer::AppServer() {
	/* Open the display. TODO: Multi-display support. */
	m_display = new Display("/display/0");
}

/** Create a new session.
 * @param path		Path to binary to run as initial session process.
 * @return		Pointer to new session. */
Session *AppServer::CreateSession(const char *path) {
	Session *session = new Session(this, path);
	m_sessions.insert(make_pair(session->GetID(), session));

	/* Notify connections. */
	for(auto it = m_connections.begin(); it != m_connections.end(); ++it) {
		(*it)->OnCreateSession(session->GetID());
	}

	clog << "AppServer: Created session " << session->GetID() << endl;
	return session;
}

/** Remove a session from the server.
 * @param session	Session to remove. */
void AppServer::RemoveSession(Session *session) {
	/* Notify connections. */
	for(auto it = m_connections.begin(); it != m_connections.end(); ++it) {
		(*it)->OnDestroySession(session->GetID());
	}

	m_sessions.erase(session->GetID());
	clog << "AppServer: Destroyed session " << session->GetID() << endl;
}

/** Remove a connection from the server.
 * @param conn		Connection to remove. */
void AppServer::RemoveConnection(Connection *conn) {
	m_connections.remove(conn);
}

/** Handle a connection to the application server.
 * @param handle	Handle to the connection. */
void AppServer::HandleConnection(handle_t handle) {
	m_connections.push_back(new Connection(this, handle));
}

/** Main function for the application server.
 * @param argc		Argument count.
 * @param argv		Argument array. */
int main(int argc, char **argv) {
	AppServer server;

	/* Create the initial session. */
	server.CreateSession("/system/binaries/uitest");

	/* Run the event loop. */
	server.Run();
	return 0;
}
