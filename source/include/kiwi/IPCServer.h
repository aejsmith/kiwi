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
 * @brief		IPC server class.
 */

#ifndef __KIWI_IPCSERVER_H
#define __KIWI_IPCSERVER_H

#include <kiwi/EventLoop.h>
#include <kiwi/IPCPort.h>

namespace kiwi {

/** IPC server class. */
class IPCServer : public EventLoop {
public:
	IPCServer(handle_t handle = 3);

	/** Get the port ID.
	 * @return		Port ID. */
	port_id_t getPortID() const { return m_port.getID(); }
protected:
	virtual void handleConnection(handle_t handle) = 0;
private:
	void _handleConnection();

	IPCPort m_port;			/**< Port the server is using. */
};

}

#endif /* __KIWI_IPCSERVER_H */
