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
 * @brief		Window server.
 */

#ifndef __WINDOWSERVER_H
#define __WINDOWSERVER_H

#include <kiwi/Service/Service.h>
#include <kiwi/Service/SessionManager.h>

#include <map>

class Display;
class InputManager;
class Session;

/** Main class for the window server. */
class WindowServer : public kiwi::Service {
	/** Type of the session map. */
	typedef std::map<session_id_t, Session *> SessionMap;
public:
	WindowServer();

	void RemoveSession(Session *session);

	/** Get the active session.
	 * @return		Pointer to the active session. */
	Session *GetActiveSession() const { return m_active; }

	/** Get the display.
	 * @return		Pointer to display. */
	Display *GetDisplay() const { return m_display; }
private:
	Session *LookupSession(session_id_t id);
	void HandleConnection(handle_t handle, port_client_t &info);
	void SwitchSession(session_id_t id, session_id_t prev);

	kiwi::SessionManager *m_sessmgr;	/**< Connection to session manager. */
	SessionMap m_sessions;			/**< Map of known sessions. */
	Session *m_active;			/**< Currently active session. */
	Display *m_display;			/**< Display the server is using. */
	InputManager *m_inputmgr;		/**< Input manager. */
};

#endif /* __WINDOWSERVER_H */
