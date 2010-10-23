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
 * @brief		Service main class.
 */

#ifndef __KIWI_SERVICE_SERVICE_H
#define __KIWI_SERVICE_SERVICE_H

#include <kiwi/EventLoop.h>
#include <kiwi/IPCPort.h>

#include <string>

namespace kiwi {

/** Main class for a service. */
class Service : public EventLoop {
public:
	Service();
protected:
	virtual void HandleConnection(handle_t handle, ipc_client_info_t &info);
private:
	void _AddPort(const std::string &name, port_id_t id);
	void _HandleConnection();

	void *m_svcmgr;			/**< Connection to service manager. */
	IPCPort *m_port;		/**< Port for single port services. */
};

}

#endif /* __KIWI_SERVICE_SERVICE_H */
