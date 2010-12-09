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
 * @brief		Shutdown command.
 */

#include <kernel/system.h>
#include <kernel/status.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** Main function for the shutdown command. */
int main(int argc, char **argv) {
	int action = SHUTDOWN_POWEROFF;
	status_t ret;

	if(argc > 1) {
		if(strcmp(argv[1], "--help") == 0) {
			printf("Usage: %s [-r]\n", argv[0]);
			return EXIT_SUCCESS;
		} else if(strcmp(argv[1], "-r") == 0) {
			action = SHUTDOWN_REBOOT;
		}
	}

	if(strcmp(argv[0], "reboot") == 0) {
		action = SHUTDOWN_REBOOT;
	}

	ret = kern_shutdown(action);
	printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
	return EXIT_FAILURE;
}
