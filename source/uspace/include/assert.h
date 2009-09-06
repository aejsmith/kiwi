/* Assertion function
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

#undef assert
#ifdef NDEBUG
# define assert(ignore)	((void)0)
#else
# define assert(cond)	if(!(cond)) { __assert_fail(#cond, __FILE__, __LINE__, __func__); }
#endif

#ifndef __ASSERT_H
#define __ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

static inline void __assert_fail(const char *cond, const char *file, unsigned int line, const char *func) {
	if(func == (__const char *)0) {
		printf("Assert failed: '%s' (%s:%d)\n", cond, file, line);
	} else {
		printf("Assert failed: '%s' (%s:%d - %s)\n", cond, file, line, func);
	}
	abort();
}

#ifdef __cplusplus
}
#endif

#endif /* __ASSERT_H */
