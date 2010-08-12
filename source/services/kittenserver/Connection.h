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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "Kitten.h"
#include "ClientConnection.h"

/** Class representing a client of the kitten server. */
class Connection : public org::kiwi::KittenServer::ClientConnection {
public:
	Connection(handle_t handle);
private:
	status_t CreateKitten(const std::string &name, Kitten::Colour colour, Kitten::ID &id);
	status_t SetCurrentKitten(Kitten::ID id);
	status_t GetName(std::string &name);
	status_t GetColour(Kitten::Colour &colour);
	status_t Stroke(int32_t duration);

	Kitten *m_current_kitten;	/**< Current kitten. */
};

#endif /* __CONNECTION_H */
