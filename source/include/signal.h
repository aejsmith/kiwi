/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		POSIX signal functions.
 */

#ifndef __SIGNAL_H
#define __SIGNAL_H

#define __POSIX_DEFS_ONLY
#include <kernel/signal.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Value returned from signal() on error. */
#define SIG_ERR		((void (*)(int))-1)

/** Type of a signal handler. */
typedef void (*sighandler_t)(int);

extern const char *const sys_siglist[];

extern int kill(pid_t pid, int num);
/* int killpg(pid_t, int); */
extern void psignal(int sig, const char *s);
extern void psiginfo(const siginfo_t *info, const char *s);
/* int pthread_kill(pthread_t, int); */
/* int pthread_sigmask(int, const sigset_t *, sigset_t *); */
extern int raise(int num);
extern int sigaction(int num, const struct sigaction *__restrict act,
                     struct sigaction *__restrict oldact);
extern int sigaltstack(const stack_t *__restrict ss, stack_t *__restrict oldss);
extern int sigaddset(sigset_t *set, int num);
extern int sigdelset(sigset_t *set, int num);
extern int sigemptyset(sigset_t *set);
extern int sigfillset(sigset_t *set);
/* int sighold(int); */
/* int sigignore(int); */
/* int siginterrupt(int, int); */
extern int sigismember(const sigset_t *set, int num);
extern sighandler_t signal(int num, sighandler_t handler);
/* int sigpause(int); */
/* int sigpending(sigset_t *); */
extern int sigprocmask(int how, const sigset_t *__restrict set, sigset_t *__restrict oset);
extern int sigsuspend(const sigset_t *mask);
/* int sigwait(const sigset_t *, int *); */

#ifdef __cplusplus
}
#endif

#endif /* __SIGNAL_H */
