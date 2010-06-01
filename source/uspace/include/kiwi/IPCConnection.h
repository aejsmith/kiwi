/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IPC connection class.
 */

#ifndef __KIWI_IPCCONNECTION_H
#define __KIWI_IPCCONNECTION_H

#include <kiwi/Handle.h>

namespace kiwi {

/** Class implementing an IPC connection. */
class IPCConnection : public Handle {
public:
	IPCConnection(handle_t handle = -1);

	bool connect(port_id_t id);
	bool connect(const char *name);

	bool send(uint32_t type, const void *buf, size_t size);
	bool receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout = -1);

	bool waitHangup(useconds_t timeout = -1) const;

	Signal<IPCConnection *> onMessage;
	Signal<IPCConnection *> onHangup;
protected:
	void registerEvents();
	void eventReceived(int id);
};

}

#endif /* __KIWI_IPCCONNECTION_H */
