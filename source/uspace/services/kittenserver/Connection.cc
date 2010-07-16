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

#include <kernel/errors.h>
#include <iostream>
#include "Connection.h"

using namespace std;
using namespace kiwi;

/** Construct a connection object.
 * @param handle	Handle to the connection. */
Connection::Connection(handle_t handle) :
	org::kiwi::KittenServer::ClientConnection(handle),
	m_current_kitten(NULL)
{
}

/** Create a new kitten.
 * @note		The kitten will be set as the current kitten.
 * @param name		Name of the kitten.
 * @param colour	Colour of the kitten.
 * @param id		Where to store ID of kitten.
 * @return		Result of the call. */
RPCResult Connection::createKitten(std::string name, Kitten::Colour colour, Kitten::ID &id) {
	Kitten *kitten;

	cout << "Connection::createKitten(" << name << ", {" << static_cast<int>(colour.red) << ',';
	cout << static_cast<int>(colour.green) << ',' << static_cast<int>(colour.blue) << "})" << endl;
	kitten = new Kitten(name, colour, this);
	id = kitten->getID();
	m_current_kitten = kitten;
	return 0;
}

/** Set the current kitten.
 * @param id		ID of kitten to set as current.
 * @return		Result of the call. */
RPCResult Connection::setCurrentKitten(Kitten::ID id) {
	Kitten *kitten;

	cout << "Connection::setCurrentKitten(" << id << ")" << endl;
	kitten = Kitten::lookup(id);
	if(!kitten) {
		return ERR_NOT_FOUND;
	} else if(!kitten->isOwner(this)) {
		return ERR_PERM_DENIED;
	}

	m_current_kitten = kitten;
	return 0;
}

/** Get the name of the current kitten.
 * @param name		Where to store name of kitten.
 * @return		Result of the call. */
RPCResult Connection::getName(std::string &name) {
	cout << "Connection::getName()" << endl;
	if(!m_current_kitten) {
		return ERR_NOT_FOUND;
	}

	name = m_current_kitten->getName();
	return 0;
}

/** Get the colour of the current kitten.
 * @param colour	Where to store colour of kitten.
 * @return		Result of the call. */
RPCResult Connection::getColour(Kitten::Colour &colour) {
	cout << "Connection::getColour()" << endl;
	if(!m_current_kitten) {
		return ERR_NOT_FOUND;
	}

	colour = m_current_kitten->getColour();
	return 0;
}

/** Stroke the current kitten.
 * @param duration	How long to stroke the kitten for (in seconds).
 * @return		Result of the call. */
RPCResult Connection::stroke(int32_t duration) {
	cout << "Connection::stroke(" << duration << ")" << endl;
	if(!m_current_kitten) {
		return ERR_NOT_FOUND;
	}

	m_current_kitten->stroke(duration);
	return 0;
}
