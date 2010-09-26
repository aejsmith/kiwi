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
 * @brief		Session manager interface.
 */

#ifndef __KIWI_SESSIONMANAGER_H
#define __KIWI_SESSIONMANAGER_H

#include <kernel/types.h>

#include <kiwi/Object.h>
#include <kiwi/Signal.h>

namespace kiwi {

/** Class to interface with the session manager. */
class SessionManager : public Object {
public:
	SessionManager();
	~SessionManager();

	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);
	
	Signal<session_id_t> OnCreateSession;
	Signal<session_id_t> OnDestroySession;
	Signal<session_id_t, session_id_t> OnSwitchSession;
private:
	void *m_conn;			/**< Connection implementation. */
};

}

#endif /* __KIWI_SESSIONMANAGER_H */
