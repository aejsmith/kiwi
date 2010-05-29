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

#include <kiwi/Handle.h>
#include <kiwi/IPCConnection.h>

namespace kiwi {

/** Class implementing an IPC port. */
class IPCPort : public Handle {
public:
	IPCPort(handle_t handle = -1);

	bool create();
	bool open(port_id_t id);
	bool registerName(const char *name);

	IPCConnection *listen(useconds_t timeout = -1) const;
	port_id_t getID() const;

	Signal<IPCPort *> onConnection;
private:
	void registerEvents();
	void eventReceived(int id);
};

}

#endif /* __KIWI_IPCPORT_H */
