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
 * @brief		Write command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/object.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "../failshell.h"

using namespace std;

/** Write command. */
class WriteCommand : Shell::Command {
public:
	WriteCommand() : Command("write", "Write data to a file.") {}

	/** Write to a file.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		handle_t handle;
		char buf[4096];
		offset_t off;
		size_t bytes;
		int ret, i;

		if(SHELL_HELP(argc, argv) || argc < 4) {
			cout << "Usage: " << argv[0] << " <file> <offset> <word1> [<word2>...]" << endl;
			return -ERR_PARAM_INVAL;
		}

		off = strtoul(argv[2], NULL, 10);

		if((handle = fs_file_open(argv[1], FS_FILE_WRITE)) < 0) {
			if(handle == -ERR_NOT_FOUND) {
				if((ret = fs_file_create(argv[1])) == 0) {
					handle = fs_file_open(argv[1], FS_FILE_WRITE);
				} else {
					cout << "Create failed (" << ret << ")" << endl;
				}
			}
			if(handle < 0) {
				cout << "Open failed (" << handle << ")" << endl;
				return handle;
			}
		}

		for(i = 3; i < argc; i++) {
			strcpy(buf, argv[i]);
			if((i + 1) == argc) {
				strcat(buf, "\n");
			} else {
				strcat(buf, " ");
			}
			if((ret = fs_file_write(handle, buf, strlen(buf), off, &bytes)) != 0) {
				cout << "Write failed (" << ret << ")" << endl;
				return ret;
			} else if(bytes != strlen(buf)) {
				cout << "Didn't write all data (" << bytes << ")" << endl;
				return -ERR_DEVICE_ERROR;
			}

			off += bytes;
		}

		handle_close(handle);
		return 0;
	}
};

/** Instance of the write command. */
static WriteCommand write_command;
