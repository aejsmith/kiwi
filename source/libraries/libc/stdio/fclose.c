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
 * @brief		File close function.
 */

#include <stdlib.h>
#include <unistd.h>

#include "stdio_priv.h"

/** Close a file stream.
 * @param stream	File stream to close.
 * @return		0 on success, EOF on failure. */
int fclose(FILE *stream) {
	if(close(stream->fd) != 0) {
		return EOF;
	}
	free(stream);
	return 0;
}
