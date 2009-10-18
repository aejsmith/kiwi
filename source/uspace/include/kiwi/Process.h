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

#include <kernel/process.h>

#include <kiwi/Handle.h>

namespace kiwi {

/** Class providing functionality to create and manipulate processes. */
class Process : public Handle {
public:
	Process(char **args, char **env = 0, bool usepath = true, int flags = PROCESS_CREATE_INHERIT);
	Process(const char *cmdline, char **env = 0, bool usepath = true, int flags = PROCESS_CREATE_INHERIT);
	Process(identifier_t id);

	bool Initialised(int *status = 0) const;
	int WaitTerminate(timeout_t timeout = -1) const;
	identifier_t GetID(void) const;

	static identifier_t GetCurrentID(void);
private:
	void _Init(char **args, char **env, bool usepath, int flags);

	int m_init_status;		/**< Initialisation status. */
};

}

#endif /* __KIWI_PROCESS_H */
