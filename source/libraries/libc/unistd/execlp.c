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
 * @brief		POSIX program execution function.
 */

#include <stdarg.h>
#include <unistd.h>

#define ARGV_MAX	512

/** Execute a binary in the PATH.
 *
 * If the given path contains a / character, this function will simply call
 * execve() with the given arguments and the current process' environment.
 * Otherwise, it will search the PATH for the name given and execute it
 * if found.
 *
 * @param file		Name of binary to execute.
 * @param arg		Arguments for process (NULL-terminated argument list).
 *
 * @return		Does not return on success, -1 on failure.
 */
int execlp(const char *file, const char *arg, ...) {
	int i;
	va_list ap;
	const char *argv[ARGV_MAX];

	argv[0] = arg;
	va_start(ap, arg);

	for(i = 1; i < ARGV_MAX; i++) {
		argv[i] = va_arg(ap, const char *);
		if(argv[i] == NULL) {
			break;
		}
	}

	va_end(ap);
	return execvp(file, (char *const *)argv);
}

/** Execute a binary.
 *
 * Executes a binary with the given arguments and the current process'
 * environment.
 *
 * @param path		Path to binary to execute.
 * @param arg		Arguments for process (NULL-terminated argument list).
 *
 * @return		Does not return on success, -1 on failure.
 */
int execl(const char *path, const char *arg, ...) {
	int i;
	va_list ap;
	const char *argv[ARGV_MAX];

	argv[0] = arg;
	va_start(ap, arg);

	for(i = 1; i < ARGV_MAX; i++) {
		argv[i] = va_arg(ap, const char *);
		if(argv[i] == NULL) {
			break;
		}
	}

	va_end(ap);
	return execv(path, (char *const *)argv);
}
