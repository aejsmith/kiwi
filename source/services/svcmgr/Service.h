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
 * @brief		Service manager service class.
 */

#ifndef __SERVICE_H
#define __SERVICE_H

#include <kiwi/Process.h>

#include <list>
#include <string>

#include "Port.h"

/** Class defining a service known to the service manager. */
class Service {
public:
	/** Possible service states. */
	enum State {
		Stopped,			/**< Service is stopped. */
		Running,			/**< Service is running. */
	};

	/** Behaviour flags. */
	enum Flags {
		OnDemand = 1,			/**< Should only be started when a port is needed. */
	};

	Service(const char *name, const char *desc, const char *cmdline, int flags = 0, const char *port = 0);
	bool start();

	/** Get the service's flags.
	 * @return		Behaviour flags for the service. */
	int getFlags() const { return m_flags; }

	/** Get the service's state.
	 * @return		State of the service. */
	State getState() const { return m_state; }

	/** Get a reference to the port list.
	 * @return		Reference to the port list. */
	Port *getPort() const { return m_port; }
private:
	void processExited(int status);

	std::string m_name;		/**< Name of the service. */
	std::string m_description;	/**< Description of the service. */
	std::string m_cmdline;		/**< Command line for the service. */
	int m_flags;			/**< Behaviour flags. */
	Port *m_port;			/**< Port for this service. */

	State m_state;			/**< State of the service. */
	kiwi::Process m_process;	/**< Process for the service. */
};

#endif /* __SERVICE_H */