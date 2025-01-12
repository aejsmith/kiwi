/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Directory handling functions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** Directory entry structure given by readdir(). */
struct dirent {
    ino_t d_ino;                    /**< Inode number. */
    unsigned short d_reclen;        /**< Size of this dirent. */
    char d_name[];                  /**< Name of dirent (null-terminated). */
};

struct __dstream_internal;
typedef struct __dstream_internal DIR;

extern int alphasort(const void *a, const void *b);
extern int closedir(DIR *dir);
extern DIR *opendir(const char *path);
extern struct dirent *readdir(DIR *dir);
extern void rewinddir(DIR *dir);
extern int scandir(
    const char *path, struct dirent ***namelist,
    int (*filter)(const struct dirent *),
    int (*compar)(const void *, const void *));

/* void seekdir(DIR *, long); */
/* long telldir(DIR *); */

__SYS_EXTERN_C_END
