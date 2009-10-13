/* Kiwi shell - Unlink command
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
 * @brief		Unlink command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>

#include <iostream>

#include "../failshell.h"

using namespace std;

/** Unlink command. */
class UnlinkCommand : Shell::Command {
public:
	UnlinkCommand() : Command("unlink", "Unlink a file/directory.") {}

	/** Unlink a file.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		if(SHELL_HELP(argc, argv) || argc != 2) {
			cout << "Usage: " << argv[0] << " <path>" << endl;
			return -ERR_PARAM_INVAL;
		}

		return fs_unlink(argv[1]);
	}
};

/** Instance of the unlink command. */
static UnlinkCommand unlink_command;
