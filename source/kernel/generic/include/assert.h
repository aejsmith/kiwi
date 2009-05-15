/* Kiwi assertion function
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Assertion function.
 */

#include <fatal.h>

/* Assert should be redefined according to NDEBUG each time the header is
 * included. */
#undef assert
#ifdef NDEBUG
# define assert(cond)		((void)0)
#else
# define assert(cond)		if(!(cond)) { fatal("Assertion failure: %s\nat %s:%d", #cond, __FILE__, __LINE__); }
#endif
