/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Signal wrapper functions.
 */

#include <kernel/signal.h>
#include <kernel/status.h>

#include <string.h>

#include "libkernel.h"

extern status_t _kern_signal_action(int num, const sigaction_t *newp, sigaction_t *oldp);
extern void kern_signal_return(void);

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

	ret = _kern_signal_action(num, (newp) ? &new : NULL, oldp);
	if(ret == STATUS_SUCCESS && oldp) {
		memcpy(oldp, &old, sizeof(old) - sizeof(old.sa_restorer));
	}

	return ret;
}
