/* Kiwi IA32 CPU context structures
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		IA32 CPU context structures.
 */

#ifndef __ARCH_CONTEXT_H
#define __ARCH_CONTEXT_H

/** Context structure offsets. */
#define CONTEXT_OFF_IP		0	/**< EIP offset. */
#define CONTEXT_OFF_SP		4	/**< ESP offset. */
#define CONTEXT_OFF_BP		8	/**< EBP offset. */
#define CONTEXT_OFF_BX		12	/**< EBX offset. */
#define CONTEXT_OFF_DI		16	/**< EDI offset. */
#define CONTEXT_OFF_SI		20	/**< ESI offset. */

#ifndef __ASM__

#include <types.h>

/** Structure containing a CPU context. */
typedef struct context {
	ptr_t ip;			/**< EIP/RIP - Instruction pointer. */
	ptr_t sp;			/**< ESP/RSP - Stack pointer. */

	unative_t bp;			/**< EBP/RBP. */
	unative_t bx;			/**< EBX/RBX. */
	unative_t di;			/**< EDI. */
	unative_t si;			/**< ESI. */
} context_t;

#endif /* __ASM__ */
#endif /* __ARCH_CONTEXT_H */
