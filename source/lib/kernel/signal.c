/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Signal functions.
 */

#include <kernel/private/signal.h>
#include <kernel/status.h>

#include <string.h>

#include "libkernel.h"

/** Examine and modify the action for a signal.
 * @param num		Signal number to modify.
 * @param newp		If not NULL, pointer to a new action to set.
 * @param oldp		If not NULL, where to store previous action.
 * @return		Status code describing result of the operation. */
__export status_t kern_signal_action(int num, const sigaction_t *newp, sigaction_t *oldp) {
	sigaction_t new, old;
	status_t ret;

	if(newp) {
		memcpy(&new, newp, sizeof(new) - sizeof(new.sa_restorer));
		new.sa_restorer = kern_signal_return;
	}

	ret = _kern_signal_action(num, (newp) ? &new : NULL, &old);
	if(ret == STATUS_SUCCESS && oldp)
		memcpy(oldp, &old, sizeof(old) - sizeof(old.sa_restorer));

	return ret;
}
