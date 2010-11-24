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
 * @brief		Signal string functions.
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
	[SIGPOLL]  = "I/O ready",
	[SIGWINCH] = "Window changed",
};

/** Get string representation of a signal number.
 * @return		Pointer to string. */
char *strsignal(int sig) {
	if(sig < 0 || sig >= NSIG) {
		return (char *)"Unknown signal";
	}
	return (char *)sys_siglist[sig];
}

/** Print string representation of signal.
 *
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param sig		Signal number to print.
 * @param s		Optional message to precede signal with.
 */
void psignal(int sig, const char *s) {
	if(s && s[0]) {
		fprintf(stderr, "%s: %s\n", s, strsignal(sig));
	} else {
		fprintf(stderr, "%s\n", strsignal(sig));
	}
}
