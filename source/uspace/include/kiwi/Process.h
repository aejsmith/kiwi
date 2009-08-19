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

KIWI_BEGIN_NAMESPACE

/** Class providing functionality to create and manipulate other processes. */
class Process : Handle {
public:
	/** Create a new process.
	 * @param process	Pointer to store object pointer in.
	 * @param args		NULL-terminated argument array (first entry
	 *			should be the path to the program to run).
	 * @param env		NULL-terminated environment variable array. A
	 *			NULL value for this argument will result in the
	 *			new process inheriting the current environment.
	 *			This is the default.
	 * @param inherit	Whether the new process should inherit handles
	 *			that are marked as inheritable; defaults to
	 *			true.
	 * @return		0 on success, negative error code on failure. */
	static int create(Process *&process, char **args, char **env = 0, bool inherit = true);

	/** Create a new process.
	 * @param process	Pointer to store object pointer in.
	 * @param cmdline	Command line string, each argument seperated by
	 *			spaces. First part of the string should be the
	 *			path to the program to run.
	 * @param env		NULL-terminated environment variable array. A
	 *			NULL value for this argument will result in the
	 *			new process inheriting the current environment.
	 *			This is the default.
	 * @param inherit	Whether the new process should inherit handles
	 *			that are marked as inheritable; defaults to
	 *			true.
	 * @return		0 on success, negative error code on failure. */
	static int create(Process *&process, const char *cmdline, char **env = 0, bool inherit = true);

	/** Open an existing process.
	 * @param process	Pointer to store object pointer in.
	 * @param id		ID of the process to open.
	 * @return		0 on success, negative error code on failure. */
	static int open(Process *&process, identifier_t id);

	/** Get the ID of the process this object refers to.
	 * @return		ID of the process. */
	identifier_t get_id(void);

	/** Get the ID of the current process.
	 * @return		ID of the current process. */
	static identifier_t get_current_id(void);
protected:
	/** Process objects must be created via the create/open functions. */
	Process() {}
};

KIWI_END_NAMESPACE

#endif /* __KIWI_PROCESS_H */
