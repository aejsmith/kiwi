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

#include <kernel/process.h>

#include <iostream>

#include "Session.h"
#include "SessionManager.h"

using namespace kiwi;
using namespace std;

/** Whether the initial session has been created. */
bool Session::s_initial_created = false;

/** Construct a new session.
 * @param sessmgr	Session manager for the session.
 * @param perms		Permissions for the session. */
Session::Session(SessionManager *sessmgr, uint32_t perms) :
	m_sessmgr(sessmgr), m_permissions(perms)
{
	if(!s_initial_created) {
		m_id = 0;
		s_initial_created = true;
		return;
	}

	/* Arguments and environment for new process. */
	const char *const args[] = { "/system/services/svcmgr", 0 };
	const char *const env[] = {
		"PATH=/system/binaries",
		"HOME=/",
		0
	};
	handle_t map[][2] = {
		{ 0, 0 },
		{ 1, 1 },
		{ 2, 2 },
	};

	/* Execute the process. */
	handle_t handle;
	status_t ret = process_create(args[0], args, env, PROCESS_CREATE_SESSION, NULL,
	                              map, 3, NULL, PROCESS_QUERY, &handle);
	if(ret != STATUS_SUCCESS) {
		throw ProcessError(ret);
	}

	/* Save the session ID. */
	m_id = process_session(handle);

	/* Wrap the process in a Process object, and add an event handler for
	 * it dying. The session will be removed when this process dies. */
	m_process = new Process(handle);
	m_process->OnExit.Connect(this, &Session::ProcessExited);
}

/** Handle termination of session main process.
 * @param status	Exit status. */
void Session::ProcessExited(int status) {
	clog << "Session " << m_id << " main process terminated with status " << status <<endl;
	m_sessmgr->RemoveSession(this);
	DeleteLater();
}
