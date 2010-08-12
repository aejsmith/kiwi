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
 * @brief		String unformatting function.
 */

#include "stdio_priv.h"

/** Unformat a buffer.
 *
 * Unformats a buffer into a list of arguments according to the given format
 * string.
 *
 * @param data		Structure containing helper functions.
 * @param fmt		Format string.
 * @param args		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int do_scanf(struct scanf_args *data, const char *restrict fmt, va_list args) {
	libc_stub(__FUNCTION__);
}
