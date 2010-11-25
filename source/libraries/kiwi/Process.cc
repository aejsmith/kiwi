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
#include <kernel/object.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "Internal.h"

using namespace kiwi;

/** Construct the process object.
 * @note		The process is not created here. Once the object has
 *			been initialised, you can either open an existing
 *			process using Open(), or start a new process using
 *			Create().
 * @param handle	If not negative, a existing process handle to make the
 *			object use. Must refer to a process object. */
Process::Process(handle_t handle) {
	if(handle >= 0) {
		if(unlikely(object_type(handle) != OBJECT_TYPE_PROCESS)) {
			libkiwi_fatal("Thread::Thread: Handle must refer to a thread object.");
		}

		SetHandle(handle);
	}
}

/** Create a new process.
 *
 * Creates a new process. If the object currently refers to a process, the old
 * process will be closed upon success, and the object will refer to the new
 * process. Upon failure, the old process will remain open.
 *
 * @param args		NULL-terminated argument array. First entry should be
 *			the path to the program to run. If this does not contain
 *			a / character, it will be searched for in the
 *			directories specified by the PATH environment variable.
 *			If you wish to execute a file in the current directory,
 *			you should use "./file" as the path string.
 * @param env		NULL-terminated environment variable array. The default
 *			is to use the current environment. Must not be NULL.
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process. Be warned that handles created through the C
 *			and C++ standard libraries are marked as inheritable to
 *			support POSIX behaviour.
 *
 * @return		True if successful, false if an error occurred. Error
 *			information can be retrieved by calling GetError().
 */
bool Process::Create(const char *const args[], const char *const env[], HandleMap *handles) {
	std::unique_ptr<handle_t [][2]> map;
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
			size_t i = 0;

			map.reset(new handle_t[mapsz][2]);
			for(auto it = handles->begin(); it != handles->end(); ++it) {
				map[i][0] = it->first;
				map[i++][1] = it->second;
			}
		}
	}

	/* If the given string does not contain a / character, search for it in
	 * the path. */
	if(!strchr(args[0], '/')) {
		const char *path, *current, *next;

		path = getenv("PATH");
		if(!path) {
			path = "/system/binaries";
		}

		for(current = path; current; current = next) {
			size_t name_len, path_len;
			char buf[FS_PATH_MAX];

			/* Work out the length of this entry. */
			next = strchr(current, ':');
			if(next) {
				path_len = next - current;
				next++;
			} else {
				path_len = strlen(current);
			}

			if(!path_len) {
				/* A zero-length entry means current directory. */
				buf[0] = '.';
				path_len = 1;
			} else {
				/* Copy the path into the buffer. */
				if(unlikely(path_len >= (FS_PATH_MAX - 3))) {
					m_error = STATUS_INVALID_ARG;
					return false;
				}
				memcpy(buf, current, path_len);
			}

			name_len = strlen(args[0]);
			if(unlikely(path_len + name_len >= (FS_PATH_MAX - 2))) {
				m_error = STATUS_INVALID_ARG;
				return false;
			}

			/* Place the file name in the buffer. */
			buf[path_len] = '/';
			memcpy(&buf[path_len + 1], args[0], name_len + 1);

			/* Try to create the process using this path string. */
			ret = process_create(buf, args, env, 0, NULL, map.get(), mapsz, NULL,
			                     PROCESS_QUERY, &handle);
			if(ret == STATUS_SUCCESS) {
				SetHandle(handle);
				return true;
			}

			/* Continue searching the rest of the path if the entry
			 * was not found, or we do not have execute permission
			 * on the file. */
			if(unlikely(ret != STATUS_NOT_FOUND && ret != STATUS_NOT_DIR &&
			            ret != STATUS_ACCESS_DENIED)) {
				m_error = ret;
				return false;
			}
		}

		m_error = STATUS_NOT_FOUND;
		return false;
	} else {
		ret = process_create(args[0], args, env, 0, NULL, map.get(), mapsz, NULL,
		                     PROCESS_QUERY, &handle);
		if(unlikely(ret != STATUS_SUCCESS)) {
			m_error = ret;
			return false;
		}

		SetHandle(handle);
		return true;
	}
}

