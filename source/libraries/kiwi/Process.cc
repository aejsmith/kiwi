/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <kernel/fs.h>
#include <kernel/process.h>

#include <kiwi/Error.h>
#include <kiwi/Process.h>

#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace kiwi;
using namespace std;

/** Construct the process object. It will not refer to a process. */
Process::Process() {}

/** Construct the object and create a new process.
 * @see			Process::Create(). */
Process::Process(const char *const args[], const char *const env[], HandleMap *handles) {
	Create(args, env, handles);
}

/** Construct the object and create a new process.
 * @see			Process::Create(). */
Process::Process(const char *cmdline, const char *const env[], HandleMap *handles) {
	Create(cmdline, env, handles);
}

/** Construct the object to refer to an existing process.
 * @see			Process::Open(). */
Process::Process(process_id_t id) {
	Open(id);
}

/** Create a new process.
 *
 * Creates a new process. If the object currently refers to a process, the old
 * process will be closed upon success, and the object will refer to the new
 * process. Upon failure, the old process will remain open.
 *
 * @param args		NULL-terminated argument array. First entry should be
 *			the path to the program to run.
 * @param env		NULL-terminated environment variable array. The default
 *			is to use the current environment.
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process. Be warned that handles created through the C
 *			and C++ standard libraries are marked as inheritable to
 *			support POSIX behaviour.
 *
 * @throw ProcessError	Thrown if the process could not be created.
 */
void Process::Create(const char *const args[], const char *const env[], HandleMap *handles) {
	handle_t (*map)[2] = 0;
	handle_t handle;
	int mapsz = -1;
	status_t ret;

	assert(args && args[0]);
	assert(env);

	/* If a handle map was provided, convert it into the format expected
	 * by the kernel. */
	if(handles) {
		mapsz = handles->size();
		if(mapsz) {
			map = new handle_t[mapsz][2];
			size_t i = 0;
			for(HandleMap::iterator it = handles->begin(); it != handles->end(); ++it) {
				map[i][0] = it->first;
				map[i++][1] = it->second;
			}
		}
	}

	if(!strchr(args[0], '/')) {
		const char *path = getenv("PATH");
		if(!path) {
			path = "/system/binaries";
		}

		char *cur, *next;
		for(cur = const_cast<char *>(path); cur; cur = next) {
			char buf[FS_PATH_MAX];
			size_t len;

			if(!(next = strchr(cur, ':'))) {
				next = cur + strlen(cur);
			}

			if(next == cur) {
				buf[0] = '.';
				cur--;
			} else {
				if((next - cur) >= (FS_PATH_MAX - 3)) {
					ret = STATUS_INVALID_ARG;
					goto fail;
				}

				memcpy(buf, cur, next - cur);
			}

			buf[next - cur] = '/';
			len = strlen(args[0]);
			if(len + (next - cur) >= (FS_PATH_MAX - 2)) {
				ret = STATUS_INVALID_ARG;
				goto fail;
			}

			memcpy(&buf[next - cur + 1], args[0], len + 1);

			ret = process_create(buf, args, (env) ? env : environ, 0, map, mapsz, &handle);
			if(ret == STATUS_SUCCESS) {
				goto success;
			} else if(ret != STATUS_NOT_FOUND && ret != STATUS_NOT_DIR) {
				goto fail;
			}

			if(*next == 0) {
				break;
			}
			next++;
		}

		ret = STATUS_NOT_FOUND;
		goto fail;
	} else {
		ret = process_create(args[0], args, (env) ? env : environ, 0, map, mapsz, &handle);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
		goto success;
	}
success:
	if(map) { delete[] map; }
	SetHandle(handle);
	return;
fail:
	if(map) { delete[] map; }
	throw ProcessError(ret);
}

/** Create a new process.
 *
 * Creates a new process. If the object currently refers to a process, the old
 * process will be closed upon success, and the object will refer to the new
 * process. Upon failure, the old process will remain open.
 *
 * @param cmdline	Command line string, each argument seperated by a
 *			space character. First part of the string should be the
 *			path to the program to run.
 * @param env		NULL-terminated environment variable array. The default
 *			is to use the current environment.
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process. Be warned that handles created through the C
 *			and C++ standard libraries are marked as inheritable to
 *			support POSIX behaviour.
 *
 * @throw ProcessError	Thrown if the process could not be created.
 */
void Process::Create(const char *cmdline, const char *const env[], HandleMap *handles) {
	vector<char *> args;
	char *tok, *dup;

	assert(cmdline && cmdline[0]);
	assert(env);

	/* Duplicate the command line string so we can modify it. */
	auto_ptr<char> orig(new char[strlen(cmdline) + 1]);
	strcpy(orig.get(), cmdline);
	dup = orig.get();

	/* Create a vector from each token. */
	while((tok = strsep(&dup, " "))) {
		if(!tok[0]) {
			continue;
		}
		args.push_back(tok);
	}

	/* Null-terminate the array. */
	args.push_back(0);

	/* Create the process. */
	Create(&args[0], env, handles);
}

/** Open an existing process.
 *
 * Opens an existing process. If the object currently refers to a process, the
 * old process will be closed upon success, and the object will refer to the
 * new process. Upon failure, the old process will remain open.
 *
 * @param id		ID of the process to open.
 *
 * @throw ProcessError	If the process could not be opened.
 */
void Process::Open(process_id_t id) {
	handle_t handle;
	status_t ret = process_open(id, &handle);
	if(ret != STATUS_SUCCESS) {
		throw ProcessError(ret);
	}

	SetHandle(handle);
}

/** Wait for the process to die.
 * @param statusp	If not NULL, where to store the exit code of the
 *			process.
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the process has not already
 *			terminated, and a value of -1 (the default) will block
 *			indefinitely until the process terminates.
 * @return		True if successful, false if the timeout expired. */
bool Process::WaitForExit(int *statusp, useconds_t timeout) const {
	if(!Wait(PROCESS_EVENT_DEATH, timeout)) {
		return false;
	}
	if(statusp) {
		process_status(m_handle, statusp);
	}
	return true;
}

/** Get the ID of the process.
 * @return		ID of the process. */
process_id_t Process::GetID(void) const {
	return process_id(m_handle);
}

/** Get the ID of the process' session.
 * @return		ID of the process' session. */
process_id_t Process::GetSessionID(void) const {
	return process_session(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
process_id_t Process::GetCurrentID(void) {
	return process_id(-1);
}

/** Get the ID of the current process' session.
 * @return		ID of the current process' session. */
process_id_t Process::GetCurrentSessionID(void) {
	return process_session(-1);
}

/** Register events with the event loop. */
void Process::RegisterEvents() {
	RegisterEvent(PROCESS_EVENT_DEATH);
}

/** Callback for an object event being received.
 * @param event		Event ID received. */
void Process::EventReceived(int event) {
	if(event == PROCESS_EVENT_DEATH) {
		int status = 0;
		process_status(m_handle, &status);
		OnExit(status);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		UnregisterEvent(PROCESS_EVENT_DEATH);
	}
}
