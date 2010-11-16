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
