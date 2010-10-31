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

#include <kiwi/Error.h>
#include <kiwi/Handle.h>

namespace kiwi {

/** Class implementing an IPC connection. */
class KIWI_PUBLIC IPCConnection : public ErrorHandle {
public:
	IPCConnection(handle_t handle = -1);

	bool Connect(port_id_t id);
	bool Connect(const char *name);

	bool Send(uint32_t type, const void *buf, size_t size);
	bool Receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout = -1);

	bool WaitForHangup(useconds_t timeout = -1) const;

	/** Signal emitted when a message is received on the connection.
	 * @note		The handler must call Receive() to get the
	 *			message itself. If it does not, this signal
	 *			will be repeatedly emitted until it is. */
	Signal<> OnMessage;

	/** Signal emitted when the remote end of the connection hangs up. */
	Signal<> OnHangup;
private:
	void RegisterEvents();
	void HandleEvent(int id);
};

}

#endif /* __KIWI_IPCCONNECTION_H */
