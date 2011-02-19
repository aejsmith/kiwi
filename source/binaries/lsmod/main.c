/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
