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
 * @brief		POSIX alarm function.
 */

#include <unistd.h>
#include "../libc.h"

/** Arrange for a SIGALRM signal to be delivered after a certain time.
 * @param seconds	Seconds to wait for.
 * @return		Seconds until previously scheduled alarm was to be
 *			delivered, or 0 if no previous alarm. */
unsigned int alarm(unsigned int seconds) {
	libc_stub("alarm", false);
	return 0;
}
