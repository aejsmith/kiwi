/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		POSIX program execution function.
 */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

/**
 * Execute a binary in the PATH.
 *
 * If the given path contains a / character, this function will simply call
 * execve() with the given arguments and the current process' environment.
 * Otherwise, it will search the PATH for the name given and execute it
 * if found.
 *
 * @param file		Name of binary to execute.
 * @param argv		Arguments for process (NULL-terminated array).
 *
 * @return		Does not return on success, -1 on failure.
 */
int execvp(const char *file, char *const argv[]) {
	const char *path = getenv("PATH");
	char buf[PATH_MAX];
	char *cur, *next;
	size_t len;

	/* Use the default path if PATH is not set in the environment */
	if(path == NULL)
		path = "/system/bin";

	/* If file contains a /, just run it */
	if(strchr(file, '/'))
		return execve(file, argv, environ);

	for(cur = (char *)path; cur; cur = next) {
		next = strchr(cur, ':');
		if(!next)
			next = cur + strlen(cur);

		if(next == cur) {
			buf[0] = '.';
			cur--;
		} else {
			if((next - cur) >= (PATH_MAX - 3)) {
				errno = EINVAL;
				return -1;
			}

			memmove(buf, cur, (size_t)(next - cur));
		}

		buf[next - cur] = '/';
		len = strlen(file);
		if(len + (next - cur) >= (PATH_MAX - 2)) {
			errno = EINVAL;
			return -1;
		}

		memmove(&buf[next - cur + 1], file, len + 1);
		if(execve(buf, argv, environ) == -1) {
			if((errno != EACCES) && (errno != ENOENT) && (errno != ENOTDIR))
				return -1;
		}

		if(*next == 0)
			break;
		next++;
	}

	return -1;
}

/**
 * Execute a binary.
 *
 * Executes a binary with the given arguments and a copy of the calling
 * process' environment.
 *
 * @param path		Path to binary to execute.
 * @param argv		Arguments for process (NULL-terminated array).
 *
 * @return		Does not return on success, -1 on failure.
 */
int execv(const char *path, char *const argv[]) {
	return execve(path, argv, environ);
}
