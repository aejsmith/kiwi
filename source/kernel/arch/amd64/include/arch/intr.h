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
 * @brief		AMD64 interrupt functions/definitions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

/** Interrupt frame structure offsets. */
#define IFRAME_OFF_R15		0x0
#define IFRAME_OFF_R14		0x8
#define IFRAME_OFF_R13		0x10
#define IFRAME_OFF_R12		0x18
#define IFRAME_OFF_R11		0x20
#define IFRAME_OFF_R10		0x28
#define IFRAME_OFF_R9		0x30
#define IFRAME_OFF_R8		0x38
#define IFRAME_OFF_BP		0x40
#define IFRAME_OFF_SI		0x48
#define IFRAME_OFF_DI		0x50
#define IFRAME_OFF_DX		0x58
#define IFRAME_OFF_CX		0x60
#define IFRAME_OFF_BX		0x68
#define IFRAME_OFF_AX		0x70
#define IFRAME_OFF_NUM		0x78
#define IFRAME_OFF_ERROR	0x80
#define IFRAME_OFF_IP		0x88
#define IFRAME_OFF_CS		0x90
#define IFRAME_OFF_FLAGS	0x98
#define IFRAME_OFF_SP		0xa0
#define IFRAME_OFF_SS		0xa8

#ifndef __ASM__

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
	unative_t r15;			/**< R15. */
	unative_t r14;			/**< R14. */
	unative_t r13;			/**< R13. */
	unative_t r12;			/**< R12. */
	unative_t r11;			/**< R11. */
	unative_t r10;			/**< R10. */
	unative_t r9;			/**< R9. */
	unative_t r8;			/**< R8. */
	unative_t bp;			/**< RBP. */
	unative_t si;			/**< RSI. */
	unative_t di;			/**< RDI. */
	unative_t dx;			/**< RDX. */
	unative_t cx;			/**< RCX. */
	unative_t bx;			/**< RBX. */
	unative_t ax;			/**< RAX. */
	unative_t num;			/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< IP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< FLAGS. */
	unative_t sp;			/**< SP. */
	unative_t ss;			/**< SS. */
} __packed intr_frame_t;

#endif /* __ASM__ */
#endif /* __ARCH_INTR_H */