/** Create a new process.
 *
 * Creates a new process. If the object currently refers to a process, the old
 * process will be closed upon success, and the object will refer to the new
 * process. Upon failure, the old process will remain open.
 *
 * @param cmdline	Command line string with each argument separated by a
 *			space character. First part of the string should be the
 *			path to the program to run. If this does not contain a
 *			/ character, it will be searched for in the directories
 *			specified by the PATH environment variable. If you wish
 *			to execute a file in the current directory, you should
 *			use "./file" as the path string.
 * @param env		NULL-terminated environment variable array. The default
 *			is to use the current environment. Must not be NULL.
 * @param handles	Pointer to map describing how to duplicate handles from
 *			the calling process into the new process. If NULL, all
 *			inheritable handles will be duplicated to the new
 *			process. Be warned that handles created through the C
 *			and C++ standard libraries are marked as inheritable to
 *			support POSIX behaviour.
 *
 * @return		True if successful, false if an error occurred. Error
 *			information can be retrieved by calling GetError().
 */
bool Process::Create(const char *cmdline, const char *const env[], HandleMap *handles) {
	char *tok, *dup;

	assert(cmdline && cmdline[0]);
	assert(env);

	/* Duplicate the command line string so we can modify it. */
	std::unique_ptr<char []> orig(new char[strlen(cmdline) + 1]);
	strcpy(orig.get(), cmdline);
	dup = orig.get();

	/* Create a vector from each token. */
	std::vector<char *> args;
	while((tok = strsep(&dup, " "))) {
		if(tok[0]) {
			args.push_back(tok);
		}
	}

	/* Null-terminate the array. */
	args.push_back(0);

	/* Create the process. */
	return Create(&args[0], env, handles);
}

/** Open an existing process.
 *
 * Opens an existing process. If the object currently refers to a process, the
 * old process will be closed upon success, and the object will refer to the
 * new process. Upon failure, the old process will remain open.
 *
 * @param id		ID of the process to open.
 *
 * @return		True if successful, false if an error occurred. Error
 *			information can be retrieved by calling GetError().
 */
bool Process::Open(process_id_t id) {
	handle_t handle;
	status_t ret;

	ret = process_open(id, PROCESS_QUERY, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		m_error = ret;
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Wait for the process to die.
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the process has not already
 *			terminated, and a value of -1 (the default) will block
 *			indefinitely until the process terminates.
 * @return		True if process exited within timeout, false if not. */
bool Process::Wait(useconds_t timeout) const {
	return (_Wait(PROCESS_EVENT_DEATH, timeout) == STATUS_SUCCESS);
}

/** Check whether the process is running.
 * @return		Whether the process is running. */
bool Process::IsRunning() const {
	int status;
	return (m_handle >= 0 && process_status(m_handle, &status) == STATUS_STILL_RUNNING);
}

/** Get the exit status of the process.
 * @return		Exit status of the process, or -1 if still running. */
int Process::GetStatus() const {
	int status;

	if(process_status(m_handle, &status) != STATUS_SUCCESS) {
		return -1;
	}
	return status;
}

/** Get the ID of the process.
 * @return		ID of the process. */
process_id_t Process::GetID(void) const {
	return process_id(m_handle);
}

/** Get the ID of the current process.
 * @return		ID of the current process. */
process_id_t Process::GetCurrentID(void) {
	return process_id(-1);
}

/** Register events with the event loop. */
void Process::RegisterEvents() {
	RegisterEvent(PROCESS_EVENT_DEATH);
}

/** Callback for an object event being received.
 * @param event		Event ID received. */
void Process::HandleEvent(int event) {
	if(event == PROCESS_EVENT_DEATH) {
		int status = 0;
		process_status(m_handle, &status);
		OnExit(status);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		UnregisterEvent(PROCESS_EVENT_DEATH);
	}
}
