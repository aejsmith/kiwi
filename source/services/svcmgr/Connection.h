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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "org.kiwi.ServiceManager.h"
#include "ServiceManager.h"

/** Class representing a client of the service manager. */
class Connection : public org::kiwi::ServiceManager::ClientConnection {
public:
	Connection(handle_t handle, ServiceManager *svcmgr);
private:
	status_t LookupPort(const std::string &name, port_id_t &id);

	ServiceManager *m_svcmgr;	/**< ServiceManager instance this connection belongs to. */
};

#endif /* __CONNECTION_H */
