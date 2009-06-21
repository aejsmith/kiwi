/* Kiwi x86 kernel debugger functions
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
 * @brief		x86 kernel debugger functions.
 */

#include <arch/memmap.h>
#include <arch/page.h>
#include <arch/x86/sysreg.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <symbol.h>
#include <kdbg.h>

extern bool kdbg_int1_handler(unative_t num, intr_frame_t *frame);

/** Structure containing a stack frame. */
typedef struct stack_frame {
	struct stack_frame *next;	/**< Pointer to next stack frame. */
	ptr_t addr;			/**< Function return address. */
} stack_frame_t;

/** Breakpoint tracking structures. */
static struct {
	bool used;
	bool enabled;
	ptr_t addr;
} kdbg_breakpoints[4];

/** Set breakpoint settings in the debug registers. */
static inline void kdbg_setup_dreg(void) {
	unative_t dr7 = 0;
	size_t i;

	sysreg_dr0_write(kdbg_breakpoints[0].addr);
	sysreg_dr1_write(kdbg_breakpoints[1].addr);
	sysreg_dr2_write(kdbg_breakpoints[2].addr);
	sysreg_dr3_write(kdbg_breakpoints[3].addr);

	for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
		if(kdbg_breakpoints[i].enabled) {
			dr7 |= (1<<i);
		}
	}

	sysreg_dr7_write(dr7);
}

/** Debug exception handler.
 *
 * Handles a debug exception by pulling the entry reason out of EAX and
 * calling KDBG.
 *
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 *
 * @return		True if handled, false if not.
 */
bool kdbg_int1_handler(unative_t num, intr_frame_t *frame) {
	static bool bp_resume = false;
	int reason = KDBG_ENTRY_USER;
	unative_t dr6;
	size_t i = 0;

	/* Work out the reason. */
	dr6 = sysreg_dr6_read();
	if(!(dr6 & (SYSREG_DR6_B0 | SYSREG_DR6_B1 | SYSREG_DR6_B2 |
	            SYSREG_DR6_B3 | SYSREG_DR6_BD | SYSREG_DR6_BS |
	            SYSREG_DR6_BT))) {
		/* No bits set, assume this came from from kdbg_enter(), in
		 * which case the reason will be in EAX. */
		reason = (unative_t)frame->ax;
	} else {
		if(dr6 & SYSREG_DR6_BS) {
			/* See comment later on about QEMU/Resume Flag. */
			if(bp_resume) {
				bp_resume = false;
				kdbg_setup_dreg();
				frame->flags &= ~SYSREG_FLAGS_TF;
				sysreg_dr6_write(0);
				return true;
			}

			reason = KDBG_ENTRY_STEPPED;
		} else if(dr6 & (SYSREG_DR6_B0 | SYSREG_DR6_B1 | SYSREG_DR6_B2 | SYSREG_DR6_B3)) {
			reason = KDBG_ENTRY_BREAK;
			for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
				if(frame->ip == kdbg_breakpoints[i].addr) {
					break;
				}
			}
		}
	}

	kdbg_enter(reason, frame);

	/* Clear the Debug Status Register (DR6). */
	sysreg_dr6_write(0);

	/* So this nasty load of crap is to hack past QEMU's lack of Resume
	 * Flag support. Disable the breakpoint temporarily, set single step
	 * and then re-enable after the step. */
	if(reason == KDBG_ENTRY_BREAK) {
		if(i >= 4 || !kdbg_breakpoints[i].enabled) {
			return true;
		}

		sysreg_dr7_write(sysreg_dr7_read() & ~(1<<i));

		/* Prevent a requested step from actually continuing. */
		if(!(frame->flags & SYSREG_FLAGS_TF)) {
			bp_resume = true;
			frame->flags |= SYSREG_FLAGS_TF;
		}
	}

	return true;
}

/** Call KDBG.
 *
 * Wrapper function for kdbg_main(). If a register structure is provided,
 * then directly calls kdbg_main(), otherwise raises a debug interrupt to
 * generate a register structure and enter KDBG.
 *
 * @param reason	KDBG entry reason.
 * @param frame		Interrupt stack frame. (if NULL will generate one).
 */
void kdbg_enter(int reason, intr_frame_t *frame) {
	if(frame == NULL) {
		/* Raise a debug interrupt so we can get into the debugger
		 * with an interrupt frame. Store the entry reason in EAX. */
		__asm__ volatile("int $1" :: "a"((unative_t)reason));
		return;
	}

	/* Disable breakpoints while KDBG is running. */
	sysreg_dr7_write(0);

	if(kdbg_main(reason, frame) == KDBG_STEP) {
		frame->flags |= SYSREG_FLAGS_TF;
	} else {
		frame->flags &= ~SYSREG_FLAGS_TF;
	}

	/* Work out a new Debug Control Register value. */
	kdbg_setup_dreg();
}

/*
 * Architecture-implemented commands.
 */

