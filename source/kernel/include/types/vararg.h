/* Kiwi variadic argument definitions
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
 * @brief		Variadic argument definitions.
 */

#ifndef __TYPES_VARARG_H
#define __TYPES_VARARG_H

/** Variadic argument list. */
typedef __builtin_va_list va_list;

/** Initialises a va_list. */
#define va_start(a,b)		__builtin_va_start(a,b)

/** Ends use of a va_list. */
#define va_end(a)		__builtin_va_end(a)

/** Gets an argument from a va_list. */
#define va_arg(a, b)		__builtin_va_arg(a, b)

#endif /* __TYPES_VARARG_H */
