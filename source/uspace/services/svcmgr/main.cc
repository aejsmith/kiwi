/* Kiwi service manager
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
 * @brief		Service manager.
 */

#include <kernel/ipc.h>

#include <kiwi/Process.h>

#include <iostream>

using namespace kiwi;
using namespace std;

int main(int argc, char **argv) {
	handle_t handle;
	Process *proc;
	int ret;

	/* Check if we are process 1. */
	if(Process::GetCurrentID() != 1) {
		cout << "Must be run as process 1" << endl;
		return 1;
	}

	/* Create the service manager port. */
	if((handle = ipc_port_create()) < 0) {
		cout << "Could not register port (" << handle << ")" << endl;
		return 1;
	} else if(ipc_port_id(handle) != 1) {
		cout << "Created port is not port 1" << endl;
		return 1;
	}

	proc = new Process("/system/binaries/console");
	if(!proc->Initialised(&ret)) {
		printf("Failed to create process (%d)\n", ret);
		delete proc;
		return 1;
	}

	proc->WaitTerminate();
	delete proc;
	while(1);
}
