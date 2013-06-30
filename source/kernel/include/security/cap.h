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
