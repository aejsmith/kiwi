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
 * @brief		Session manager interface.
 */

#ifndef __KIWI_SERVICE_SESSIONMANAGER_H
#define __KIWI_SERVICE_SESSIONMANAGER_H

#include <kiwi/Object.h>
#include <kiwi/Signal.h>

namespace kiwi {

/** Class to interface with the session manager. */
class KIWI_PUBLIC SessionManager : public Object {
public:
	SessionManager();
	~SessionManager();

	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);

	/** Signal emitted when a new session is created.
	 * @param		ID of the session. */
	Signal<session_id_t> OnCreateSession;

	/** Signal emitted when a session is destroyed.
	 * @param		ID of the session. */
	Signal<session_id_t> OnDestroySession;

	/** Signal emitted when a session switch occurs.
	 * @param		ID of the new session.
	 * @param		ID of the previous session. */
	Signal<session_id_t, session_id_t> OnSwitchSession;
private:
	void *m_priv;			/**< Connection implementation. */
};

}

#endif /* __KIWI_SERVICE_SESSIONMANAGER_H */
