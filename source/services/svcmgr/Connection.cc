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
 * @brief		Service manager connection class.
 */

#include <iostream>

#include "ServiceManager.h"
#include "Connection.h"

using namespace std;
using namespace kiwi;

/** Construct a connection object.
 * @param handle	Handle to the connection.
 * @param svcmgr	ServiceManager instance this connection belongs to. */
Connection::Connection(handle_t handle, ServiceManager *svcmgr) :
	org::kiwi::ServiceManager::ClientConnection(handle),
	m_svcmgr(svcmgr)
{
}

/** Look up a port.
 * @param name		Name of port to look up.
 * @param id		Where to store ID of port.
 * @return		0 on success, error code on failure. */
status_t Connection::LookupPort(const string &name, port_id_t &id) {
	return m_svcmgr->LookupPort(name, id) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}
