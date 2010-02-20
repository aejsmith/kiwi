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
 * @brief		Sleep command.
 */

#include <kernel/errors.h>
#include <kernel/thread.h>

#include <iostream>
#include <stdlib.h>

#include "../failshell.h"

using namespace std;

/** Sleep directory command. */
class SleepCommand : Shell::Command {
public:
	SleepCommand() : Command("sleep", "Sleep for a number of seconds.") {}

	/** Change the current directory.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		unsigned long seconds;

		if(SHELL_HELP(argc, argv) || argc != 2) {
			cout << "Usage: " << argv[0] << " <seconds>" << endl;
			return -ERR_PARAM_INVAL;
		}

		seconds = strtoul(argv[1], NULL, 0);
		return thread_usleep(seconds * 1000000);
	}
};

/** Instance of the sleep command. */
static SleepCommand sleep_command;
