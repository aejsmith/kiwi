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
 * @brief		Execute shell command function.
 */

#include <stdlib.h>
#include <unistd.h>

/** Execute a shell command.
 * @param command	Command line to execute, will be run using 'sh -c <line>'.
 * @return		Exit status of process (in format returned by wait()),
 *			or -1 if unable to fork the process. */
int system(const char *command) {
	int status;
	pid_t pid;

	pid = fork();
	if(pid == 0) {
		execl("/system/binaries/sh", "/system/binaries/sh", "-c", command, NULL);
		exit(127);
	} else if(pid > 0) {
		pid = waitpid(pid, &status, 0);
		if(pid < 0) {
			return -1;
		}

		return status;
	} else {
		return -1;
	}
}
