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
class KIWI_PUBLIC IPCPort : public Handle {
public:
	IPCPort(handle_t handle = -1);

	bool Create();
	bool Open(port_id_t id);
	
	bool Listen(IPCConnection *&conn, useconds_t timeout = -1);
	handle_t Listen(ipc_client_info_t *infop = 0, useconds_t timeout = -1);
	port_id_t GetID() const;

	/** Get information about the last error that occurred.
	 * @return		Reference to error object for last error. */
	const Error &GetError() const { return m_error; }

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

	Error m_error;			/**< Error information. */
};

}

#endif /* __KIWI_IPCPORT_H */
