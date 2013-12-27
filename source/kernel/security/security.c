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

/**
 * Get the currently active security token.
 *
 * Gets the current thread's active security token. A thread's active security
 * token remains constant for the entire time that the thread is in the kernel,
 * i.e. if another thread changes the process-wide security context, the change
 * will not take effect until the current thread returns to userspace. The
 * returned token does not have an extra reference added, it remains valid
 * until the calling thread exits the kernel. If it needs to be kept after this,
 * the token must be explicitly referenced.
 *
 * @return		Currently active security token.
 */
token_t *security_current_token(void) {
	token_t *token;

	if(curr_thread->active_token) {
		token = curr_thread->active_token;
	} else {
		mutex_lock(&curr_proc->lock);

		token = (curr_thread->token) ? curr_thread->token
			: curr_proc->token;
		token_retain(token);

		mutex_unlock(&curr_proc->lock);

		/* Save the active token to be returned by subsequent calls.
		 * An alternative to doing this would be to always save the
		 * token in thread_at_kernel_entry(), however doing so would be
		 * inefficient: it would require a process lock on every kernel
		 * entry, and for a lot of kernel entries the security token
		 * is not required. By saving the token the first time we call
		 * this function, we still achieve the desired behaviour of not
		 * having the thread's identity change while doing security
		 * checks. */
		curr_thread->active_token = token;
	}

	return token;
}
