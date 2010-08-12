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
 * @brief		Directory list command.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <cerrno>
#include <cstdlib>
#include <iostream>

#include "../failshell.h"

using namespace std;

/** Directory list command. */
class LSCommand : Shell::Command {
public:
	LSCommand() : Command("ls", "Show the contents of a directory.") {}

	/** Change the current directory.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		char path[FS_PATH_MAX];
		fs_dir_entry_t *entry;
		handle_t handle;
		const char *dir;
		fs_info_t info;
		status_t ret;

		if(SHELL_HELP(argc, argv) || (argc != 1 && argc != 2)) {
			cout << "Usage: " << argv[0] << " [<directory>]" << endl;
			return 1;
		}

		dir = (argc == 2) ? argv[1] : ".";
		ret = fs_dir_open(dir, 0, &handle);
		if(ret != STATUS_SUCCESS) {
			cout << "Failed to open directory (" << ret << ")" << endl;
			return 1;
		}

		entry = reinterpret_cast<fs_dir_entry_t *>(malloc(4096));
		if(!entry) {
			cout << "Failed to allocate directory entry" << endl;
			return 1;
		}

		printf("ID    Links  Size       Name\n");
		printf("==    =====  ====       ====\n");

		while(true) {
			ret = fs_dir_read(handle, entry, 4096);
			if(ret != STATUS_SUCCESS) {
				handle_close(handle);
				free(entry);
				if(ret != STATUS_NOT_FOUND) {
					cout << "Failed to read directory (" << ret << ")" << endl;
					return ret;
				}
				return 0;
			}

			strcpy(path, dir);
			strcat(path, "/");
			strcat(path, entry->name);

			/* Get information. */
			ret = fs_info(path, false, &info);
			if(ret != STATUS_SUCCESS) {
				cout << "Failed to get entry information (" << ret << ")" << endl;
				handle_close(handle);
				free(entry);
				return ret;
			}

			printf("%-5llu %-6zu %-10llu ", info.id, info.links, info.size);
			ret = fs_symlink_read(path, path, FS_PATH_MAX);
			if(ret == STATUS_SUCCESS) {
				printf("%s -> %s\n", entry->name, path);
			} else {
				printf("%s\n", entry->name);
			}
		}
	}
};

/** Instance of the LS command. */
static LSCommand ls_command;
