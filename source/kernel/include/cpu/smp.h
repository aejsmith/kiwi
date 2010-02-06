/* Kiwi Symmetric Multiprocessing (SMP) code
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
 * @brief		Symmetric Multiprocessing (SMP) code.
 */

#ifndef __CPU_SMP_H
#define __CPU_SMP_H

extern atomic_t ap_boot_wait;

extern void smp_boot_cpus(void);
extern void smp_detect_cpus(void);

#endif /* __CPU_SMP_H */
