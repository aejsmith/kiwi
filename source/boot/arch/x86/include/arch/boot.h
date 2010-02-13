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
 * @brief		x86 bootloader definitions.
 */

#ifndef __ARCH_BOOT_H
#define __ARCH_BOOT_H

#include <boot/vfs.h>
#include <kargs.h>

extern void idt_init(void);
extern void arch_load_kernel(vfs_node_t *file);
extern void arch_enter_kernel(void) __noreturn;
extern void arch_early_init(void);

#endif /* __ARCH_BOOT_H */