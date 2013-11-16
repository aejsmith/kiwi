/*
 * Copyright (C) 2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Security tokens.
 */

#ifndef __SECURITY_TOKEN_H
#define __SECURITY_TOKEN_H

#include <kernel/security.h>

#include <object.h>

/** Structure of a security token. */
typedef struct token {
	security_context_t ctx;		/**< Context that the token holds. */
	refcount_t count;		/**< Reference count for the token. */

	/**
	 * Whether the token needs to be copied when inheriting.
	 *
	 * This is set when the effective and inheritable privilege sets differ,
	 * as in this case the token must be copied when inheriting. If they
	 * are the same, the same token can be shared.
	 */
	bool copy_on_inherit;
} token_t;

extern token_t *system_token;
extern object_type_t token_object_type;

extern void token_retain(token_t *token);
extern void token_release(token_t *token);
extern token_t *token_inherit(token_t *source);

extern token_t *token_current(void);

extern void token_init(void);

#endif /* __SECURITY_TOKEN_H */
