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
 * @brief		Date command.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** Print a usage message. */
static void usage(char *argv0) {
	printf("Usage: %s [-u] [+format]\n", argv0);
}

/** Main function for the date command. */
int main(int argc, char **argv) {
	bool use_utc = false;
	const char *format;
	char buf[4096];
	time_t current;
	struct tm *tm;
	int i, c;

	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return EXIT_SUCCESS;
		}
	}

	/* Parse options. */
	while((c = getopt(argc, argv, "u")) != -1) {
		switch(c) {
		case 'u':
			use_utc = true;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	current = time(NULL);
	tm = (use_utc) ? gmtime(&current) : localtime(&current);

	if(optind < argc) {
		if(argv[argc - 1][0] != '+') {
			return EXIT_FAILURE;
		}
		format = argv[argc - 1] + 1;
	} else {
		format = "%a %b %e %H:%M:%S %Z %Y";
	}

	if(strftime(buf, sizeof(buf), format, tm) == 0) {
		return EXIT_FAILURE;
	}

	puts(buf);
	return EXIT_SUCCESS;
}
