/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		x86 CPU context structures.
 */

#ifndef __ARCH_CONTEXT_H
#define __ARCH_CONTEXT_H

/** Context structure offsets. */
#ifdef __x86_64__
# define CONTEXT_OFF_IP		0	/**< RIP offset. */
# define CONTEXT_OFF_SP		8	/**< RSP offset. */
# define CONTEXT_OFF_BP		16	/**< RBP offset. */
# define CONTEXT_OFF_BX		24	/**< RBX offset. */
# define CONTEXT_OFF_R12	32	/**< R12 offset. */
# define CONTEXT_OFF_R13	40	/**< R13 offset. */
# define CONTEXT_OFF_R14	48	/**< R14 offset. */
# define CONTEXT_OFF_R15	56	/**< R15 offset. */
#else
# define CONTEXT_OFF_IP		0	/**< EIP offset. */
# define CONTEXT_OFF_SP		4	/**< ESP offset. */
# define CONTEXT_OFF_BP		8	/**< EBP offset. */
# define CONTEXT_OFF_BX		12	/**< EBX offset. */
# define CONTEXT_OFF_DI		16	/**< EDI offset. */
# define CONTEXT_OFF_SI		20	/**< ESI offset. */
#endif

#ifndef __ASM__

#include <types.h>

/** Structure containing a CPU context. */
typedef struct context {
	ptr_t ip;			/**< EIP/RIP - Instruction pointer. */
	ptr_t sp;			/**< ESP/RSP - Stack pointer. */

	unative_t bp;			/**< EBP/RBP. */
	unative_t bx;			/**< EBX/RBX. */
#ifdef __x86_64__
	unative_t r12;			/**< R12. */
	unative_t r13;			/**< R13. */
	unative_t r14;			/**< R14. */
	unative_t r15;			/**< R15. */
#else
	unative_t di;			/**< EDI. */
	unative_t si;			/**< ESI. */
#endif
} context_t;

#endif /* __ASM__ */
#endif /* __ARCH_CONTEXT_H */
