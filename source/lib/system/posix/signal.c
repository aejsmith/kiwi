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
 * @brief               POSIX signal functions.
 */

#include <kernel/exception.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "libsystem.h"

/** Convert a kernel exception code to a signal number. */
int exception_to_signal(unsigned code) {
    switch (code) {
        case EXCEPTION_ADDR_UNMAPPED:       return SIGSEGV;
        case EXCEPTION_ACCESS_VIOLATION:    return SIGSEGV;
        case EXCEPTION_STACK_OVERFLOW:      return SIGSEGV;
        case EXCEPTION_PAGE_ERROR:          return SIGBUS;
        case EXCEPTION_INVALID_ALIGNMENT:   return SIGBUS;
        case EXCEPTION_INVALID_INSTRUCTION: return SIGILL;
        case EXCEPTION_INT_DIV_ZERO:        return SIGFPE;
        case EXCEPTION_INT_OVERFLOW:        return SIGFPE;
        case EXCEPTION_FLOAT_DIV_ZERO:      return SIGFPE;
        case EXCEPTION_FLOAT_OVERFLOW:      return SIGFPE;
        case EXCEPTION_FLOAT_UNDERFLOW:     return SIGFPE;
        case EXCEPTION_FLOAT_PRECISION:     return SIGFPE;
        case EXCEPTION_FLOAT_DENORMAL:      return SIGFPE;
        case EXCEPTION_FLOAT_INVALID:       return SIGFPE;
        case EXCEPTION_BREAKPOINT:          return SIGTRAP;
        case EXCEPTION_ABORT:               return SIGABRT;
        default:
            libsystem_log(CORE_LOG_WARN, "unhandled exception code %u", code);
            return SIGKILL;
    }
}

/** Sends a signal to a process.
 * @param pid           ID of process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int kill(pid_t pid, int num) {
    libsystem_stub("raise", true);
    return -1;
}

/** Sends a signal to the current process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int raise(int num) {
    __asm__ volatile("ud2a");
    libsystem_stub("raise", true);
    return -1;
}

/** Examines or changes the action of a signal.
 * @param num           Signal number to modify.
 * @param act           Pointer to new action for signal (can be NULL).
 * @param oldact        Pointer to location to store previous action in (can
 *                      be NULL).
 * @return              0 on success, -1 on failure. */
int sigaction(int num, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
    //libsystem_stub("sigaction", false);
    return -1;
}

/** Sets the handler of a signal.
 * @param num           Signal number.
 * @param handler       Handler function.
 * @return              Previous handler, or SIG_ERR on failure. */
sighandler_t signal(int num, sighandler_t handler) {
    libsystem_stub("signal", false);
    return SIG_ERR;
}

/** Sets the signal mask.
 * @param how           How to set the mask.
 * @param set           Signal set to mask (can be NULL).
 * @param oset          Where to store previous masked signal set (can be NULL).
 * @return              0 on success, -1 on failure. */
int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset) {
    //libsystem_stub("sigprocmask", false);
    return -1;
}

/**
 * Gets and sets the alternate signal stack for the current thread. This stack
 * is used to execute signal handlers with the SA_ONSTACK flag set. The
 * alternate stack is a per-thread attribute. If fork() is called, the new
 * process' initial thread inherits the alternate stack from the thread that
 * called fork().
 *
 * @param ss            Alternate stack to set (can be NULL).
 * @param oset          Where to store previous alternate stack (can be NULL).
 *
 * @return              0 on success, -1 on failure.
 */
int sigaltstack(const stack_t *restrict ss, stack_t *restrict oldss) {
    libsystem_stub("sigaltstack", false);
    return -1;
}

int sigsuspend(const sigset_t *mask) {
    libsystem_stub("sigsuspend", true);
    return -1;
}

