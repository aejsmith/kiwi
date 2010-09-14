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
 * @brief		Session class.
 */

#ifndef __SESSION_H
#define __SESSION_H

#include <kiwi/Process.h>

class SessionManager;

/** Class representing a session. */
class Session : public kiwi::Object {
public:
	/** Session permissions. */
	enum Permission {
		kCreatePermission = (1<<0),	/**< Allow session creation. */
		kSwitchPermission = (1<<1),	/**< Allow switching to sessions other than 0. */
	};

	Session(SessionManager *sessmgr, uint32_t perms);

	/** Get the ID of the session.
	 * @return		ID of the session. */
	session_id_t GetID() const { return m_id; }

	/** Check if the session has a permission.
	 * @param perm		Permission to check.
	 * @return		Whether the session has the permission. */
	bool HasPermission(uint32_t perm) const { return ((m_permissions & perm) == perm); }
private:
	void ProcessExited(int status);

	SessionManager *m_sessmgr;		/**< Session manager this session is on. */
	session_id_t m_id;			/**< ID of the session. */
	uint32_t m_permissions;			/**< Permissions of the session. */
	kiwi::Process *m_process;		/**< Main process for the session. */

	static bool s_initial_created;		/**< Whether the initial session has been created. */
};

#endif /* __SESSION_H */
