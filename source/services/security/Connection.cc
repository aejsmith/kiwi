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
 * @brief		Security server connection class.
 */

#include "Connection.h"
#include "SecurityServer.h"
#include "Session.h"

/** Create a connection object.
 * @param server	Server the connection is on.
 * @param session	Session the connection is from.
 * @param handle	Handle to the connection. */
Connection::Connection(SecurityServer *server, Session *session, handle_t handle) :
	org::kiwi::SecurityServer::ClientConnection(handle),
	m_server(server), m_session(session)
{
}

/** Create a new session.
 * @param id		Where to store ID of session.
 * @return		Status code describing result of the operation. */
status_t Connection::CreateSession(session_id_t &id) {
	Session *session;
	status_t ret;

	if(!m_session->HasPermission(Session::kCreatePermission)) {
		return STATUS_PERM_DENIED;
	}

	ret = m_server->CreateSession(session);
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

	return m_server->SwitchSession(id);
}

/** Handle the connection being hung up. */
void Connection::HandleHangup() {
	/* Remove us from the server. */
	m_server->RemoveConnection(this);
	DeleteLater();
}
