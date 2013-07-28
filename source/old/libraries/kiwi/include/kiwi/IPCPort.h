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
 * @brief		IPC port class.
 */

#ifndef __KIWI_IPCPORT_H
#define __KIWI_IPCPORT_H

#include <kernel/ipc.h>

#include <kiwi/Handle.h>
#include <kiwi/IPCConnection.h>

namespace kiwi {

/** Class implementing an IPC port.
 * @todo		Separate Listen, Accept and Reject functions. */
class KIWI_PUBLIC IPCPort : public ErrorHandle {
public:
	IPCPort(handle_t handle = -1);

	bool Create();
	bool Open(port_id_t id);
	
	bool Listen(IPCConnection *&conn, useconds_t timeout = -1);
	handle_t Listen(port_client_t *infop = 0, useconds_t timeout = -1);
	port_id_t GetID() const;

	/** Signal emitted when a connection is received.
	 * @note		Does not actually accept the connection: you
	 *			must call Listen() in your handler function.
	 *			If the connection is not listened for, this
	 *			signal will be repeatedly emitted until it is,
	 *			or until the connection attempt is cancelled. */
	Signal<> OnConnection;
private:
	void RegisterEvents();
	void HandleEvent(int event);
};

}

#endif /* __KIWI_IPCPORT_H */
