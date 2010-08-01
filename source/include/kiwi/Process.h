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

#ifndef __KIWI_PROCESS_H
#define __KIWI_PROCESS_H

#include <kiwi/Handle.h>
#include <utility>
#include <vector>

extern char **environ;

namespace kiwi {

/** Class providing functionality to create and manipulate processes. */
class Process : public Handle {
public:
	/** Type of the handle map. */
	typedef std::vector<std::pair<handle_t, handle_t> > HandleMap;

	Process(handle_t handle = -1);

	bool create(const char *const args[], const char *const env[] = environ,
	            HandleMap *handles = 0);
	bool create(const char *cmdline, const char *const env[] = environ,
	            HandleMap *handles = 0);
	bool open(process_id_t id);

	bool waitTerminate(useconds_t timeout = -1) const;
	process_id_t getID(void) const;

	static process_id_t getCurrentID(void);

	Signal<int> onExit;
private:
	void registerEvents();
	void eventReceived(int event);
};

}

#endif /* __KIWI_PROCESS_H */
