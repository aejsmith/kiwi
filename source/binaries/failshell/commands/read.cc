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
 * @brief		File read command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/object.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "../failshell.h"

using namespace std;

/** File read command. */
class ReadCommand : Shell::Command {
public:
	ReadCommand() : Command("read", "Read bytes from a file (does not output them).") {}

	/** Read bytes from a file.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		size_t bytes, blksize, blocks, i;
		handle_t handle;
		offset_t offset;
		char *block;
		int ret;

		if(SHELL_HELP(argc, argv) || argc != 5) {
			cout << "Usage: " << argv[0] << " <file> <offset> <blocks> <blksize>" << endl;
			return -ERR_PARAM_INVAL;
		}

		offset = strtoul(argv[2], NULL, 0);
		blocks = strtoul(argv[3], NULL, 0);
		blksize = strtoul(argv[4], NULL, 0);

		if((handle = fs_file_open(argv[1], FS_FILE_READ)) < 0) {
			cout << "Failed to open " << argv[1] << " (" << handle << ")" << endl;
			return handle;
		} else if((ret = fs_handle_seek(handle, FS_SEEK_SET, offset, NULL)) != 0) {
			cout << "Failed to seek (" << ret << ")" << endl;
			handle_close(handle);
			return ret;
		}

		block = new char[blksize];
		for(i = 0; i < blocks; i++) {
			if((ret = fs_file_read(handle, block, blksize, &bytes)) != 0) {
				cout << "Failed to read (" << ret << ")" << endl;
				break;
			} else if(bytes == 0) {
				cout << "Reached end of file" << endl;
				break;
			}
		}

		delete [] block;
		handle_close(handle);
		return ret;
	}
};

/** Instance of the read command. */
static ReadCommand read_command;
