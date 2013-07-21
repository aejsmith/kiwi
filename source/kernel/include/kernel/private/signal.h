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
 * @brief		Internal signal functions/definitions.
 */

#ifndef __KERNEL_PRIVATE_SIGNAL_H
#define __KERNEL_PRIVATE_SIGNAL_H

#include <kernel/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void kern_signal_return(void);

#ifndef KERNEL
extern status_t _kern_signal_action(int num, const sigaction_t *newp, sigaction_t *oldp);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PRIVATE_SIGNAL_H */
