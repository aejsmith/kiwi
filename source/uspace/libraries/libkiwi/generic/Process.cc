/* Kiwi process class
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Process class.
 */

#include <kernel/errors.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <cstdlib>
#include <string.h>

using namespace kiwi;

extern char **environ;

#if 0
# pragma mark Object creation functions.
#endif

/* FIXME. */
#define PATH_MAX 4096

/** Create a new process. */
int Process::create(Process *&process, char **args, char **env, bool inherit, bool usepath) {
	char buf[PATH_MAX];
	const char *path;
	char *cur, *next;
	handle_t ret;
	size_t len;

	if(!(process = new Process)) {
		return -ERR_NO_MEMORY;
	}

	if(usepath && !strchr(args[0], '/')) {
		if(!(path = getenv("PATH"))) {
			path = "/system/binaries";
		}

		for(cur = (char *)path; cur; cur = next) {
			if(!(next = strchr(cur, ':'))) {
				next = cur + strlen(cur);
			}

			if(next == cur) {
				buf[0] = '.';
				cur--;
			} else {
				if((next - cur) >= (PATH_MAX - 3)) {
					return -ERR_PARAM_INVAL;
				}

				memcpy(buf, cur, (size_t)(next - cur));
			}

			buf[next - cur] = '/';
			len = strlen(args[0]);
			if(len + (next - cur) >= (PATH_MAX - 2)) {
				return -ERR_PARAM_INVAL;
			}

			memcpy(&buf[next - cur + 1], args[0], len + 1);

			if((ret = process_create(buf, args, (env) ? env : environ, inherit)) >= 0) {
				process->m_handle = ret;
				return 0;
			} else if(ret != -ERR_NOT_FOUND) {
				delete process;
				return ret;
			}

			if(*next == 0) {
				break;
			}
			next++;
		}

		delete process;
		return -ERR_NOT_FOUND;
	} else {
		if((ret = process_create(args[0], args, (env) ? env : environ, inherit)) < 0) {
			delete process;
			return ret;
		}

		process->m_handle = ret;
		return 0;
	}
}

/** Create a new process. */
int Process::create(Process *&process, const char *cmdline, char **env, bool inherit, bool usepath) {
	char **args = NULL, **tmp, *tok, *dup, *orig;
	size_t count = 0;
	int ret;

	/* Duplicate the command line string so we can modify it. */
	if(!(orig = strdup(cmdline))) {
		return -ERR_NO_MEMORY;
	}
	dup = orig;

	/* Loop through each token of the command line and place them into an
	 * array. */
	while((tok = strsep(&dup, " "))) {
		if(!tok[0]) {
			continue;
		}

		if(!(tmp = reinterpret_cast<char **>(realloc(args, (count + 2) * sizeof(char *))))) {
			free(orig);
			free(args);
			return -ERR_NO_MEMORY;
		}
		args = tmp;

		/* Duplicating the token is not necessary, and not doing so
		 * makes it easier to handle failure - just free the array and
		 * duplicated string. */
		args[count++] = tok;
		args[count] = NULL;
	}

	if(!count) {
		return -ERR_PARAM_INVAL;
	}

	ret = create(process, args, env, inherit);
	free(args);
	free(orig);
	return ret;
}

/** Open an existing process. */
int Process::open(Process *&process, identifier_t id) {
	handle_t ret;

	if(!(process = new Process)) {
		return -ERR_NO_MEMORY;
	} else if((ret = process_open(id)) < 0) {
		delete process;
		return ret;
	}

	process->m_handle = ret;
	return 0;
}

#if 0
# pragma mark Object manipulation functions.
#endif

/** Get the ID of the process this object refers to.
 * @return		ID of the process. */
identifier_t Process::get_id(void) {
	return process_id(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
identifier_t Process::get_current_id(void) {
	return process_id(-1);
}
