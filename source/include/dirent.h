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
 * @brief		Directory handling functions.
 */

#ifndef __DIRENT_H
#define __DIRENT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Directory entry structure given by readdir(). */
struct dirent {
	ino_t d_ino;			/**< Inode number. */
	unsigned short d_reclen;	/**< Size of this dirent. */
	char d_name[];			/**< Name of dirent (null-terminated). */
};

struct __dstream_internal;
typedef struct __dstream_internal DIR;

extern int alphasort(const void *a, const void *b);
extern int closedir(DIR *dir);
extern DIR *opendir(const char *path);
extern struct dirent *readdir(DIR *dir);
extern void rewinddir(DIR *dir);
extern int scandir(const char *path, struct dirent ***namelist, int (*filter)(const struct dirent *),
                   int(*compar)(const void *, const void *));

/* void seekdir(DIR *, long); */
/* long telldir(DIR *); */

#ifdef __cplusplus
}
#endif

#endif /* __DIRENT_H */
