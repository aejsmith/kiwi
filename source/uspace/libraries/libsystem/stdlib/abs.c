/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Absolute value functions.
 */

#include <stdlib.h>

/** Compute the absolute value of an integer.
 *
 * Computes the absolute value of the given integer.
 *
 * @param j		Integer to compute from.
 */
int abs(int j) {
	return (j < 0) ? -j : j;
}

/** Compute the absolute value of a long.
 *
 * Computes the absolute value of the given long integer.
 *
 * @param j		Long integer to compute from.
 */
long labs(long j) {
	return (j < 0) ? -j : j;
}

/** Compute the absolute value of a long long.
 *
 * Computes the absolute value of the given long long integer.
 *
 * @param j		Long long integer to compute from.
 */
long long llabs(long long j) {
	return (j < 0) ? -j : j;
}
