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
 * @brief		Date command.
 */

#include <ctime>
#include <iostream>

#include "../failshell.h"

using namespace std;

/** Date command. */
class DateCommand : Shell::Command {
public:
	DateCommand() : Command("date", "Get the current date/time.") {}

	/** Get the current date/time.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		if(SHELL_HELP(argc, argv) || argc != 1) {
			cout << "Usage: " << argv[0] << endl;
			return 1;
		}

		time_t date = time(NULL);
		cout << asctime(localtime(&date));
		return 0;
	}
};

/** Instance of the date command. */
static DateCommand date_command;
