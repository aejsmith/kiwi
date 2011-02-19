/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Safe user memory access functions.
 */

#ifndef __MM_SAFE_H
#define __MM_SAFE_H

#include <mm/flags.h>

#include <types.h>

extern status_t validate_user_address(void *dest, size_t size);

extern status_t memcpy_from_user(void *dest, const void *src, size_t count);
extern status_t memcpy_to_user(void *dest, const void *src, size_t count);
extern status_t memset_user(void *dest, int val, size_t count);
extern status_t strlen_user(const char *str, size_t *lenp);

extern status_t strdup_from_user(const void *src, char **destp);
extern status_t strndup_from_user(const void *src, size_t max, char **destp);
extern status_t arrcpy_from_user(const char *const src[], char ***arrayp);

#endif /* __MM_SAFE_H */
