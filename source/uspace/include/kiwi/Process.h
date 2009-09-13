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

#ifndef __KIWI_PROCESS_H
#define __KIWI_PROCESS_H

#include <kiwi/Handle.h>

namespace kiwi {

/** Class providing functionality to create and manipulate processes. */
class Process : public Handle {
public:
	/** Create a new process.
	 * @note		After creating the object you should call
	 *			Initialised() to check if initialisation
	 *			succeeded.
	 * @param args		NULL-terminated argument array. First entry
	 *			should be the path to the program to run.
	 * @param env		NULL-terminated environment variable array. A
	 *			NULL value for this argument will result in the
	 *			new process inheriting the current environment.
	 *			This is the default.
	 * @param inherit	Whether the new process should inherit handles
	 *			that are marked as inheritable; defaults to
	 *			true.
	 * @param usepath	If true, and the program path does not contain
	 *			a '/' character, then it will be looked up in
	 *			all directories listed in the PATH environment
	 *			variable. The first match will be executed. The
	 *			default value for this is true. */
	Process(char **args, char **env = 0, bool inherit = true, bool usepath = true);

	/** Create a new process.
	 * @note		After creating the object you should call
	 *			Initialised() to check if initialisation
	 *			succeeded.
	 * @param cmdline	Command line string, each argument seperated by
	 *			spaces. First part of the string should be the
	 *			path to the program to run. If this first
	 *			argument does not contain a '/' character, then
	 *			the name will be looked up inthe directories listed in the PATH environment
	 *			variable, and the first match will be executed.
	 * @param env		NULL-terminated environment variable array. A
	 *			NULL value for this argument will result in the
	 *			new process inheriting the current environment.
	 *			This is the default.
	 * @param inherit	Whether the new process should inherit handles
	 *			that are marked as inheritable; defaults to
	 *			true.
	 * @param usepath	If true, and the program path does not contain
	 *			a '/' character, then it will be looked up in
	 *			all directories listed in the PATH environment
	 *			variable. The first match will be executed. The
	 *			default value for this is true. */
	Process(const char *cmdline, char **env = 0, bool inherit = true, bool usepath = true);

	/** Open an existing process.
	 * @note		After creating the object you should call
	 *			Initialised() to check if initialisation
	 *			succeeded.
	 * @param id		ID of the process to open. */
	Process(identifier_t id);

	/** Check whether initialisation was successful.
	 * @param status	Optional pointer to integer to store error
	 *			code in if not successful.
	 * @return		True if successful, false if not. */
	bool Initialised(int *status = 0) const;

	/** Wait for the process to die.
	 * @param timeout	Timeout in microseconds. A value of 0 will
	 *			return an error immediately if the process has
	 *			not already terminated, and a value of -1 (the
	 *			default) will block indefinitely until the
	 *			process terminates.
	 * @return		0 on success, error code on failure. */
	int WaitTerminate(timeout_t timeout = -1) const;

	/** Get the ID of the process.
	 * @return		ID of the process. */
	identifier_t GetID(void) const;

	/** Get the ID of the current process.
	 * @return		ID of the current process. */
	static identifier_t GetCurrentID(void);
private:
	/** Internal creation function.
	 * @param args		NULL-terminated argument array.
	 * @param env		NULL-terminated environment variable array.
	 * @param inherit	Whether the new process should inherit handles.
	 * @param usepath	Whether to use the PATH environment variable. */
	void _Init(char **args, char **env, bool inherit, bool usepath);

	int m_init_status;		/**< Initialisation status. */
};

};

#endif /* __KIWI_PROCESS_H */
