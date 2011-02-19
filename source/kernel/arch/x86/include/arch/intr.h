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
 * @brief		x86 interrupt functions/definitions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

/** Definitions for hardware exception numbers. */
#define X86_EXCEPT_DE		0	/**< Divide Error. */
#define X86_EXCEPT_DB		1	/**< Debug. */
#define X86_EXCEPT_NMI		2	/**< Non-Maskable Interrupt. */
#define X86_EXCEPT_BP		3	/**< Breakpoint. */
#define X86_EXCEPT_OF		4	/**< Overflow. */
#define X86_EXCEPT_BR		5	/**< BOUND Range Exceeded. */
#define X86_EXCEPT_UD		6	/**< Invalid Opcode. */
#define X86_EXCEPT_NM		7	/**< Device Not Available. */
#define X86_EXCEPT_DF		8	/**< Double Fault. */
#define X86_EXCEPT_TS		10	/**< Invalid TSS. */
#define X86_EXCEPT_NP		11	/**< Segment Not Present. */
#define X86_EXCEPT_SS		12	/**< Stack Fault. */
#define X86_EXCEPT_GP		13	/**< General Protection Fault. */
#define X86_EXCEPT_PF		14	/**< Page Fault. */
#define X86_EXCEPT_MF		16	/**< x87 FPU Floating-Point Error. */
#define X86_EXCEPT_AC		17	/**< Alignment Check. */
#define X86_EXCEPT_MC		18	/**< Machine Check. */
#define X86_EXCEPT_XM		19	/**< SIMD Floating-Point. */

/** Various definitions. */
#define IRQ_COUNT		16	/**< Total number of IRQs. */
#define IRQ_BASE		32	/**< IRQ number base. */

/** System call interrupt number. */
#ifdef __i386__
# define SYSCALL_INT_NO		0x80
#endif

/** Interrupt frame structure offsets. */
#ifdef __x86_64__
# define IFRAME_OFF_R15		0x0
# define IFRAME_OFF_R14		0x8
# define IFRAME_OFF_R13		0x10
# define IFRAME_OFF_R12		0x18
# define IFRAME_OFF_R11		0x20
# define IFRAME_OFF_R10		0x28
# define IFRAME_OFF_R9		0x30
# define IFRAME_OFF_R8		0x38
# define IFRAME_OFF_BP		0x40
# define IFRAME_OFF_SI		0x48
# define IFRAME_OFF_DI		0x50
# define IFRAME_OFF_DX		0x58
# define IFRAME_OFF_CX		0x60
# define IFRAME_OFF_BX		0x68
# define IFRAME_OFF_AX		0x70
# define IFRAME_OFF_INT_NO	0x78
# define IFRAME_OFF_ERR_CODE	0x80
# define IFRAME_OFF_IP		0x88
# define IFRAME_OFF_CS		0x90
# define IFRAME_OFF_FLAGS	0x98
# define IFRAME_OFF_SP		0xa0
# define IFRAME_OFF_SS		0xa8
#else
# define IFRAME_OFF_GS		0x0
# define IFRAME_OFF_FS		0x4
# define IFRAME_OFF_ES		0x8
# define IFRAME_OFF_DS		0xc
# define IFRAME_OFF_DI		0x10
# define IFRAME_OFF_SI		0x14
# define IFRAME_OFF_BP		0x18
# define IFRAME_OFF_KSP		0x1c
# define IFRAME_OFF_BX		0x20
# define IFRAME_OFF_DX		0x24
# define IFRAME_OFF_CX		0x28
# define IFRAME_OFF_AX		0x2c
# define IFRAME_OFF_INT_NO	0x30
# define IFRAME_OFF_ERR_CODE	0x34
# define IFRAME_OFF_IP		0x38
# define IFRAME_OFF_CS		0x3c
# define IFRAME_OFF_FLAGS	0x40
# define IFRAME_OFF_SP		0x44
# define IFRAME_OFF_SS		0x48
#endif

#ifndef __ASM__

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
#ifdef __x86_64__
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
#else
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t es;			/**< ES. */
	unative_t ds;			/**< DS. */
	unative_t di;			/**< EDI. */
	unative_t si;			/**< ESI. */
	unative_t bp;			/**< EBP. */
	unative_t ksp;			/**< ESP (kernel). */
	unative_t bx;			/**< EBX. */
	unative_t dx;			/**< EDX. */
	unative_t cx;			/**< ECX. */
	unative_t ax;			/**< EAX. */
#endif
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< IP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< FLAGS. */
	unative_t sp;			/**< SP. */
	unative_t ss;			/**< SS. */
} __packed intr_frame_t;

/** Enable interrupts.
 * @return		Previous interrupt state. */
static inline bool intr_enable(void) {
	unative_t flags;

	__asm__ volatile("pushf; sti; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Disable interrupts.
 * @return		Previous interrupt state. */
static inline bool intr_disable(void) {
	unative_t flags;

	__asm__ volatile("pushf; cli; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Restore saved interrupt state.
 * @param state		State to restore. */
static inline void intr_restore(bool state) {
	if(state) {
		__asm__ volatile("sti");
	} else {
		__asm__ volatile("cli");
	}
}

/** Get interrupt state.
 * @return		Current interrupt state. */
static inline bool intr_state(void) {
	unative_t flags;

	__asm__ volatile("pushf; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Interrupt handler function type. */
typedef void (*intr_handler_t)(unative_t num, intr_frame_t *frame);

extern void intr_register(unative_t num, intr_handler_t handler);
extern void intr_remove(unative_t num);
extern void intr_init(void);

#endif /* __ASM__ */
#endif /* __ARCH_INTR_H */
