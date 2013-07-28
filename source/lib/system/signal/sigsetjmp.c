/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Non-local jump functions.
 */

#include <setjmp.h>
#include <signal.h>
#include <stddef.h>

/**
 * Save current environment.
 *
 * Saves the current execution environment to be restored by a call to
 * siglongjmp(). If specified, the current signal mask will also be saved.
 *
 * @param env		Buffer to save to.
 * @param savemask	If not 0, the current signal mask will be saved.
 *
 * @return		0 if returning from direct invocation, non-zero if
 *			returning from siglongjmp().
 */
int sigsetjmp(sigjmp_buf env, int savemask) {
	if(savemask)
		sigprocmask(SIG_BLOCK, NULL, &env->mask);

	env->restore_mask = savemask;
	return setjmp(env->buf);
}

/**
 * Restore environment.
 *
 * Restores an execution environment saved by a previous call to sigsetjmp().
 * If the original call to sigsetjmp() specified savemask as non-zero, the
 * signal mask at the time of the call will be restored.
 *
 * @param env		Buffer to restore.
 * @param val		Value that the original sigsetjmp() call should return.
 */
void siglongjmp(sigjmp_buf env, int val) {
	if(env->restore_mask)
		sigprocmask(SIG_SETMASK, &env->mask, NULL);

	longjmp(env->buf, val);
}
