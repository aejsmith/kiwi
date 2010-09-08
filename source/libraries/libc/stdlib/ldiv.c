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
 * @brief		Integer division function.
 */

#include <stdlib.h>

/** Find the quotient and remainder of an integer division.
 * @param num		Numerator.
 * @param denom		Denominator.
 * @return		Structure containing result of division. */
div_t div(int num, int denom) {
	div_t r;

	r.quot = num / denom;
	r.rem = num % denom;
	if(num >= 0 && r.rem < 0) {
		r.quot++;
		r.rem -= denom;
	} else if(num < 0 && r.rem > 0) {
		r.quot--;
		r.rem += denom;
	}

	return r;
}

/** Find the quotient and remainder of an integer division.
 * @param num		Numerator.
 * @param denom		Denominator.
 * @return		Structure containing result of division. */
ldiv_t ldiv(long num, long denom) {
	ldiv_t r;

	r.quot = num / denom;
	r.rem = num % denom;
	if(num >= 0 && r.rem < 0) {
		r.quot++;
		r.rem -= denom;
	} else if(num < 0 && r.rem > 0) {
		r.quot--;
		r.rem += denom;
	}

	return r;
}
