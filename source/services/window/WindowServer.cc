/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Window server.
 */

#include <algorithm>

#include "Display.h"
#include "InputManager.h"
#include "Session.h"
#include "WindowServer.h"

using namespace kiwi;
using namespace std;

/** Construct the window server. */
WindowServer::WindowServer() : m_active(0) {
	/* Set up the display. TODO: Multi-display support. */
	m_display = new Display(this, "/display/0");

	/* Connect to the session manager to get switch notifications. */
	m_sessmgr = new SessionManager();
	m_sessmgr->OnSwitchSession.Connect(this, &WindowServer::SwitchSession);

	/* Activate session 0. */
	SwitchSession(0, 0);

	/* Create the input device manager. */
	m_inputmgr = new InputManager(this);
}

/** Remove a session from the server.
 * @param session	Session to remove. */
void WindowServer::RemoveSession(Session *session) {
	m_sessions.erase(session->GetID());
}

/** Look up a session, creating it if it doesn't exist.
 * @param id		ID of session to look up.
 * @return		Pointer to session. */
Session *WindowServer::LookupSession(session_id_t id) {
	SessionMap::iterator it = m_sessions.find(id);
	if(it == m_sessions.end()) {
		/* Not seen this session before, add it. */
		Session *session = new Session(this, id);
		it = m_sessions.insert(make_pair(id, session)).first;
	}

	return it->second;
}

/** Handle a connection to the window server.
 * @param handle	Handle to the connection.
 * @param info		Information about the connecting thread. */
void WindowServer::HandleConnection(handle_t handle, port_client_t &info) {
	/* Get the session to accept the connection. */
	LookupSession(info.sid)->HandleConnection(handle);
}

/** Handle a session switch.
 * @param id		ID of new session.
 * @param prev		Previous session ID. */
void WindowServer::SwitchSession(session_id_t id, session_id_t prev) {
	/* Find the session to switch to. */
	Session *session = LookupSession(id);

	if(m_active) {
		m_active->Deactivate();
	}
	swap(m_active, session);
	m_active->Activate();
}

/** Main function for the window server.
 * @param argc		Argument count.
 * @param argv		Argument array. */
int main(int argc, char **argv) {
	WindowServer server;
	server.Run();
	return 0;
}
