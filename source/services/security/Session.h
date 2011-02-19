/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Session class.
 */

#ifndef __SESSION_H
#define __SESSION_H

#include <kiwi/Process.h>

class SecurityServer;

/** Class representing a session. */
class Session : public kiwi::Object {
public:
	/** Session permissions. */
	enum Permission {
		kCreatePermission = (1<<0),	/**< Allow session creation. */
		kSwitchPermission = (1<<1),	/**< Allow switching to sessions other than 0. */
	};

	Session(SecurityServer *server, uint32_t perms);

	/** Get the ID of the session.
	 * @return		ID of the session. */
	session_id_t GetID() const { return m_id; }

	/** Check if the session has a permission.
	 * @param perm		Permission to check.
	 * @return		Whether the session has the permission. */
	bool HasPermission(uint32_t perm) const { return ((m_permissions & perm) == perm); }
private:
	void ProcessExited(int status);

	SecurityServer *m_server;		/**< Server this session is on. */
	session_id_t m_id;			/**< ID of the session. */
	uint32_t m_permissions;			/**< Permissions of the session. */
	kiwi::Process *m_process;		/**< Main process for the session. */

	static bool s_initial_created;		/**< Whether the initial session has been created. */
};

#endif /* __SESSION_H */
