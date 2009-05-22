/* Kiwi AMD64 CPU context structures
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
 * @brief		AMD64 CPU context structures.
 */

#ifndef __ARCH_CONTEXT_H
#define __ARCH_CONTEXT_H

/** Context structure offsets. */
#define CONTEXT_OFF_IP		0	/**< RIP offset. */
#define CONTEXT_OFF_SP		8	/**< RSP offset. */
#define CONTEXT_OFF_BP		16	/**< RBP offset. */
#define CONTEXT_OFF_BX		24	/**< RBX offset. */
#define CONTEXT_OFF_R12		32	/**< R12 offset. */
#define CONTEXT_OFF_R13		40	/**< R13 offset. */
#define CONTEXT_OFF_R14		48	/**< R14 offset. */
#define CONTEXT_OFF_R15		56	/**< R15 offset. */

#ifndef __ASM__

#include <types.h>

/** Structure containing a CPU context. */
typedef struct context {
	ptr_t ip;			/**< EIP/RIP - Instruction pointer. */
	ptr_t sp;			/**< ESP/RSP - Stack pointer. */

	unative_t bp;			/**< EBP/RBP. */
	unative_t bx;			/**< EBX/RBX. */
	unative_t r12;			/**< R12. */
	unative_t r13;			/**< R13. */
	unative_t r14;			/**< R14. */
	unative_t r15;			/**< R15. */
} context_t;

#endif /* __ASM__ */
#endif /* __ARCH_CONTEXT_H */
