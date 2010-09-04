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
 * @brief		Application class.
 */

#include <kernel/status.h>

#include <iostream>

#include "Application.h"
#include "Session.h"

using namespace std;
using namespace kiwi;

/** Construct an application object.
 * @param session	Session that the application is running under.
 * @param handle	Handle to the connection. */
Application::Application(Session *session, handle_t handle) :
	org::kiwi::AppServer::Session::ClientConnection(handle),
	m_session(session)
{
}

/** Destroy an application connection. */
Application::~Application() {
	/* Remove us from the session. */
	m_session->RemoveApplication(this);
}

/** Create a new session.
 * @param id		Where ID of new session will be placed.
 * @return		Status code describing result of the operation. */
status_t Application::CreateSession(session_id_t &id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Switch to a different session.
 * @param id		ID of session to switch to.
 * @return		Status code describing result of the operation. */
status_t Application::SwitchSession(session_id_t id) {
	return STATUS_NOT_IMPLEMENTED;
}
