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
 * @brief		Capability functions.
 */

#ifndef __SECURITY_CAP_H
#define __SECURITY_CAP_H

#include <proc/process.h>

/** Check whether a process has a capability.
 * @param process	Process to check (NULL for current process).
 * @param cap		Capability to check for.
 * @return		Whether the process has the capability. */
static inline bool cap_check(process_t *process, int cap) {
	if(!process) {
		process = curr_proc;
	}
	return security_context_has_cap(&process->security, cap);
}

#endif /* __SECURITY_CAP_H */
