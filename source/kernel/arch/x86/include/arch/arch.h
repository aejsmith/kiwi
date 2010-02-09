/*
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
 * @brief		x86 architecture header.
 */

#ifndef __ARCH_ARCH_H
#define __ARCH_ARCH_H

#include <kargs.h>

extern void arch_premm_init(kernel_args_t *args);
extern void arch_postmm_init(kernel_args_t *args);
extern void arch_final_init(void);
extern void arch_ap_init(void);
extern void arch_reboot(void);

#endif /* __ARCH_ARCH_H */
