/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Service manager port class.
 */

#ifndef __PORT_H
#define __PORT_H

#include <kiwi/IPCPort.h>
#include <string>

class Service;

/** Class containing details of a port. */
class Port {
public:
	Port(const char *name, Service *service);

	void StartListening();
	void StopListening();

	/** Get the name of the port.
	 * @return		Name of the port. */
	const std::string &GetName() const { return m_name; }

	/** Get the ID of the port.
	 * @return		ID of the port. */
	port_id_t GetID() const { return m_port.GetID(); }
private:
	void HandleConnection();

	std::string m_name;		/**< Name of the port. */
	kiwi::IPCPort m_port;		/**< Handle to the port. */
	Service *m_service;		/**< Service that the port belongs to. */
};

#endif /* __PORT_H */
