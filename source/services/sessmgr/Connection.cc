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
 * @brief		Session manager connection class.
 */

#include "Connection.h"
#include "Session.h"
#include "SessionManager.h"

/** Create a connection object.
 * @param sessmgr	Session manager the connection is on.
 * @param session	Session the connection is from.
 * @param handle	Handle to the connection. */
Connection::Connection(SessionManager *sessmgr, Session *session, handle_t handle) :
	org::kiwi::SessionManager::ClientConnection(handle),
	m_sessmgr(sessmgr), m_session(session)
{
}

/** Create a new session.
 * @param id		Where to store ID of session.
 * @return		Status code describing result of the operation. */
status_t Connection::CreateSession(session_id_t &id) {
	if(!m_session->HasPermission(Session::kCreatePermission)) {
		return STATUS_PERM_DENIED;
	}

	Session *session;
	status_t ret = m_sessmgr->CreateSession(session);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	id = session->GetID();
	return STATUS_SUCCESS;
}

/** Switch to a different session.
 * @param id		ID of session to switch to.
 * @return		Status code describing result of the operation. */
status_t Connection::SwitchSession(session_id_t id) {
	if(id != 0 && !m_session->HasPermission(Session::kSwitchPermission)) {
		return STATUS_PERM_DENIED;
	}

	return m_sessmgr->SwitchSession(id);
}

/** Handle the connection being hung up. */
void Connection::HandleHangup() {
	/* Remove us from the server. */
	m_sessmgr->RemoveConnection(this);
	DeleteLater();
}
