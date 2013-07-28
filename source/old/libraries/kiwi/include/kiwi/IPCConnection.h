/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	void HandleEvent(int event);
};

}

#endif /* __KIWI_IPCCONNECTION_H */
