/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Rewind directory function.
 */

#include <stdlib.h>
#include <string.h>

#include "dirent/dirent.h"

/** Reset a directory stream's position to the beginning.
 * @param dir           Directory stream to rewind. */
void rewinddir(DIR *dir) {
    kern_file_rewind_dir(dir->handle);
}
