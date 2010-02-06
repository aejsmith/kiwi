/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 interrupt functions/definitions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

#include <types.h>

/** Definitions for fault numbers. */
#define FAULT_DIVIDE		0	/**< Divide Error. */
#define FAULT_DEBUG		1	/**< Debug. */
#define FAULT_NMI		2	/**< Non-Maskable Interrupt. */
#define FAULT_BREAKPOINT	3	/**< Breakpoint. */
#define FAULT_OVERFLOW		4	/**< Overflow. */
#define FAULT_BOUND		5	/**< BOUND Range Exceeded. */
#define FAULT_INVALID_OPCODE	6	/**< Invalid Opcode. */
#define FAULT_DEVICE_NOT_AVAIL	7	/**< Device Not Available. */
#define FAULT_DOUBLE		8	/**< Double Fault. */
#define FAULT_COPROC_OVERRUN	9	/**< Coprocessor Segment Overrun. */
#define FAULT_INVALID_TSS	10	/**< Invalid TSS. */
#define FAULT_SEGMENT_NOT_PRES	11	/**< Segment Not Present. */
#define FAULT_STACK		12	/**< Stack Fault. */
#define FAULT_GP		13	/**< General Protection Fault. */
#define FAULT_PAGE		14	/**< Page Fault. */
#define FAULT_FPU		16	/**< x87 FPU Floating-Point Error. */
#define FAULT_ALIGNMENT		17	/**< Alignment Check. */
#define FAULT_MCE		18	/**< Machine Check. */
#define FAULT_SIMD		19	/**< SIMD Floating-Point. */

/** Various definitions. */
#define IRQ_COUNT		16	/**< Total number of IRQs. */
#define IRQ_BASE		32	/**< IRQ number base. */

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
#ifdef __x86_64__
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
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
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< RIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< RFLAGS. */
	unative_t sp;			/**< RSP. */
	unative_t ss;			/**< SS. */
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
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< EIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< EFLAGS. */
	unative_t sp;			/**< ESP. */
	unative_t ss;			/**< SS. */
#endif
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

/** Interrupt handler routine type.
 * @return		Whether to reschedule. */
typedef bool (*intr_handler_t)(unative_t num, intr_frame_t *frame);

extern void intr_register(unative_t num, intr_handler_t handler);
extern void intr_remove(unative_t num);
extern void intr_init(void);

#endif /* __ARCH_INTR_H */
