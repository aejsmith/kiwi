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
 * @brief		Test application.
 */

// This -> kiwi/Utility/<something>.h
template <typename T>
static inline T p2align(T n, int alignment) {
	if(n % alignment) {
		n += alignment;
		n -= n % alignment;
	}
	return n;
}

int main(int argc, char **argv) {
	while(1);
}
