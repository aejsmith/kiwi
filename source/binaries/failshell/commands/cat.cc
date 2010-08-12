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
 * @brief		File concatenation command.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <iostream>
#include <stdio.h>

#include "../failshell.h"

using namespace std;

/** File concatenation command. */
class CatCommand : Shell::Command {
public:
	CatCommand() : Command("cat", "Concatenate files together.") {}

	/** Concatenate files together.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		handle_t handle;
		fs_info_t info;
		size_t bytes;
		char *block;
		int i, ret;

		if(SHELL_HELP(argc, argv) || argc < 2) {
			cout << "Usage: " << argv[0] << " <file1> [<file2> ...]" << endl;
			if(argc < 2) {
				cout << "             _______ " << endl;
				cout << "            (_Meow!_)" << endl;
				cout << "              | /    " << endl;
				cout << "          /|_ |/     " << endl;
				cout << "        ,'  .\\     " << endl;
				cout << "    ,--'    _,'     " << endl;
				cout << "   /       /        " << endl;
				cout << "  (   -.  |         " << endl;
				cout << "  |     ) |         " << endl;
				cout << " (`-.  `--.)        " << endl;
				cout << "  `._)----'         " << endl << endl;
			}
			return STATUS_INVALID_ARG;
		}

		for(i = 1; i < argc; i++) {
			ret = fs_file_open(argv[i], FS_FILE_READ, &handle);
			if(ret != STATUS_SUCCESS) {
				cout << "Failed to open " << argv[i] << " (" << ret << ")" << endl;
				return handle;
			}
			ret = fs_handle_info(handle, &info);
			if(ret != STATUS_SUCCESS) {
				cout << "Failed to get information on " << argv[i] << " (" << ret << ")" << endl;
				handle_close(handle);
				return ret;
			}

			block = new char[info.blksize];
			while(true) {
				ret = fs_file_read(handle, block, info.blksize, &bytes);
				if(ret != STATUS_SUCCESS) {
					cout << "Failed to read " << argv[i] << " (" << ret << ")" << endl;
					handle_close(handle);
					delete[] block;
					return ret;
				} else if(bytes == 0) {
					break;
				}

				fwrite(block, bytes, 1, stdout);
			}

			handle_close(handle);
			delete[] block;
		}

		return STATUS_SUCCESS;
	}
};

/** Instance of the cat command. */
static CatCommand cat_command;
