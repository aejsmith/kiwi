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
 * @brief		RPC test client.
 */

#include <iostream>
#include "ServerConnection.h"

using namespace org::kiwi::KittenServer;
using namespace std;
using namespace kiwi;

/** Current kitten ID. */
static KittenID current_kitten = 0;

/** Handle the kitten purring.
 * @param duration	How long the kitten purred for. */
static void handlePurr(int32_t duration) {
	cout << "Kitten " << current_kitten << " purred for " << duration << " seconds!" << endl;
}

/** Main function for the RPC test client.
 * @param argc		Argument count.
 * @param argv		Argument array. */
int main(int argc, char **argv) {
	Colour white = { 255, 255, 255 }, black = { 0, 0, 0 };
	ServerConnection conn;
	KittenID wid, bid;
	RPCResult ret;
	string name;

	conn.connect();
	conn.onPurr.connect(handlePurr);

	if((ret = conn.createKitten("Mittens", white, wid)) != 0) {
		cout << "Could not create white kitten: " << ret << endl;
		return 1;
	}
	current_kitten = wid;
	if((ret = conn.getName(name)) != 0) {
		cout << "Could not get kitten name: " << ret << endl;
		return 1;
	}
	cout << "Got back name: " << name << endl;
	if((ret = conn.getColour(white)) != 0) {
		cout << "Could not get kitten colour (1): " << ret << endl;
		return 1;
	}
	cout << "Got back colour: " << static_cast<int>(white.red) << ", ";
	cout << static_cast<int>(white.green) << ", " << static_cast<int>(white.blue) << endl;
	if((ret = conn.stroke(5)) != 0) {
		cout << "Could not stroke white kitten: " << ret << endl;
		return 1;
	}

	if((ret = conn.createKitten("Jeremy", black, bid)) != 0) {
		cout << "Could not create black kitten: " << ret << endl;
		return 1;
	}
	current_kitten = bid;
	if((ret = conn.stroke(8)) != 0) {
		cout << "Could not stroke black kitten: " << ret << endl;
		return 1;
	}

	if((ret = conn.setCurrentKitten(wid)) != 0) {
		cout << "Could not set white kitten: " << ret << endl;
		return 1;
	}
	current_kitten = wid;
	if((ret = conn.getColour(white)) != 0) {
		cout << "Could not get kitten colour (2): " << ret << endl;
		return 1;
	}
	cout << "Got back colour: " << static_cast<int>(white.red) << ", ";
	cout << static_cast<int>(white.green) << ", " << static_cast<int>(white.blue) << endl;

	return 0;
}
