/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief               Signal string functions.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>

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

/** Get string representation of a signal number.
 * @return              Pointer to string. */
char *strsignal(int sig) {
    if (sig < 0 || sig >= NSIG)
        return (char *)"Unknown signal";

    return (char *)sys_siglist[sig];
}

/**
 * Print string representation of signal.
 *
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
 * Print string representation of signal.
 *
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param info          Signal to print information on.
 * @param s             Optional message to precede signal with.
 */
void psiginfo(const siginfo_t *info, const char *s) {
    psignal(info->si_signo, s);
}