/**
 * Saves the current execution environment to be restored by a call to
 * siglongjmp(). If specified, the current signal mask will also be saved.
 *
 * @param env           Buffer to save to.
 * @param savemask      If not 0, the current signal mask will be saved.
 *
 * @return              0 if returning from direct invocation, non-zero if
 *                      returning from siglongjmp().
 */
int sigsetjmp(sigjmp_buf env, int savemask) {
    //if (savemask)
    //  sigprocmask(SIG_BLOCK, NULL, &env->mask);

    //env->restore_mask = savemask;
    return setjmp(env->buf);
}

/**
 * Restores an execution environment saved by a previous call to sigsetjmp().
 * If the original call to sigsetjmp() specified savemask as non-zero, the
 * signal mask at the time of the call will be restored.
 *
 * @param env           Buffer to restore.
 * @param val           Value that the original sigsetjmp() call should return.
 */
void siglongjmp(sigjmp_buf env, int val) {
    //if (env->restore_mask)
    //  sigprocmask(SIG_SETMASK, &env->mask, NULL);

    longjmp(env->buf, val);
}

/** Adds a signal to a signal set.
 * @param set           Set to add to.
 * @param num           Signal to add.
 * @return              0 on success, -1 on failure. */
int sigaddset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set |= (1 << num);
    return 0;
}

/** Removes a signal from a signal set.
 * @param set           Set to remove from.
 * @param num           Signal to remove.
 * @return              0 on success, -1 on failure. */
int sigdelset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~(1 << num);
    return 0;
}

/** Clears all signals in a signal set.
 * @param set           Set to clear.
 * @return              Always 0. */
int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

/** Sets all signals in a signal set.
 * @param set           Set to fill.
 * @return              Always 0. */
int sigfillset(sigset_t *set) {
    *set = -1;
    return 0;
}

/** Checks if a signal is included in a set.
 * @param set           Set to check.
 * @param num           Signal number to check for.
 * @return              1 if member, 0 if not, -1 if signal number is invalid. */
int sigismember(const sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    return (*set & (1 << num)) ? 1 : 0;
}

/** Array of signal strings. */
const char *const sys_siglist[NSIG] = {
    [SIGHUP]   = "Hangup",
    [SIGINT]   = "Interrupt",
    [SIGQUIT]  = "Quit",
    [SIGILL]   = "Illegal instruction",
    [SIGTRAP]  = "Trace trap",
    [SIGABRT]  = "Aborted",
    [SIGBUS]   = "Bus error",
    [SIGFPE]   = "Floating-point exception",
    [SIGKILL]  = "Killed",
    [SIGCHLD]  = "Child death/stop",
    [SIGSEGV]  = "Segmentation fault",
    [SIGSTOP]  = "Stopped",
    [SIGPIPE]  = "Broken pipe",
    [SIGALRM]  = "Alarm call",
    [SIGTERM]  = "Terminated",
    [SIGUSR1]  = "User signal 1",
    [SIGUSR2]  = "User signal 2",
    [SIGCONT]  = "Continued",
    [SIGURG]   = "Urgent I/O condition",
    [SIGTSTP]  = "Stopped (terminal)",
    [SIGTTIN]  = "Stopped (terminal input)",
    [SIGTTOU]  = "Stopped (terminal output)",
    [SIGWINCH] = "Window changed",
};

/** Gets the string representation of a signal number.
 * @return              Pointer to string. */
char *strsignal(int sig) {
    if (sig < 0 || sig >= NSIG)
        return (char *)"Unknown signal";

    return (char *)sys_siglist[sig];
}

/**
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param sig           Signal number to print.
 * @param s             Optional message to precede signal with.
 */
void psignal(int sig, const char *s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: %s\n", s, strsignal(sig));
    } else {
        fprintf(stderr, "%s\n", strsignal(sig));
    }
}

/**
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param info          Signal to print information on.
 * @param s             Optional message to precede signal with.
 */
void psiginfo(const siginfo_t *info, const char *s) {
    psignal(info->si_signo, s);
}
