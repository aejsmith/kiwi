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

#ifndef __APPSERVER_H
#define __APPSERVER_H

#include <kiwi/IPCServer.h>
#include <list>
#include <map>

#include "Connection.h"
#include "Display.h"
#include "Session.h"

/** Class implementing the application server. */
class AppServer : public kiwi::IPCServer {
	/** Type of the session map. */
	typedef std::map<session_id_t, Session *> SessionMap;

	/** Type of the connection list. */
	typedef std::list<Connection *> ConnectionList;
public:
	AppServer();

	Session *CreateSession(const char *path);
	void RemoveSession(Session *session);
	void RemoveConnection(Connection *conn);
private:
	void HandleConnection(handle_t handle);

	SessionMap m_sessions;			/**< Map of sessions. */
	ConnectionList m_connections;		/**< Connections to the server. */
	Display *m_display;			/**< Display that the server is using. */
};

#endif /* __APPSERVER_H */
