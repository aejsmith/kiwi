/*
 * Copyright (C) 2009-2022 Alex Smith
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

extern pid_t wait(int *_status);
/* int waitid(idtype_t, id_t, siginfo_t *, int); */
extern pid_t waitpid(pid_t pid, int *_status, int flags);

__SYS_EXTERN_C_END