/** Print out a stack trace.
 *
 * Prints out a stack trace using the base pointer in the current KDBG
 * register structure.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_backtrace(int argc, char **argv) {
	stack_frame_t *frame;
	size_t off = 0;
	symbol_t *sym;
	ptr_t page;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_NONE, "Prints out a backtrace.\n");
		return KDBG_OK;
	}

	/* Get the stack frame. */
	frame = (stack_frame_t *)curr_kdbg_frame->bp;

	/* Make sure we stay on the same page. */
	page = (ptr_t)frame & PAGE_MASK;

	/* Print out the address of where the exception occurred. */
	sym = symbol_lookup_addr(curr_kdbg_frame->ip, &off);
	kprintf(LOG_NONE, "--- Interrupt ---\n");
	kprintf(LOG_NONE, "[%p] %s+0x%zx\n", curr_kdbg_frame->ip,
	        (sym) ? sym->name : "<unknown>", off);

	kprintf(LOG_NONE, "--- Stacktrace ---\n");
	while(frame && ((ptr_t)frame & PAGE_MASK) == page) {
		if((((ptr_t)frame + sizeof(stack_frame_t) - 1) & PAGE_MASK) != page) {
			break;
		}
		sym = symbol_lookup_addr(frame->addr, &off);
		kprintf(LOG_NONE, "[%p] %s+0x%zx\n", frame->addr,
		        (sym) ? sym->name : "<unknown>", off);
		frame = frame->next;
	}

	return KDBG_OK;
}

/** Delete a breakpoint.
 *
 * Removes the breakpoint with the given ID.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_bdelete(int argc, char **argv) {
	size_t num;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_NONE, "Deletes the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_NONE, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_NONE, "Breakpoint number %zu invalid\n", num);
		return KDBG_FAIL;
	}

	kdbg_breakpoints[num].used = false;
	kdbg_breakpoints[num].enabled = false;
	kdbg_breakpoints[num].addr = 0;
	return KDBG_OK;
}

/** Disable a breakpoint.
 *
 * Disables the breakpoint with the specified ID.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_bdisable(int argc, char **argv) {
	size_t num;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_NONE, "Disables the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_NONE, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_NONE, "Breakpoint number %zu invalid.\n", num);
		return KDBG_FAIL;
	}

	kdbg_breakpoints[num].enabled = false;
	return KDBG_OK;
}

/** Enable a breakpoint.
 *
 * Enables the breakpoint with the specified ID.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_benable(int argc, char **argv) {
	size_t num;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_NONE, "Enables the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_NONE, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_NONE, "Breakpoint number %zu invalid\n", num);
		return KDBG_FAIL;
	}

	kdbg_breakpoints[num].enabled = true;
	return KDBG_OK;
}

/** Create/list breakpoints.
 *
 * Creates a new breakpoint or lists all current breakpoints if no argument
 * given.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_break(int argc, char **argv) {
	size_t i, off = 0;
	unative_t addr;
	symbol_t *sym;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [address]\n\n", argv[0]);

		kprintf(LOG_NONE, "Creates a new breakpoint at the given address. The address is treated as an\n");
		kprintf(LOG_NONE, "expression. If no arguments are given, will list all current breakpoints.\n");
		kprintf(LOG_NONE, "New breakpoints default to being enabled. It should be noted that breakpoints\n");
		kprintf(LOG_NONE, "do not work on older versions of QEMU (0.9.1 or earlier) - it only gained\n");
		kprintf(LOG_NONE, "support for hardware breakpoints in revision 5747.\n");

		return KDBG_OK;
	}

	if(argc < 2) {
		for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
			if(!kdbg_breakpoints[i].used) {
				continue;
			}
			sym = symbol_lookup_addr(kdbg_breakpoints[i].addr, &off);
			kprintf(LOG_NONE, "Breakpoint %zu: [%p] %s+0x%zx (%s)\n", i,
			            kdbg_breakpoints[i].addr, (sym) ? sym->name : "<unknown>",
			            off, (kdbg_breakpoints[i].enabled) ? "enabled" : "disabled");
		}

		return KDBG_OK;
	} else {
		if(kdbg_parse_expression(argv[1], &addr, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(addr < KERNEL_VIRT_BASE) {
			kprintf(LOG_NONE, "Cannot set breakpoint outside of kernel code.\n");
			return KDBG_FAIL;
		}

		/* Search for a free slot. */
		for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
			if(kdbg_breakpoints[i].used) {
				continue;
			}

			kdbg_breakpoints[i].used = true;
			kdbg_breakpoints[i].enabled = true;
			kdbg_breakpoints[i].addr = (ptr_t)addr;

			sym = symbol_lookup_addr(kdbg_breakpoints[i].addr, &off);
			kprintf(LOG_NONE, "Created breakpoint %zu: [%p] %s+0x%zx\n",
			        i, addr, (sym) ? sym->name : "<unknown>", off);
			return KDBG_OK;
		}

		kprintf(LOG_NONE, "No free breakpoint slots.\n");
		return KDBG_FAIL;
	}
}
