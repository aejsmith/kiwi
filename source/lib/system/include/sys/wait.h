/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX process waiting functions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/**
 * When a process is killed due to a POSIX signal, the signal information is
 * communicated in the status code passed to kern_process_kill(). A magic value
 * is set in the upper 16 bits to identify a status code that originated from
 * a POSIX signal.
 */
#define __POSIX_KILLED_STATUS   0x5dba

/** Process exit status codes. */
#define __WEXITED       (1<<0)      /**< Process exited normally. */
#define __WSIGNALED     (1<<1)      /**< Process exited because of a signal. */
#define __WSTOPPED      (1<<2)      /**< Process was stopped. */

/** Status macros for waitpid(). */
#define WIFEXITED(x)    (((x) & 0xff) == __WEXITED)
#define WIFSIGNALED(x)  (((x) & 0xff) == __WSIGNALED)
#define WIFSTOPPED(x)   (((x) & 0xff) == __WSTOPPED)
#define WEXITSTATUS(x)  (((x) & 0xff00) >> 8)
#define WTERMSIG(x)     (((x) & 0xff00) >> 8)
#define WSTOPSIG(x)     (((x) & 0xff00) >> 8)

/** Options for waitpid(). */
#define WNOHANG         (1<<0)      /**< Do not wait for a child. */
#define WUNTRACED       (1<<1)      /**< Return if a child has stopped (but is not traced). */

extern pid_t wait(int *_status);
/* int waitid(idtype_t, id_t, siginfo_t *, int); */
extern pid_t waitpid(pid_t pid, int *_status, int flags);

__SYS_EXTERN_C_END
