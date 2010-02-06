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
 * @brief		Variable argument functions.
 */

#ifndef __STDARG_H
#define __STDARG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef __builtin_va_list va_list;

#define va_start(a,b)	__builtin_va_start(a,b)
#define va_end(a)	__builtin_va_end(a)
#define va_arg(a,b)	__builtin_va_arg(a,b)

#ifdef __cplusplus
}
#endif

#endif /* __STDARG_H */
