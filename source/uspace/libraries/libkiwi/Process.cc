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

#include <kernel/errors.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace kiwi;
using namespace std;

/* FIXME. */
#define PATH_MAX 4096

/** Constructor for Process.
 * @param handle	Handle ID (default is -1, which means the object will
 *			not refer to a handle). */
Process::Process(handle_t handle) {
	setHandle(handle);
}

/** Create a new process.
 *
 * Creates a new process. If the object currently refers to a process, the old
 * process will be closed upon success, and the object will refer to the new
 * process. Upon failure, the old process will remain open.
 *
 * @param args		NULL-terminated argument array. First entry should be
 *			the path to the program to run.
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment (the default).
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process.
 *
 * @return		Whether creation was successful.
 */
bool Process::create(const char *const args[], const char *const env[], HandleMap *handles) {
	handle_t (*map)[2] = 0;
	size_t mapsz = -1;
	handle_t handle;

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
			char buf[PATH_MAX];
			size_t len;

			if(!(next = strchr(cur, ':'))) {
				next = cur + strlen(cur);
			}

			if(next == cur) {
				buf[0] = '.';
				cur--;
			} else {
				if((next - cur) >= (PATH_MAX - 3)) {
					goto fail;
				}

				memcpy(buf, cur, next - cur);
			}

			buf[next - cur] = '/';
			len = strlen(args[0]);
			if(len + (next - cur) >= (PATH_MAX - 2)) {
				goto fail;
			}

			memcpy(&buf[next - cur + 1], args[0], len + 1);

			handle = process_create(buf, args, (env) ? env : environ, 0, map, mapsz);
			if(handle >= 0) {
				goto success;
			} else if(errno != ERR_NOT_FOUND) {
				goto fail;
			}

			if(*next == 0) {
				break;
			}
			next++;
		}

		goto fail;
	} else {
		handle = process_create(args[0], args, (env) ? env : environ, 0, map, mapsz);
		if(handle < 0) {
			goto fail;
		}

		goto success;
	}
success:
	if(map) { delete[] map; }
	setHandle(handle);
	return true;
fail:
	if(map) { delete[] map; }
	return false;
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
 * @param env		NULL-terminated environment variable array. A NULL
 *			value for this argument will result in the new process
 *			inheriting the current environment (the default).
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process.
 *
 * @return		Whether creation was successful.
 */
bool Process::create(const char *cmdline, const char *const env[], HandleMap *handles) {
	vector<char *> args;
	char *tok, *dup;

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

	if(!args.size()) {
		return false;
	}

	/* Null-terminate the array. */
	args.push_back(0);

	return create(&args[0], env, handles);
}

/** Open an existing process.
 *
 * Opens an existing process. If the object currently refers to a process, the
 * old process will be closed upon success, and the object will refer to the
 * new process. Upon failure, the old process will remain open.
 *
 * @param id		ID of the process to open.
 *
 * @return		Whether opening the process was successful.
 */
bool Process::open(process_id_t id) {
	handle_t handle = process_open(id);
	if(handle < 0) {
		return false;
	}

	setHandle(handle);
	return true;
}

/** Wait for the process to die.
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the process has not already
 *			terminated, and a value of -1 (the default) will block
 *			indefinitely until the process terminates.
 * @return		True on success, false on failure. */
bool Process::waitTerminate(useconds_t timeout) const {
	return wait(PROCESS_EVENT_DEATH, timeout);
}

/** Get the ID of the process.
 * @return		ID of the process. */
process_id_t Process::getID(void) const {
	return process_id(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
process_id_t Process::getCurrentID(void) {
	return process_id(-1);
}

/** Register events with the event loop. */
void Process::registerEvents() {
	registerEvent(PROCESS_EVENT_DEATH);
}

/** Callback for an object event being received.
 * @param event		Event ID received. */
void Process::eventReceived(int event) {
	if(event == PROCESS_EVENT_DEATH) {
		int status = 0;
		process_status(m_handle, &status);
		onExit(status);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		unregisterEvent(PROCESS_EVENT_DEATH);
	}
}
