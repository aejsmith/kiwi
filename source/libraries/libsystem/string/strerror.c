/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <string.h>
#include "../libsystem.h"

/** Get string representation of an error number.
 * @param err		Error number.
 * @return		Pointer to string (should NOT be modified). */
char *strerror(int err) {
	if((size_t)err >= __libsystem_error_size || __libsystem_error_list[err] == NULL) {
		return (char *)"Unknown error";
	}
	return (char *)__libsystem_error_list[err];
}
