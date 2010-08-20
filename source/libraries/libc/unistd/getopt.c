/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Option parsing function.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *optarg;
int optind = 1, opterr = 1, optopt;

/** Parse command line options.
 *
 * Parses command line options according to the provided option string.
 * The option string should be a string of valid option characters. If an
 * option requires an argument, the character should be followed by a :
 * character in the string.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @param opts		Argument string.
 *
 * @return		Option character found, '?' if unknown character,
 *			':' if missing an argument and the first character
 *			of opts was a colon ('?' if missing and first
 *			character was not a colon), and -1 when option
 *			parsing is finished.
 */
int getopt(int argc, char *const argv[], const char *opts) {
	static int offset = 1;
	char *ptr, *tmp;
	int ret;

	if(optind >= argc || argv[optind] == NULL || *argv[optind] != '-' || strcmp(argv[optind], "-") == 0) {
		return -1;
	} else if(strcmp(argv[optind], "--") == 0) {
		optind++;
		return -1;
	}

	tmp = argv[optind] + offset++;
	ret = *tmp++;
	ptr = strchr(opts, ret);
	if(ptr == NULL) {
		optopt = ret;
		if(opterr != 0) {
			fprintf(stderr, "%s: illegal option -- %c\n", argv[0], ret);
		}
		ret = '?';
		goto out;
	}

	if(ptr[1] == ':') {
		if(*tmp) {
			optarg = tmp;
			optind++;
			offset = 1;
			return ret;
		} else {
			if(optind + 1 >= argc) {
				fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], ret);
				ret = (*opts == ':') ? ':' : '?';
			} else {
				optarg = argv[++optind];
				optind++;
				offset = 1;
				return ret;
			}
		}
	}
out:
	if(!*(argv[optind] + offset)) {
		offset = 1;
		optind++;
	}

	return ret;
}
