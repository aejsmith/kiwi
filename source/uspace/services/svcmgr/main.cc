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

#include <kiwi/Process.h>

#include <string.h>

#include <stdio.h>
#include <stdlib.h>

using namespace kiwi;

int main(int argc, char **argv) {
	Process *proc;
	int ret;

	if(Process::GetCurrentID() != 1) {
		printf("svcmgr: not process 1, exiting...\n");
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
