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

class Connection;
class ServiceManager;

/** Class defining a service known to the service manager. */
class Service {
public:
	/** Possible service states. */
	enum State {
		kStopped,		/**< Service is stopped. */
		kRunning,		/**< Service is running. */
	};

	/** Behaviour flags. */
	enum Flags {
		kOnDemand = (1<<0),	/**< Should only be started when a port is needed. */
		kCritical = (1<<1),	/**< Raise a fatal error if the service exits. */
	};

	Service(ServiceManager *svcmgr, const char *name, const char *desc, const char *cmdline,
	        int flags = 0, const char *port = 0);
	bool Start();

	/** Get the service's flags.
	 * @return		Behaviour flags for the service. */
	int GetFlags() const { return m_flags; }

	/** Get the service's state.
	 * @return		State of the service. */
	State GetState() const { return m_state; }

	/** Get the service's port.
	 * @return		Pointer to service's port. */
	Port *GetPort() const { return m_port; }
private:
	static void StartHelper(void *data);
	void ProcessExited(int status);

	ServiceManager *m_svcmgr;	/**< Service manager the service is for. */
	std::string m_name;		/**< Name of the service. */
	std::string m_description;	/**< Description of the service. */
	std::string m_cmdline;		/**< Command line for the service. */
	int m_flags;			/**< Behaviour flags. */
	Port *m_port;			/**< Port for this service. */
	State m_state;			/**< State of the service. */
	kiwi::Process m_process;	/**< Process for the service. */
	Connection *m_conn;		/**< Connection to the service. */
};

#endif /* __SERVICE_H */
