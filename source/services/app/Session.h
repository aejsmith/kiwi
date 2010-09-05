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

#ifndef __SESSION_H
#define __SESSION_H

#include <kiwi/IPCPort.h>

#include <list>

#include "org.kiwi.AppServer.h"

class AppServer;
class Application;

/** Class representing a session. */
class Session : public kiwi::Object {
	/** Type of the application list. */
	typedef std::list<Application *> ApplicationList;
public:
	Session(AppServer *server, const char *path);
	~Session();

	void RemoveApplication(Application *app);

	/** Get the ID of the session.
	 * @return		ID of the session. */
	session_id_t GetID() const { return m_id; }
private:
	void HandleConnection();

	AppServer *m_server;		/**< Server that the connection is for. */
	session_id_t m_id;		/**< ID of the session. */
	kiwi::IPCPort m_port;		/**< Port for the session. */
	ApplicationList m_apps;		/**< Applications running in the session. */

	static bool s_initial_created;	/**< Whether the initial session has been created. */
};

#endif /* __SESSION_H */
