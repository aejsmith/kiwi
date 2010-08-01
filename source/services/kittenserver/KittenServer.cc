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

#include "Connection.h"
#include "KittenServer.h"

/** Handle a connection to the kitten server.
 * @param handle	Handle to the connection. */
void KittenServer::handleConnection(handle_t handle) {
	new Connection(handle);
}

/** Main function for the kitten server.
 * @param argc		Argument count.
 * @param argv		Argument array. */
int main(int argc, char **argv) {
	KittenServer server;
	server.run();
	return 0;
}
