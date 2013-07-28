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

	ret = kern_fs_unmount(argv[1]);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
