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
 * @brief		Security context functions.
 */

#ifndef __SECURITY_CONTEXT_H
#define __SECURITY_CONTEXT_H

#include <kernel/security.h>

struct process;

extern security_context_t init_security_context;

extern void security_context_canonicalise(security_context_t *context);
extern status_t security_context_validate(const security_context_t *setter,
                                          const security_context_t *prev,
                                          security_context_t *context);

extern security_context_t *security_context_get(struct process *process);
extern void security_context_release(struct process *process);

extern user_id_t security_current_uid(void);
extern group_id_t security_current_gid(void);

extern void security_init(void);

#endif /* __SECURITY_CONTEXT_H */
