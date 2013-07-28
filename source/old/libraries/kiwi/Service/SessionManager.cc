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

#include <kiwi/Service/SessionManager.h>
#include "org.kiwi.SecurityServer.h"

using namespace kiwi;
using namespace org::kiwi::SecurityServer;

/** Macro to get the session manager connection. */
#define GET_CONN()	(reinterpret_cast<ServerConnection *>(m_priv))

/** Connect to the session manager. */
SessionManager::SessionManager() {
	m_priv = new ServerConnection();
	GET_CONN()->OnCreateSession.Connect(OnCreateSession);
	GET_CONN()->OnDestroySession.Connect(OnDestroySession);
	GET_CONN()->OnSwitchSession.Connect(OnSwitchSession);
}

/** Destroy the connection. */
SessionManager::~SessionManager() {
	delete GET_CONN();
}

/** Create a new session.
 * @param id		Where to store ID of session.
 * @return		Status code describing result of the operation. */
status_t SessionManager::CreateSession(session_id_t &id) {
	return GET_CONN()->CreateSession(id);
}

/** Switch to a different session.
 * @param id		ID of session to switch to.
 * @return		Status code describing result of the operation. */
status_t SessionManager::SwitchSession(session_id_t id) {
	return GET_CONN()->SwitchSession(id);
}
