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

#include <utility>
#include "Connection.h"
#include "Kitten.h"

using namespace std;

/** Map of IDs to Kittens. */
Kitten::KittenMap Kitten::s_kitten_map;

/** Next kitten ID. */
Kitten::ID Kitten::s_next_id = 0;

/** Construct a Kitten.
 * @param name		Name of the kitten.
 * @param colour	Colour of the kitten.
 * @param owner		Connection that created the kitten. */
Kitten::Kitten(string name, Colour colour, Connection *owner) :
	m_id(s_next_id++), m_name(name), m_colour(colour), m_owner(owner)
{
	/* Add the kitten to the kitten map. */
	s_kitten_map.insert(make_pair(m_id, this));
}

/** Stroke a kitten.
 * @param duration	How long to stroke the kitten for. */
void Kitten::stroke(int duration) {
	m_owner->onPurr(duration);
}

/** Look up a kitten by ID.
 * @param id		ID of kitten to look up.
 * @return		Pointer to kitten if found, NULL if not. */
Kitten *Kitten::lookup(ID id) {
	KittenMap::iterator it;

	it = s_kitten_map.find(id);
	if(it == s_kitten_map.end()) {
		return NULL;
	}

	return it->second;
}
