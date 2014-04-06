/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Alternate signal stack function.
 */

#include <signal.h>

#include "libsystem.h"

/**
 * Get and set the alternate signal stack.
 *
 * Gets and sets the alternate signal stack for the current thread. This stack
 * is used to execute signal handlers with the SA_ONSTACK flag set. The
 * alternate stack is a per-thread attribute. If fork() is called, the new
 * process' initial thread inherits the alternate stack from the thread that
 * called fork().
 *
 * @param ss		Alternate stack to set (can be NULL).
 * @param oset		Where to store previous alternate stack (can be NULL).
 *
 * @return		0 on success, -1 on failure.
 */
int sigaltstack(const stack_t *restrict ss, stack_t *restrict oldss) {
	libsystem_stub("sigaltstack", false);
	return -1;
}
