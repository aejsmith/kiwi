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

#include <kiwi/Application.h>
#include <kiwi/Error.h>

#include <cstdlib>
#include <iostream>

#include "org.kiwi.AppServer.h"

using namespace kiwi;
using namespace org::kiwi::AppServer::Session;
using namespace std;

/** Macro to get the connection pointer. */
#define GET_CONN()	reinterpret_cast<ServerConnection *>(m_conn)

/** Set up the application. */
Application::Application() :
	m_conn(0)
{
	/* Find the session port ID. */
	const char *var = getenv("APPSERVER_PORT");
	if(!var) {
		clog << "Could not find app sever port ID" << endl;
		throw OSError(STATUS_NOT_FOUND);
	}

	/* Set up a connection to the application server. */
	ServerConnection *conn = new ServerConnection(strtol(var, NULL, 10));
	m_conn = reinterpret_cast<void *>(conn);
}

/** Destroy the application. */
Application::~Application() {
	if(m_conn) {
		ServerConnection *conn = GET_CONN();
		delete conn;
	}
}
