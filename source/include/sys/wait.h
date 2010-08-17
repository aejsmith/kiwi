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
 * @brief		POSIX process waiting functions.
 */

#ifndef __SYS_WAIT_H
#define __SYS_WAIT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Process exit status codes. */
#define __WEXITED	(1<<0)		/**< Process exited normally. */
#define __WSIGNALED	(1<<1)		/**< Process exited because of a signal. */
#define __WSTOPPED	(1<<2)		/**< Process was stopped. */

/** Status macros for waitpid(). */
#define WIFEXITED(x)	(((x) & 0xFF) == __WEXITED)
#define WIFSIGNALED(x)	(((x) & 0xFF) == __WSIGNALED)
#define WIFSTOPPED(x)	(((x) & 0xFF) == __WSTOPPED)
#define WEXITSTATUS(x)	(((x) & 0xFF00) >> 8)
#define WTERMSIG(x)	(((x) & 0xFF00) >> 8)
#define WSTOPSIG(x)	(((x) & 0xFF00) >> 8)

/** Options for waitpid(). */
#define WNOHANG		(1<<0)		/**< Do not wait for a child. */

extern pid_t wait(int *statusp);
/* int waitid(idtype_t, id_t, siginfo_t *, int); */
extern pid_t waitpid(pid_t pid, int *statusp, int flags);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_WAIT_H */
