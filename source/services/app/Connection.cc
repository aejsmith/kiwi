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
 * @brief		Application server connection class.
 */

#include <kernel/status.h>

#include <iostream>

#include "AppServer.h"
#include "Connection.h"

using namespace std;
using namespace kiwi;

/** Construct a connection object.
 * @param server	Server that the connection is for.
 * @param handle	Handle to the connection. */
Connection::Connection(AppServer *server, handle_t handle) :
	org::kiwi::AppServer::ClientConnection(handle),
	m_server(server)
{
}

/** Destroy a connection. */
Connection::~Connection() {
	/* Remove us from the server. */
	m_server->RemoveConnection(this);
}
