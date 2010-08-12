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

#include <kernel/status.h>
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
status_t Connection::CreateKitten(const string &name, Kitten::Colour colour, Kitten::ID &id) {
	cout << "Connection::CreateKitten(" << name << ", {" << static_cast<int>(colour.red) << ',';
	cout << static_cast<int>(colour.green) << ',' << static_cast<int>(colour.blue) << "})" << endl;

	Kitten *kitten = new Kitten(name, colour, this);
	id = kitten->GetID();
	m_current_kitten = kitten;
	return STATUS_SUCCESS;
}

/** Set the current kitten.
 * @param id		ID of kitten to set as current.
 * @return		Result of the call. */
status_t Connection::SetCurrentKitten(Kitten::ID id) {
	cout << "Connection::SetCurrentKitten(" << id << ")" << endl;

	Kitten *kitten = Kitten::Lookup(id);
	if(!kitten) {
		return STATUS_NOT_FOUND;
	} else if(!kitten->IsOwner(this)) {
		return STATUS_PERM_DENIED;
	}

	m_current_kitten = kitten;
	return STATUS_SUCCESS;
}

/** Get the name of the current kitten.
 * @param name		Where to store name of kitten.
 * @return		Result of the call. */
status_t Connection::GetName(string &name) {
	cout << "Connection::GetName()" << endl;

	if(!m_current_kitten) {
		return STATUS_NOT_FOUND;
	}
	name = m_current_kitten->GetName();
	return STATUS_SUCCESS;
}

/** Get the colour of the current kitten.
 * @param colour	Where to store colour of kitten.
 * @return		Result of the call. */
status_t Connection::GetColour(Kitten::Colour &colour) {
	cout << "Connection::GetColour()" << endl;

	if(!m_current_kitten) {
		return STATUS_NOT_FOUND;
	}
	colour = m_current_kitten->GetColour();
	return STATUS_SUCCESS;
}

/** Stroke the current kitten.
 * @param duration	How long to stroke the kitten for (in seconds).
 * @return		Result of the call. */
status_t Connection::Stroke(int32_t duration) {
	cout << "Connection::Stroke(" << duration << ")" << endl;

	if(!m_current_kitten) {
		return STATUS_NOT_FOUND;
	}
	m_current_kitten->Stroke(duration);
	return STATUS_SUCCESS;
}
