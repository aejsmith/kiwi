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
 * @brief		Session manager.
 */

#ifndef __SESSIONMANAGER_H
#define __SESSIONMANAGER_H

#include <kiwi/Service/Service.h>

#include <list>
#include <map>

class Connection;
class Session;

/** Main class for the session manager. */
class SessionManager : public kiwi::Service {
	/** Type of the session map. */
	typedef std::map<session_id_t, Session *> SessionMap;

	/** Type of the connection list. */
	typedef std::list<Connection *> ConnectionList;
public:
	SessionManager();

	status_t CreateSession(Session *&session);
	status_t SwitchSession(session_id_t id);

	void RemoveSession(Session *session);
	void RemoveConnection(Connection *conn);
private:
	void HandleConnection(handle_t handle, ipc_connect_info_t &info);

	SessionMap m_sessions;		/**< Map of known sessions. */
	ConnectionList m_connections;	/**< Connections to the server. */
	Session *m_active;		/**< Currently active session. */
};

#endif /* __SESSIONMANAGER_H */
