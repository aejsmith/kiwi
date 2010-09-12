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

#include <kiwi/private/log.h>
#include <kiwi/Application.h>
#include <kiwi/Error.h>

#include <cstdlib>
#include <iostream>

#include "org.kiwi.AppServer.h"

using namespace kiwi;
using namespace org::kiwi::AppServer::Session;
using namespace std;

/** Application server connection instance. */
ServerConnection *g_app_server = 0;

/** Set up the application. */
Application::Application() {
	if(g_app_server) {
		log::fatal("Application::Application: can only have 1 Application instance per process.\n");
	}

	/* Find the session port ID. */
	const char *var = getenv("APPSERVER_PORT");
	if(!var) {
		log::fatal("Could not find application server port ID\n");
	}

	/* Set up a connection to the application server. */
	g_app_server = new ServerConnection(strtol(var, NULL, 10));
}

/** Destroy the application. */
Application::~Application() {
	if(g_app_server) {
		delete g_app_server;
		g_app_server = 0;
	}
}
