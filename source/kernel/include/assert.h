/*
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

#ifndef __ASSERT_H
#define __ASSERT_H

#include <compiler.h>
#include <fatal.h>

#if CONFIG_DEBUG
# define assert(cond)	if(unlikely(!(cond))) { fatal("Assertion failure: %s\nat %s:%d", #cond, __FILE__, __LINE__); }
#else
# define assert(cond)	((void)0)
#endif

#endif /* __ASSERT_H */
