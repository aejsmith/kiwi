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
 * @brief		Module list command.
 */

#include <kernel/module.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>

/** Main function for the lsmod command. */
int main(int argc, char **argv) {
	module_info_t *modules;
	size_t count, i;
	status_t ret;

	ret = kern_module_info(NULL, &count);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	modules = malloc(count * sizeof(module_info_t));
	if(!modules) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	ret = kern_module_info(modules, &count);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	printf("Name             Count Size     Description\n");
	printf("====             ===== ====     ===========\n");

	for(i = 0; i < count; i++) {
		printf("%-16s %-5d %-8zu %s\n", modules[i].name, modules[i].count,
		       modules[i].load_size, modules[i].desc);
	}

	return EXIT_SUCCESS;
}
