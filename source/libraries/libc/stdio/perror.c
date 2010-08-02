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
 * @brief		Print error function.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

/** Print an error message.
 *
 * Prints the given error message followed by the string returned from
 * strerror() for the current errno value and a newline character to stderr. If
 * the message given is NULL, then only the string given by strerror() is
 * printed.
 *
 * @param s             Error message to print.
 */
void perror(const char *s) {
	if(s != NULL && s[0]) {
		fprintf(stderr, "%s: %s\n", s, strerror(errno));
	} else {
		fprintf(stderr, "%s\n", strerror(errno));
	}
}
