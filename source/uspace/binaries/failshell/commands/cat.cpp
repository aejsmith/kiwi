/* Kiwi shell - File concatenation command
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
 * @brief		File concatenation command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/handle.h>

#include <stdio.h>

#include "../failshell.h"

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
		size_t bytes;
		int i, ret;
		char ch;

		if(SHELL_HELP(argc, argv) || argc < 2) {
			printf("Usage: %s <file1> [<file2> ...]\n", argv[0]);
			if(argc < 2) {
				printf("             _______ \n");
				printf("            (_Meow!_)\n");
				printf("              | /    \n");
				printf("          /|_ |/     \n");
				printf("        ,'  .\\     \n");
				printf("    ,--'    _,'     \n");
				printf("   /       /        \n");
				printf("  (   -.  |         \n");
				printf("  |     ) |         \n");
				printf(" (`-.  `--.)        \n");
				printf("  `._)----'         \n\n");
			}
			return -ERR_PARAM_INVAL;
		}

		for(i = 1; i < argc; i++) {
			if((handle = fs_file_open(argv[i], FS_FILE_READ)) < 0) {
				printf("Failed to open %s (%d)\n", argv[i], handle);
				return handle;
			}

			while(true) {
				if((ret = fs_file_read(handle, &ch, 1, -1, &bytes)) != 0) {
					printf("Failed to read %s (%d)\n", argv[i], ret);
					handle_close(handle);
					return ret;
				} else if(bytes == 0) {
					break;
				}

				putchar(ch);
			}

			handle_close(handle);
		}

		return 0;
	}
};

/** Instance of the cat command. */
static CatCommand cat_command;
