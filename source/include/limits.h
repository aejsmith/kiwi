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
 * @brief		Implementation-defined constants.
 */

#ifndef __LIMITS_H
#define __LIMITS_H

/** Various system limitations. */
#define PATH_MAX		4096	/**< Maximum length of a path string. */
#define SYMLINK_MAX		4096	/**< Maximum length of a symbolic link destination. */

#ifndef _GCC_LIMITS_H_
# include_next <limits.h>
#endif

#endif /* __LIMITS_H */
