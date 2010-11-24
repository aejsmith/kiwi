/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Non-local jump functions.
 */

#include <setjmp.h>
#include <signal.h>
#include <stddef.h>

/** Save current environment.
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
	if(savemask) {
		sigprocmask(SIG_BLOCK, NULL, &env->mask);
	}

	env->restore_mask = savemask;
	return setjmp(env->buf);
}

/** Restore environment.
 *
 * Restores an execution environment saved by a previous call to sigsetjmp().
 * If the original call to sigsetjmp() specified savemask as non-zero, the
 * signal mask at the time of the call will be restored.
 *
 * @param env		Buffer to restore.
 * @param val		Value that the original sigsetjmp() call should return.
 */
void siglongjmp(sigjmp_buf env, int val) {
	if(env->restore_mask) {
		sigprocmask(SIG_SETMASK, &env->mask, NULL);
	}
	longjmp(env->buf, val);
}
