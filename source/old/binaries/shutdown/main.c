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
 * @brief		Shutdown command.
 */

#include <kernel/system.h>
#include <kernel/status.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** Main function for the shutdown command. */
int main(int argc, char **argv) {
	int action = SHUTDOWN_POWEROFF;
	status_t ret;

	if(argc > 1) {
		if(strcmp(argv[1], "--help") == 0) {
			printf("Usage: %s [-r]\n", argv[0]);
			return EXIT_SUCCESS;
		} else if(strcmp(argv[1], "-r") == 0) {
			action = SHUTDOWN_REBOOT;
		}
	}

	if(strcmp(argv[0], "reboot") == 0) {
		action = SHUTDOWN_REBOOT;
	}

	ret = kern_shutdown(action);
	printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
	return EXIT_FAILURE;
}
