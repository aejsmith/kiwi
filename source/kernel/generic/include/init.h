/* Kiwi kernel initialization functions
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
 * @brief		Kernel initialization functions.
 */

#ifndef __INIT_H
#define __INIT_H

#include <compiler.h>

/** Type of an initcall function. */
typedef void (*initcall_t)(void);

/** Macro to declare an initialization function.
 * @note		Initcalls are called in the initialization thread,
 *			after other CPUs have been booted. They are called in
 *			the order that they are in the initcall section. */
#define INITCALL(func)	\
	static ptr_t __initcall_##func __section(".init.initcalls") __used = (ptr_t)func

#endif /* __INIT_H */
