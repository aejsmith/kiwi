/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		RPC test server.
 */

#ifndef __KITTEN_H
#define __KITTEN_H

#include <map>
#include <string>
#include "ClientConnection.h"

class Connection;

/** Class representing a kitten. */
class Kitten {
public:
	/** Type for the ID of a kitten. */
	typedef org::kiwi::KittenServer::KittenID ID;

	/** Structure describing the colour of a kitten. */
	typedef org::kiwi::KittenServer::Colour Colour;
private:
	/** Type of the kitten map. */
	typedef std::map<Kitten::ID, Kitten *> KittenMap;
public:
	Kitten(std::string name, Colour colour, Connection *owner);

	/** Get the ID of the kitten.
	 * @return		ID of the kitten. */
	ID GetID() const { return m_id; }

	/** Get the name of the kitten.
	 * @return		Name of the kitten. */
	const std::string &GetName() const { return m_name; }

	/** Get the colour of the kitten.
	 * @return		Colour of the kitten. */
	Colour GetColour() const { return m_colour; }

	/** Check whether a connection is the owner of the kitten.
	 * @return		Whether the connection owns the kitten. */
	bool IsOwner(Connection *conn) const { return conn == m_owner; }

	void Stroke(int duration);

	static Kitten *Lookup(ID id);
private:
	ID m_id;			/**< ID of the kitten. */
	std::string m_name;		/**< Name of the kitten. */
	Colour m_colour;		/**< Colour of the kitten. */
	Connection *m_owner;		/**< Owner of the kitten. */

	static KittenMap m_kitten_map;	/**< Map of IDs to Kittens. */
	static ID m_next_id;		/**< Next kitten ID. */
};

#endif /* __KITTEN_H */
