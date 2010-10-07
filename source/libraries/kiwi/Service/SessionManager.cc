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

#include <kiwi/Service/SessionManager.h>
#include "org.kiwi.SessionManager.h"

using namespace kiwi;
using namespace org::kiwi::SessionManager;

/** Macro to get the session manager connection. */
#define GET_CONN()	(reinterpret_cast<ServerConnection *>(m_conn))

/** Connect to the session manager. */
SessionManager::SessionManager() {
	m_conn = new ServerConnection();
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
