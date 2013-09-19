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
 * @brief		Privilege checking functions.
 */

#include <proc/process.h>
#include <proc/thread.h>

#include <security/security.h>
#include <security/token.h>

/** Get the currently active security token.
 * @return		Currently active security token, will be referenced. */
token_t *security_current_token(void) {
	token_t *token;

	mutex_lock(&curr_proc->lock);

	token = (curr_thread->token) ? curr_thread->token : curr_proc->token;
	token_retain(token);

	mutex_unlock(&curr_proc->lock);
	return token;
}

/** Get the current user ID.
 * @return		Current user ID. */
user_id_t security_current_uid(void) {
	token_t *token;
	user_id_t ret;

	mutex_lock(&curr_proc->lock);

	token = (curr_thread->token) ? curr_thread->token : curr_proc->token;
	ret = token->ctx.uid;

	mutex_unlock(&curr_proc->lock);
	return ret;
}

/** Get the current group ID.
 * @return		Current group ID. */
group_id_t security_current_gid(void) {
	token_t *token;
	group_id_t ret;

	mutex_lock(&curr_proc->lock);

	token = (curr_thread->token) ? curr_thread->token : curr_proc->token;
	ret = token->ctx.gid;

	mutex_unlock(&curr_proc->lock);
	return ret;
}

/** Check whether the current thread has a privilege.
 * @param priv		Privilege to check for.
 * @return		Whether the current thread has the privilege. */
bool security_check_priv(unsigned priv) {
	token_t *token;
	bool ret;

	mutex_lock(&curr_proc->lock);

	token = (curr_thread->token) ? curr_thread->token : curr_proc->token;
	ret = security_context_has_priv(&token->ctx, priv);

	mutex_unlock(&curr_proc->lock);
	return ret;
}
