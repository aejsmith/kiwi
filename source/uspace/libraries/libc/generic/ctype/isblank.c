/* Kiwi C library - Character type functions
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Character type functions.
 */

#include <ctype.h>

/** Test if character is blank.
 *
 * Tests that the given character is a blank space.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is blank space, zero if not.
 */
int isblank(int ch) {
	return (ch == ' ' || ch == '\t');
}
