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

/** Create a new process.
 * @param process	Pointer to store object pointer in.
 * @param args		NULL-terminated argument array (first entry should be
 *			the path to the program to run).
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment. This is the default.
 * @param inherit	Whether the new process should inherit handles that are
 *			marked as inheritable; defaults to true.
 * @return		0 on success, negative error code on failure. */
int Process::create(Process *&process, char **args, char **env, bool inherit) {
	handle_t ret;

	/* NULL value for env means to use current environment. */
	if(!env) {
		env = environ;
	}

	if(!(process = new Process)) {
		return -ERR_NO_MEMORY;
	} else if((ret = process_create(args, env, inherit)) < 0) {
		delete process;
		return ret;
	}

	process->m_handle = ret;
	return 0;
}

/** Create a new process.
 * @param process	Pointer to store object pointer in.
 * @param cmdline	Command line string, each argument seperated by spaces.
 *			First part of the string should be the path to the
 *			program to run.
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment. This is the default.
 * @param inherit	Whether the new process should inherit handles that are
 *			marked as inheritable; defaults to true.
 * @return		0 on success, negative error code on failure. */
int Process::create(Process *&process, const char *cmdline, char **env, bool inherit) {
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

/** Open an existing process.
 * @param process	Pointer to store object pointer in.
 * @param id		ID of the process to open.
 * @return		0 on success, negative error code on failure. */
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
