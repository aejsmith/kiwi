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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "org.kiwi.AppServer.h"

class AppServer;

/** Class representing a connection to the main server. */
class Connection : public org::kiwi::AppServer::ClientConnection {
public:
	Connection(AppServer *server, handle_t handle);
	~Connection();
private:
	AppServer *m_server;		/**< Server that the connection is for. */
};

#endif /* __CONNECTION_H */
