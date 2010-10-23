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
 * @brief		Security server.
 */

#include <kernel/object.h>

#include <cassert>
#include <algorithm>
#include <iostream>

#include "Connection.h"
#include "SecurityServer.h"
#include "Session.h"

using namespace std;

/** Construct the security server. */
SecurityServer::SecurityServer() {
	/* Add the initial session. Session 0 is the only session that can
	 * create new sessions and switch to sessions other than 0. */
	Session *session = new Session(this, Session::kCreatePermission | Session::kSwitchPermission);
	assert(session->GetID() == 0);
	m_sessions.insert(make_pair(0, session));
	m_active_session = session;
}

/** Create a new session.
 * @param session	Where to store pointer to new session.
 * @return		Status code describing result of the operation. */
status_t SecurityServer::CreateSession(Session *&session) {
	/* Create the session. */
	try {
		session = new Session(this, 0);
	} catch(kiwi::OSError &e) {
		return e.GetCode();
	}
	m_sessions.insert(make_pair(session->GetID(), session));

	/* Notify connections. */
	for(auto it = m_connections.begin(); it != m_connections.end(); ++it) {
		(*it)->OnCreateSession(session->GetID());
	}

	return STATUS_SUCCESS;
}

/** Switch to a different session.
 * @param id		ID of session to switch to.
 * @return		Status code describing result of the operation. */
status_t SecurityServer::SwitchSession(session_id_t id) {
	SessionMap::iterator it = m_sessions.find(id);
	if(it == m_sessions.end()) {
		return STATUS_NOT_FOUND;
	}

	/* Switch the session if it is not already active. */
	Session *session = it->second;
	if(m_active_session == session) {
		return STATUS_SUCCESS;
	}
	swap(m_active_session, session);

	/* Notify connections. */
	for(auto it = m_connections.begin(); it != m_connections.end(); ++it) {
		(*it)->OnSwitchSession(m_active_session->GetID(), session->GetID());
	}

	return STATUS_SUCCESS;
}

/** Remove a session.
 * @param session	Session to remove. */
void SecurityServer::RemoveSession(Session *session) {
	/* Switch to session 0 if the session is active. */
	if(session == m_active_session) {
		SwitchSession(0);
	}

	/* Notify connections. */
	for(auto it = m_connections.begin(); it != m_connections.end(); ++it) {
		(*it)->OnDestroySession(session->GetID());
	}

	m_sessions.erase(session->GetID());
}

/** Remove a connection.
 * @param conn		Connection to remove. */
void SecurityServer::RemoveConnection(Connection *conn) {
	m_connections.remove(conn);
}

/** Handle a connection to the security server.
 * @param handle	Handle to the connection.
 * @param info		Information about the connecting thread. */
void SecurityServer::HandleConnection(handle_t handle, ipc_client_info_t &info) {
	SessionMap::iterator it = m_sessions.find(info.sid);
	if(it == m_sessions.end()) {
		system_fatal("Received connection from unknown session");
	}

	m_connections.push_back(new Connection(this, it->second, handle));
}

/** Main function for the security server.
 * @param argc		Argument count.
 * @param argv		Argument array. */
int main(int argc, char **argv) {
	SecurityServer server;
	server.Run();
	return 0;
}
