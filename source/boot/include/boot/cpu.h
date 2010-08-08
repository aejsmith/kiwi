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
 * @brief		Bootloader CPU functions.
 */

#ifndef __BOOT_CPU_H
#define __BOOT_CPU_H

#include <lib/atomic.h>
#include <kargs.h>

extern kernel_args_cpu_t *boot_cpu;

extern uint32_t cpu_current_id(void);
extern void cpu_detect(void);
extern void cpu_boot_all(void (*entry)(void));

extern void cpu_init(void);
extern void cpu_ap_init(void);

#endif /* __BOOT_CPU_H */
