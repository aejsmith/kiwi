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
 * @brief		Security context functions.
 */

#ifndef __SECURITY_CONTEXT_H
#define __SECURITY_CONTEXT_H

#include <public/security.h>

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
