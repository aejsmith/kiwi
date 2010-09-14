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
 * @brief		Session manager connection class.
 */

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "org.kiwi.SessionManager.h"

class Session;
class SessionManager;

/** Class representing a connection to the session manager. */
class Connection : public org::kiwi::SessionManager::ClientConnection {
public:
	Connection(SessionManager *sessmgr, Session *session, handle_t handle);

	/** Get the session the connection is for.
	 * @return		Session that connection is for. */
	Session *GetSession() const { return m_session; }
private:
	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);

	void HandleHangup();

	SessionManager *m_sessmgr;	/**< Session manager that the connection is on. */
	Session *m_session;		/**< Session that the connection is on. */
};

#endif /* __CONNECTION_H */
