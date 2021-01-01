/*
 * Copyright (C) 2009-2021 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               POSIX standard symbolic constants and types.
 */

#pragma once

#include <system/arch/unistd.h>

#include <stdio.h>

__SYS_EXTERN_C_BEGIN

/** POSIX version/feature definitions.
 * @note                Not actually compliant to these versions yet. :P */
#define _POSIX_VERSION      200809l
#define _POSIX2_VERSION     200809l
#define _XOPEN_VERSION      700
#define _XOPEN_UNIX         1

/** Standard stream file descriptor numbers. */
#define STDIN_FILENO        0
#define STDOUT_FILENO       1
#define STDERR_FILENO       2

/** Mode flags for access(). */
#define F_OK                0       /**< Check for file existance. */
#define R_OK                0x0001  /**< Check for read permission. */
#define W_OK                0x0002  /**< Check for write permission. */
#define X_OK                0x0004  /**< Check for execute permission. */

extern char *optarg;
extern int optind, opterr, optopt;

extern int access(const char *path, int mode);
extern unsigned int alarm(unsigned int seconds);
extern int chdir(const char *path);
extern int chown(const char *path, uid_t uid, gid_t gid);
extern int close(int fd);
/* size_t confstr(int, char *, size_t); */
/* char *crypt(const char *, const char *); */
extern int dup(int fd);
extern int dup2(int fd, int newfd);
extern void _exit(int status);
/* void encrypt(char[64], int); */
extern int execl(const char *path, const char *arg, ...);
/* int execle(const char *, const char *, ...); */
extern int execlp(const char *file, const char *arg, ...);
extern int execv(const char *path, char *const argv[]);
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern int execvp(const char *file, char *const argv[]);
/* int faccessat(int, const char *, int, int); */
//extern int fchdir(int fd);
extern int fchown(int fd, uid_t uid, gid_t gid);
/* int fchownat(int, const char *, uid_t, gid_t, int); */
/* int fdatasync(int); */
/* int fexecve(int, char *const [], char *const []); */
extern pid_t fork(void);
/* long fpathconf(int, int); */
extern int fsync(int fd);
/* int ftruncate(int, off_t); */
extern char *getcwd(char *buf, size_t size);
extern gid_t getegid(void);
extern uid_t geteuid(void);
extern gid_t getgid(void);
/* int getgroups(int, gid_t []); */
/* long gethostid(void); */
/* int gethostname(char *, size_t); */
extern char *getlogin(void);
/* int getlogin_r(char *, size_t); */
extern int getopt(int argc, char *const argv[], const char *opts);
//extern int getpagesize(void);
//extern pid_t getpgid(pid_t);
//extern pid_t getpgrp(void);
extern pid_t getpid(void);
extern pid_t getppid(void);
//extern pid_t getsid(pid_t pid);
extern uid_t getuid(void);
extern char *getwd(char *buf);
extern int isatty(int fd);
extern int lchown(const char *path, uid_t uid, gid_t gid);
extern int link(const char *source, const char *dest);
/* int linkat(int, const char *, int, const char *, int); */
/* int lockf(int, int, off_t); */
extern off_t lseek(int fd, off_t off, int act);
/* int nice(int); */
/* long pathconf(const char *, int); */
/* int pause(void); */
extern int pipe(int fds[2]);
extern ssize_t pread(int fd, void *buf, size_t count, off_t offset);
extern ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
extern ssize_t read(int fd, void *buf, size_t count);
extern ssize_t readlink(const char *path, char *buf, size_t size);
/* ssize_t readlinkat(int, const char *restrict, char *restrict, size_t); */
extern int rmdir(const char *path);
/* int setegid(gid_t); */
/* int seteuid(uid_t); */
extern int setgid(gid_t gid);
/* int setpgid(pid_t, pid_t); */
//extern pid_t setpgrp(void);
/* int setregid(gid_t, gid_t); */
/* int setreuid(uid_t, uid_t); */
//extern pid_t setsid(void);
extern int setuid(uid_t uid);
extern unsigned int sleep(unsigned int secs);
/* void swab(const void *restrict, void *restrict, ssize_t); */
extern int symlink(const char *dest, const char *path);
/* int symlinkat(const char *, int, const char *); */
extern void sync(void);
/* long sysconf(int); */
extern pid_t tcgetpgrp(int fd);
extern int tcsetpgrp(int fd, pid_t pgid);
/* int truncate(const char *, off_t); */
extern char *ttyname(int fd);
/* int ttyname_r(int, char *, size_t); */
extern int unlink(const char *path);
//extern int unlinkat(int, const char *, int);
extern ssize_t write(int fd, const void *buf, size_t count);

__SYS_EXTERN_C_END
