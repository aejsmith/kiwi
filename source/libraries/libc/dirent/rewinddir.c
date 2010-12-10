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
 * @brief		Rewind directory function.
 */

#include <stdlib.h>
#include <string.h>

#include "dirent_priv.h"

/** Reset directory stream position to beginning of directory.
 * @param dir		Directory stream to rewind. */
void rewinddir(DIR *dir) {
	kern_file_seek(dir->handle, FILE_SEEK_SET, 0, NULL);
}
