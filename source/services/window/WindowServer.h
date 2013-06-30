/*
 * Copyright (C) 2010 Alex Smith
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
