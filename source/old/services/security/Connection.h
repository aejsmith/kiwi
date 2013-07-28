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
