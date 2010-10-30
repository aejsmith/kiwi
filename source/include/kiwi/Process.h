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

/** Class providing details of a process error.
 * @todo		Provide details of missing libraries/symbols. */
class KIWI_PUBLIC ProcessError : public Error {
public:
	ProcessError() {}
	ProcessError(status_t code) : Error(code) {}
};

/** Class providing functionality to create and manipulate processes. */
class KIWI_PUBLIC Process : public Handle {
public:
	/** Type of the handle map.
	 * @todo		Replace this. */
	typedef std::vector<std::pair<handle_t, handle_t> > HandleMap;

	Process(handle_t handle = -1);

	bool Create(const char *const args[], const char *const env[] = environ,
	            HandleMap *handles = 0);
	bool Create(const char *cmdline, const char *const env[] = environ,
	            HandleMap *handles = 0);
	bool Open(process_id_t id);
	bool Wait(useconds_t timeout = -1) const;

	bool IsRunning() const;
	int GetStatus() const;
	process_id_t GetID() const;

	/** Get information about the last error that occurred.
	 * @return		Reference to error object for last error. */
	const ProcessError &GetError() const { return m_error; }

	/** Signal emitted when the process exits.
	 * @param		Exit status code. */
	Signal<int> OnExit;

	static process_id_t GetCurrentID(void);
private:
	void RegisterEvents();
	void HandleEvent(int event);

	ProcessError m_error;		/**< Error information. */
};

}

#endif /* __KIWI_PROCESS_H */
