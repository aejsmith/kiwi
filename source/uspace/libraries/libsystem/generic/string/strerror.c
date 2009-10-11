/* Error string function
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
 * @brief		Error string function.
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *__libsystem_error_strs[] = {
	"Operation completed successfully",

	"Result too large",
	"Illegal byte sequence",
	"Mathematics argument out of domain of function",
};

/** Get error string.
 *
 * Gets the string representation of an error number given by a library
 * function.
 *
 * @param err		Error number.
 *
 * @return		Pointer to string.
 */
char *strerror(int err) {
	if((size_t)err > ((sizeof(__libsystem_error_strs) / sizeof(__libsystem_error_strs[0])) - 1)) {
		return (char *)"Unknown error";
	}
	return (char *)__libsystem_error_strs[err];
}
