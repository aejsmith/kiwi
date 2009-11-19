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
#include <cstring>
#include <vector>

using namespace kiwi;
using namespace std;

extern char **environ;

/* FIXME. */
#define PATH_MAX 4096

/** Constructor for Process.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
Process::Process(handle_t handle) : Handle(handle) {
	if(m_handle >= 0) {
		_RegisterEvent(PROCESS_EVENT_DEATH);
	}
}

/** Create a new process.
 * @param args		NULL-terminated argument array. First entry should be
 *			the path to the program to run.
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment (the default).
 * @param usepath	If true, and the program path does not contain a '/'
 *			character, then it will be looked up in all directories
 *			listed in the PATH environment variable. The first
 *			match will be executed (defaults to true).
 * @param flags		Process creation flags.
 * @return		True on success, false on failure. */
bool Process::Create(char **args, char **env, bool usepath, int flags) {
	char buf[PATH_MAX];
	const char *path;
	char *cur, *next;
	size_t len;

	if(!Close()) {
		return false;
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
					return false;
				}

				memcpy(buf, cur, (size_t)(next - cur));
			}

			buf[next - cur] = '/';
			len = strlen(args[0]);
			if(len + (next - cur) >= (PATH_MAX - 2)) {
				return false;
			}

			memcpy(&buf[next - cur + 1], args[0], len + 1);

			if((m_handle = process_create(buf, args, (env) ? env : environ, flags)) >= 0) {
				goto success;
			} else if(m_handle != -ERR_NOT_FOUND) {
				return false;
			}

			if(*next == 0) {
				break;
			}
			next++;
		}

		return false;
	} else {
		if((m_handle = process_create(args[0], args, (env) ? env : environ, flags)) < 0) {
			return false;
		}
	}
success:
	_RegisterEvent(PROCESS_EVENT_DEATH);
	return true;
}

/** Create a new process.
 * @param cmdline	Command line string, each argument seperated by a
 *			space character. First part of the string should be the
 *			path to the program to run.
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment (the default).
 * @param usepath	If true, and the program path does not contain a '/'
 *			character, then it will be looked up in all directories
 *			listed in the PATH environment variable. The first
 *			match will be executed (defaults to true).
 * @param flags		Process creation flags.
 * @return		True on success, false on failure. */
bool Process::Create(const char *cmdline, char **env, bool usepath, int flags) {
	char *tok, *dup, *orig;
	vector<char *> args;
	bool ret;

	/* Duplicate the command line string so we can modify it. */
	if(!(orig = strdup(cmdline))) {
		return false;
	}
	dup = orig;

	/* Create a vector from each token. */
	while((tok = strsep(&dup, " "))) {
		if(!tok[0]) {
			continue;
		}
		args.push_back(tok);
	}

	if(!args.size()) {
		return false;
	}

	/* Null-terminate the array. */
	args.push_back(0);

	ret = Create(&args[0], env, usepath, flags);
	free(orig);
	return ret;
}

/** Open an existing process.
 * @param id		ID of the process to open.
 * @return		True on success, false on failure. */
bool Process::Open(identifier_t id) {
	if(!Close()) {
		return false;
	} else if((m_handle = process_open(id)) < 0) {
		return false;
	}

	_RegisterEvent(PROCESS_EVENT_DEATH);
	return true;
}

/** Wait for the process to die.
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the process has not already
 *			terminated, and a value of -1 (the default) will block
 *			indefinitely until the process terminates.
 * @return		True on success, false on failure. */
bool Process::WaitTerminate(timeout_t timeout) const {
	return Wait(PROCESS_EVENT_DEATH, timeout);
}

/** Get the ID of the process.
 * @return		ID of the process. */
identifier_t Process::GetID(void) const {
	return process_id(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
identifier_t Process::GetCurrentID(void) {
	return process_id(-1);
}

/** Callback for a handle event being received.
 * @param event		Event ID received. */
void Process::_EventReceived(int event) {
	switch(event) {
	case PROCESS_EVENT_DEATH:
		/* FIXME: Get status. */
		OnExit(0);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		_UnregisterEvent(PROCESS_EVENT_DEATH);
		break;
	}
}
