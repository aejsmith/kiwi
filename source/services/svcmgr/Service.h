/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
