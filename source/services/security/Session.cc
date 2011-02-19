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

#include <kernel/process.h>

#include <iostream>

#include "SecurityServer.h"
#include "Session.h"

using namespace kiwi;
using namespace std;

/** Whether the initial session has been created. */
bool Session::s_initial_created = false;

/** Construct a new session.
 * @param server	Server for the session.
 * @param perms		Permissions for the session. */
Session::Session(SecurityServer *server, uint32_t perms) :
	m_server(server), m_permissions(perms)
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
	status_t ret = kern_process_create(args[0], args, env, PROCESS_CREATE_SESSION, NULL,
	                                   map, 3, NULL, PROCESS_RIGHT_QUERY, &handle);
	if(ret != STATUS_SUCCESS) {
		throw Error(ret);
	}

	/* Save the session ID. */
	m_id = kern_process_session(handle);

	/* Wrap the process in a Process object, and add an event handler for
	 * it dying. The session will be removed when this process dies. */
	m_process = new Process(handle);
	m_process->OnExit.Connect(this, &Session::ProcessExited);
}

/** Handle termination of session main process.
 * @param status	Exit status. */
void Session::ProcessExited(int status) {
	clog << "Session " << m_id << " main process terminated with status " << status <<endl;
	// FIXME: We should not remove the session until the kernel session
	// disappears.
	m_server->RemoveSession(this);
	DeleteLater();
}
