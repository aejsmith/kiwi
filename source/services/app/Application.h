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
 * @brief		Application class.
 */

#ifndef __APPLICATION_H
#define __APPLICATION_H

#include "org.kiwi.AppServer.h"
#include "Session.h"

/** Class representing an application's connection to the server. */
class Application : public org::kiwi::AppServer::Session::ClientConnection {
public:
	Application(Session *session, handle_t handle);
	~Application();
private:
	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);

	Session *m_session;		/**< Session that the application is running under. */
};

#endif /* __APPLICATION_H */
