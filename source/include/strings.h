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
 * @brief		String functions.
 */

#ifndef __STRINGS_H
#define __STRINGS_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Get size_t and NULL from stddef.h. I would love to know why stdc defines
 * size_t and NULL in 3 headers. */
#define __need_size_t
#include <stddef.h>

#define bzero(b, len)		(memset((b), '\0', (len)), (void)0)
#define bcopy(b1, b2, len)	(memmove((b2), (b1), (len)), (void)0)
#define bcmp(b1, b2, len)	memcmp((b1), (b2), (size_t)(len))
#define index(a, b)		strchr((a),(b))
#define rindex(a, b)		strrchr((a),(b))

#ifdef __cplusplus
}
#endif

#endif /* __STRINGS_H */
