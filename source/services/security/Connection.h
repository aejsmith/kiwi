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
 * @brief		Security server connection class.
 */

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "org.kiwi.SecurityServer.h"

class Session;
class SecurityServer;

/** Class representing a connection to the security server. */
class Connection : public org::kiwi::SecurityServer::ClientConnection {
public:
	Connection(SecurityServer *server, Session *session, handle_t handle);

	/** Get the session the connection is from.
	 * @return		Session that connection is from. */
	Session *GetSession() const { return m_session; }
private:
	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);

	void HandleHangup();

	SecurityServer *m_server;	/**< Server the connection is on. */
	Session *m_session;		/**< Session that the connection is on. */
};

#endif /* __CONNECTION_H */
