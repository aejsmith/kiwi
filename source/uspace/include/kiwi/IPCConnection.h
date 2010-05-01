/*
 * Copyright (C) 2009 Alex Smith
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

	bool Connect(port_id_t id);
	bool Connect(const char *name);

	//bool Send(Message &message);
	bool Send(uint32_t type, const void *buf, size_t size);

	//bool Receive(Message *&message, useconds_t timeout = -1);
	bool Receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout = -1);

	Signal<IPCConnection *> OnMessage;
	Signal<IPCConnection *> OnHangup;
protected:
	void _EventReceived(int id);
};

}

#endif /* __KIWI_IPCCONNECTION_H */
