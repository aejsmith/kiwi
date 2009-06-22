/* Kiwi safe user memory access functions
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
 * @brief		Safe user memory access functions.
 */

#ifndef __MM_SAFE_H
#define __MM_SAFE_H

#include <mm/flags.h>

#include <types.h>

extern int memcpy_from_user(void *dest, const void *src, size_t count);
extern int memcpy_to_user(void *dest, const void *src, size_t count);
extern int memset_user(void *dest, int val, size_t count);
extern int strlen_user(const char *str, size_t *lenp);
extern int strcpy_from_user(char *dest, const char *src);

extern int strdup_from_user(const void *src, int mmflag, char **destp);

#endif /* __MM_SAFE_H */
