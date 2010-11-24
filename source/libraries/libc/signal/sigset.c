/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Signal set manipulation functions.
 */

#include <errno.h>
#include <signal.h>

/** Add a signal to a signal set.
 * @param set		Set to add to.
 * @param num		Signal to add.
 * @return		0 on success, -1 on failure. */
int sigaddset(sigset_t *set, int num) {
	if(num < 1 || num >= NSIG) {
		errno = EINVAL;
		return -1;
	}

	*set |= (1<<num);
	return 0;
}

/** Remove a signal from a signal set.
 * @param set		Set to remove from.
 * @param num		Signal to remove.
 * @return		0 on success, -1 on failure. */
int sigdelset(sigset_t *set, int num) {
	if(num < 1 || num >= NSIG) {
		errno = EINVAL;
		return -1;
	}

	*set &= ~(1<<num);
	return 0;
}

/** Clear all signals in a signal set.
 * @param set		Set to clear.
 * @return		Always 0. */
int sigemptyset(sigset_t *set) {
	*set = 0;
	return 0;
}

/** Set all signals in a signal set.
 * @param set		Set to fill.
 * @return		Always 0. */
int sigfillset(sigset_t *set) {
	*set = -1;
	return 0;
}

/** Check if a signal is included in a set.
 * @param set		Set to check.
 * @param num		Signal number to check for.
 * @return		1 if member, 0 if not, -1 if signal number is invalid. */
int sigismember(const sigset_t *set, int num) {
	if(num < 1 || num >= NSIG) {
		errno = EINVAL;
		return -1;
	}

	return (*set & (1<<num)) ? 1 : 0;
}
