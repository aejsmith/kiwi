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
 * @brief		x86 assembly code definitions.
 */

#ifndef __ARCH_ASM_H
#define __ARCH_ASM_H

#ifndef __ASM__
# error "What are you doing?"
#endif

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(name)		\
	.global name; \
	.type name, @function; \
	name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(name)	\
	.type name, @function; \
	name:

/** Macro to define the end of a function. */
#define FUNCTION_END(name)		\
	.size name, . - name

/** Macro to define a global symbol. */
#define SYMBOL(name)			\
	.global name; \
	name:

#endif /* __ARCH_ASM_H */
