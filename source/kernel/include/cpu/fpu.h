/* Kiwi FPU context functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		FPU context functions.
 */

#ifndef __CPU_FPU_H
#define __CPU_FPU_H

#include <arch/fpu.h>

extern void fpu_context_save(fpu_context_t *ctx);
extern void fpu_context_restore(fpu_context_t *ctx);
extern void fpu_context_destroy(fpu_context_t *ctx);

extern void fpu_request(void);

extern bool fpu_state(void);
extern void fpu_enable(void);
extern void fpu_disable(void);
extern void fpu_init(void);

#endif /* __CPU_FPU_H */
