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
 * @brief		Security helper functions.
 */

#ifndef __SECURITY_SECURITY_H
#define __SECURITY_SECURITY_H

#include <security/token.h>

/** Get the current security context.
 * @return		Pointer to current security context. */
static inline security_context_t *security_current_context(void) {
	token_t *token = token_current();

	return &token->ctx;
}

/** Get the current user ID.
 * @return		Current user ID. */
static inline user_id_t security_current_uid(void) {
	token_t *token = token_current();

	return token->ctx.uid;
}

/** Get the current group ID.
 * @return		Current group ID. */
static inline group_id_t security_current_gid(void) {
	token_t *token = token_current();

	return token->ctx.gid;
}

/** Check whether the current thread has a privilege.
 * @param priv		Privilege to check for.
 * @return		Whether the current thread has the privilege. */
static inline bool security_check_priv(unsigned priv) {
	token_t *token = token_current();

	return security_context_has_priv(&token->ctx, priv);
}

#endif /* __SECURITY_SECURITY_H */
