/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		POSIX file information functions.
 */

#ifndef __SYS_STAT_H
#define __SYS_STAT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** File type mode defintions. */
#define S_IFMT 		0170000		/**< Bitmask for the file type bitfields. */
#define S_IFSOCK	0140000		/**< Socket. */
#define S_IFLNK		0120000		/**< Symbolic link. */
#define S_IFREG		0100000		/**< Regular file. */
#define S_IFBLK		0060000		/**< Block device. */
#define S_IFDIR		0040000		/**< Directory. */
#define S_IFCHR		0020000		/**< Character device. */
#define S_IFIFO		0010000		/**< FIFO. */

/** Permissions. */
#define S_ISUID		0004000		/**< Set UID bit. */
#define S_ISGID		0002000		/**< Set-group-ID bit. */
#define S_ISVTX		0001000		/**< Sticky bit. */
#define S_IRWXU		00700		/**< Mask for file owner permissions. */
#define S_IRUSR		00400		/**< Owner has read permission. */
#define S_IWUSR		00200		/**< Owner has write permission. */
#define S_IXUSR		00100		/**< Owner has execute permission. */
#define S_IRWXG		00070		/**< Mask for group permissions. */
#define S_IRGRP		00040		/**< Group has read permission. */
#define S_IWGRP		00020		/**< Group has write permission. */
#define S_IXGRP		00010		/**< Group has execute permission. */
#define S_IRWXO		00007		/**< Mask for permissions for others (not in group). */
#define S_IROTH		00004		/**< Others have read permission. */
#define S_IWOTH		00002		/**< Others have write permission. */
#define S_IXOTH		00001		/**< Others have execute permission. */

/** Macros to check if a file is a certain type. */
#define S_ISREG(m)	((m & S_IFMT) == S_IFREG)
#define S_ISBLK(m)	((m & S_IFMT) == S_IFBLK)
#define S_ISLNK(m)	((m & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)	((m & S_IFMT) == S_IFSOCK)
#define S_ISDIR(m)	((m & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	((m & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m)	((m & S_IFMT) == S_IFIFO)

/** POSIX stat structure. */
struct stat {
	dev_t st_dev;			/**< ID of device containing file. */
	ino_t st_ino;			/**< Inode number. */
	mode_t st_mode;			/**< File mode. */
	nlink_t st_nlink;		/**< Number of hard links. */
	uid_t st_uid;			/**< User ID of owner. */
	gid_t st_gid;			/**< Group ID of owner. */
	dev_t st_rdev;			/**< Device ID (if special file). */
	off_t st_size;			/**< Total size, in bytes. */
	time_t st_atime;		/**< Time of last access. */
	time_t st_mtime;		/**< Time of last modification. */
	time_t st_ctime;		/**< Time of last status change. */
	blksize_t st_blksize;		/**< Blocksize for filesystem I/O. */
	blkcnt_t st_blocks;		/**< Number of blocks allocated. */
};

extern int chmod(const char *path, mode_t mode);
extern int fchmod(int fd, mode_t mode);
/* int fchmodat(int, const char *, mode_t, int); */
extern int fstat(int fd, struct stat *statp);
/* int fstatat(int, const char *__restrict, struct stat *__restrict, int); */
/* int futimens(int, const struct timespec [2]); */
extern int lstat(const char *__restrict path, struct stat *__restrict statp);
extern int mkdir(const char *path, mode_t mode);
/* int mkdirat(int, const char *, mode_t); */
/* int mkfifo(const char *, mode_t); */
/* int mkfifoat(int, const char *, mode_t); */
extern int mknod(const char *path, mode_t mode, dev_t dev);
extern int stat(const char *__restrict path, struct stat *__restrict statp);
extern mode_t umask(mode_t mask);
/* int utimensat(int, const char *, const struct timespec [2], int); */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_STAT_H */
