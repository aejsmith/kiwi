/*
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
 * @brief		Mount command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>

#include <iostream>

#include "../failshell.h"

using namespace std;

/** Mount command. */
class MountCommand : Shell::Command {
public:
	MountCommand() : Command("mount", "Mount a filesystem.") {}

	/** Mount a filesystem.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		char *dev, *path, *type;
		int flags = 0;

		if(SHELL_HELP(argc, argv) || (argc != 4 && argc != 5)) {
			cout << "Usage: " << argv[0] << " [--rdonly] <dev> <path> <type>" << endl;
			return -ERR_PARAM_INVAL;
		}

		if(argc == 5) {
			if(strcmp(argv[1], "--rdonly") == 0) {
				flags |= FS_MOUNT_RDONLY;
			} else {
				cout << "Unknown option '" << argv[1] << "'" << endl;
				return -ERR_PARAM_INVAL;
			}

			dev = argv[2];
			path = argv[3];
			type = argv[4];
		} else {
			dev = argv[1];
			path = argv[2];
			type = argv[3];
		}

		return fs_mount(dev, path, type, flags);
	}
};

/** Instance of the mount command. */
static MountCommand mount_command;
