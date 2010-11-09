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
 * @brief		Unmount command.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** Main function for the unmount command. */
int main(int argc, char **argv) {
	status_t ret;

	if(argc != 2 || (argc >= 2 && strcmp(argv[1], "--help") == 0)) {
		printf("Usage: %s <path>\n", argv[0]);
		return EXIT_FAILURE;
	}

	ret = fs_unmount(argv[1]);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
