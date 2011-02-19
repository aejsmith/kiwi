/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#ifndef __PROC_SIGNAL_H
#define __PROC_SIGNAL_H

#include <kernel/signal.h>

struct thread;

extern status_t signal_arch_setup_frame(sigaction_t *action, siginfo_t *info, sigset_t mask);
extern status_t signal_arch_restore_frame(sigset_t *maskp);

extern void signal_send(struct thread *thread, int num, siginfo_t *info, bool force);
extern void signal_handle_pending(void);

#endif /* __PROC_SIGNAL_H */
