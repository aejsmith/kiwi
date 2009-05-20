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

#include <arch/asm.h>
#include <arch/defs.h>
#include <arch/mem.h>
#include <arch/page.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <symtab.h>
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

	write_dr0(kdbg_breakpoints[0].addr);
	write_dr1(kdbg_breakpoints[1].addr);
	write_dr2(kdbg_breakpoints[2].addr);
	write_dr3(kdbg_breakpoints[3].addr);

	for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
		if(kdbg_breakpoints[i].enabled) {
			dr7 |= (1<<i);
		}
	}

	write_dr7(dr7);
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
	dr6 = read_dr6();
	if(!(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD | X86_DR6_BS | X86_DR6_BT))) {
		/* No bits set, assume this came from from kdbg_enter(), in
		 * which case the reason will be in EAX. */
		reason = (unative_t)frame->ax;
	} else {
		if(dr6 & X86_DR6_BS) {
			/* See comment later on about QEMU/Resume Flag. */
			if(bp_resume) {
				bp_resume = false;
				kdbg_setup_dreg();
				frame->flags &= ~X86_FLAGS_TF;
				write_dr6(0);
				return true;
			}

			reason = KDBG_ENTRY_STEPPED;
		} else if(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3)) {
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
	write_dr6(0);

	/* So this nasty load of crap is to hack past QEMU's lack of Resume
	 * Flag support. Disable the breakpoint temporarily, set single step
	 * and then re-enable after the step. */
	if(reason == KDBG_ENTRY_BREAK) {
		if(i >= 4 || !kdbg_breakpoints[i].enabled) {
			return true;
		}

		write_dr7(read_dr7() & ~(1<<i));

		/* Prevent a requested step from actually continuing. */
		if(!(frame->flags & X86_FLAGS_TF)) {
			bp_resume = true;
			frame->flags |= X86_FLAGS_TF;
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
	write_dr7(0);

	if(kdbg_main(reason, frame) == KDBG_STEP) {
		frame->flags |= X86_FLAGS_TF;
	} else {
		frame->flags &= ~X86_FLAGS_TF;
	}

	/* Work out a new Debug Control Register value. */
	kdbg_setup_dreg();
}

/** Helper macro for kdbg_register_value(). */
#define KDBG_REGISTER_CHECK(n, l, p, rn, rl, rv)	\
	if((l) == (rl) && strncmp((n), (rn), (l)) == 0) { \
		*(p) = (rv); \
		return KDBG_OK; \
	}

/** Get the value of a register.
 *
 * Gets the value of a register and stores it in the given location.
 *
 * @param name		Name of register.
 * @param len		Length of register name.
 * @param regp		Location to store register value in.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_register_value(const char *name, size_t len, unative_t *regp) {
	/* First handle registers present all machines. */
	KDBG_REGISTER_CHECK(name, len, regp, "gs", 2, curr_kdbg_frame->gs);
	KDBG_REGISTER_CHECK(name, len, regp, "fs", 2, curr_kdbg_frame->fs);
	KDBG_REGISTER_CHECK(name, len, regp, "cs", 2, curr_kdbg_frame->cs);
	KDBG_REGISTER_CHECK(name, len, regp, "int_no", 6, curr_kdbg_frame->int_no);
	KDBG_REGISTER_CHECK(name, len, regp, "err_code", 8, curr_kdbg_frame->err_code);
#if CONFIG_ARCH_64BIT
	/* AMD64 registers. */
	KDBG_REGISTER_CHECK(name, len, regp, "r15",    3, curr_kdbg_frame->r15);
	KDBG_REGISTER_CHECK(name, len, regp, "r14",    3, curr_kdbg_frame->r14);
	KDBG_REGISTER_CHECK(name, len, regp, "r13",    3, curr_kdbg_frame->r13);
	KDBG_REGISTER_CHECK(name, len, regp, "r12",    3, curr_kdbg_frame->r12);
	KDBG_REGISTER_CHECK(name, len, regp, "r11",    3, curr_kdbg_frame->r11);
	KDBG_REGISTER_CHECK(name, len, regp, "r10",    3, curr_kdbg_frame->r10);
	KDBG_REGISTER_CHECK(name, len, regp, "r9",     2, curr_kdbg_frame->r9);
	KDBG_REGISTER_CHECK(name, len, regp, "r8",     2, curr_kdbg_frame->r8);
	KDBG_REGISTER_CHECK(name, len, regp, "rbp",    3, curr_kdbg_frame->bp);
	KDBG_REGISTER_CHECK(name, len, regp, "rsi",    3, curr_kdbg_frame->si);
	KDBG_REGISTER_CHECK(name, len, regp, "rdi",    3, curr_kdbg_frame->di);
	KDBG_REGISTER_CHECK(name, len, regp, "rdx",    3, curr_kdbg_frame->dx);
	KDBG_REGISTER_CHECK(name, len, regp, "rcx",    3, curr_kdbg_frame->cx);
	KDBG_REGISTER_CHECK(name, len, regp, "rbx",    3, curr_kdbg_frame->bx);
	KDBG_REGISTER_CHECK(name, len, regp, "rax",    3, curr_kdbg_frame->ax);
	KDBG_REGISTER_CHECK(name, len, regp, "rip",    3, curr_kdbg_frame->ip);
	KDBG_REGISTER_CHECK(name, len, regp, "rflags", 6, curr_kdbg_frame->flags);
	KDBG_REGISTER_CHECK(name, len, regp, "rsp",    3, curr_kdbg_frame->sp);
	KDBG_REGISTER_CHECK(name, len, regp, "ss",     2, curr_kdbg_frame->ss);
#else
	/* IA32 registers. */
	KDBG_REGISTER_CHECK(name, len, regp, "es",     2, curr_kdbg_frame->es);
	KDBG_REGISTER_CHECK(name, len, regp, "ds",     2, curr_kdbg_frame->ds);
	KDBG_REGISTER_CHECK(name, len, regp, "edi",    3, curr_kdbg_frame->di);
	KDBG_REGISTER_CHECK(name, len, regp, "esi",    3, curr_kdbg_frame->si);
	KDBG_REGISTER_CHECK(name, len, regp, "ebp",    3, curr_kdbg_frame->bp);
	KDBG_REGISTER_CHECK(name, len, regp, "ebx",    3, curr_kdbg_frame->bx);
	KDBG_REGISTER_CHECK(name, len, regp, "edx",    3, curr_kdbg_frame->dx);
	KDBG_REGISTER_CHECK(name, len, regp, "ecx",    3, curr_kdbg_frame->cx);
	KDBG_REGISTER_CHECK(name, len, regp, "eax",    3, curr_kdbg_frame->ax);
	KDBG_REGISTER_CHECK(name, len, regp, "eip",    3, curr_kdbg_frame->ip);
	KDBG_REGISTER_CHECK(name, len, regp, "eflags", 6, curr_kdbg_frame->flags);
	if(curr_kdbg_frame->cs & 3) {
		KDBG_REGISTER_CHECK(name, len, regp, "esp",    3, curr_kdbg_frame->sp);
		KDBG_REGISTER_CHECK(name, len, regp, "ss",     2, curr_kdbg_frame->ss);
	} else {
		KDBG_REGISTER_CHECK(name, len, regp, "esp",    3, curr_kdbg_frame->ksp);
	}
#endif
	kprintf(LOG_KDBG, "KDBG: Invalid register name '%.*s'\n", len, name);
	return KDBG_FAIL;
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
	symbol_t *sym;
	size_t off;
	ptr_t page;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_KDBG, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_KDBG, "Prints out a backtrace.\n");
		return KDBG_OK;
	}

	/* Get the stack frame. */
	frame = (stack_frame_t *)curr_kdbg_frame->bp;

	/* Make sure we stay on the same page. */
	page = (ptr_t)frame & PAGE_MASK;

	/* Print out the address of where the exception occurred. */
	sym = symtab_lookup_addr(&kernel_symtab, curr_kdbg_frame->ip, &off);
	kprintf(LOG_KDBG, "--- Interrupt ---\n");
	kprintf(LOG_KDBG, "[%p] %s+0x%" PRIxs "\n", curr_kdbg_frame->ip,
	        (sym) ? sym->name : "<unknown>", off);

	kprintf(LOG_KDBG, "--- Stacktrace ---\n");
	while(frame && ((ptr_t)frame & PAGE_MASK) == page) {
		sym = symtab_lookup_addr(&kernel_symtab, frame->addr, &off);
		kprintf(LOG_KDBG, "[%p] %s+0x%" PRIxs "\n", frame->addr,
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
		kprintf(LOG_KDBG, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_KDBG, "Deletes the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_KDBG, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_KDBG, "Breakpoint number %" PRIs " invalid\n", num);
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
		kprintf(LOG_KDBG, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_KDBG, "Disables the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_KDBG, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_KDBG, "Breakpoint number %" PRIs " invalid.\n", num);
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
		kprintf(LOG_KDBG, "Usage: %s id\n\n", argv[0]);
		kprintf(LOG_KDBG, "Enables the breakpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_KDBG, "Breakpoint ID expected.\n");
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].used) {
		kprintf(LOG_KDBG, "Breakpoint number %" PRIs " invalid\n", num);
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
	unative_t addr;
	size_t i, off;
	symbol_t *sym;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_KDBG, "Usage: %s [address]\n\n", argv[0]);

		kprintf(LOG_KDBG, "Creates a new breakpoint at the given address. The address is treated as an\n");
		kprintf(LOG_KDBG, "expression. If no arguments are given, will list all current breakpoints.\n");
		kprintf(LOG_KDBG, "New breakpoints default to being enabled. It should be noted that breakpoints\n");
		kprintf(LOG_KDBG, "do not work on older versions of QEMU (0.9.1 or earlier) - it only gained\n");
		kprintf(LOG_KDBG, "support for hardware breakpoints in revision 5747.\n");

		return KDBG_OK;
	}

	if(argc < 2) {
		for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
			if(!kdbg_breakpoints[i].used) {
				continue;
			}
			sym = symtab_lookup_addr(&kernel_symtab, kdbg_breakpoints[i].addr, &off);
			kprintf(LOG_KDBG, "Breakpoint %" PRIs ": [%p] %s+0x%" PRIxs " (%s)\n", i,
			            kdbg_breakpoints[i].addr, (sym) ? sym->name : "<unknown>",
			            off, (kdbg_breakpoints[i].enabled) ? "enabled" : "disabled");
		}

		return KDBG_OK;
	} else {
		if(kdbg_parse_expression(argv[1], &addr, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(addr < KERNEL_VIRT_BASE) {
			kprintf(LOG_KDBG, "Cannot set breakpoint outside of kernel code.\n");
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

			sym = symtab_lookup_addr(&kernel_symtab, kdbg_breakpoints[i].addr, &off);
			kprintf(LOG_KDBG, "Created breakpoint %" PRIs ": [%p] %s+0x%" PRIxs "\n",
			        i, addr, (sym) ? sym->name : "<unknown>", off);
			return KDBG_OK;
		}

		kprintf(LOG_KDBG, "No free breakpoint slots.\n");
		return KDBG_FAIL;
	}
}

/** Print out all registers.
 *
 * Prints out the value of all registers in the current KDBG register set.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_regs(int argc, char **argv) {
	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_KDBG, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_KDBG, "Prints out the values contained in the current CPU register set. If you wish\n");
		kprintf(LOG_KDBG, "to get the value of a single register, use the 'print' command instead.\n");

		return KDBG_OK;
	}
#if CONFIG_ARCH_64BIT
	kprintf(LOG_KDBG, "cs: 0x%04" PRIxn "  ss: 0x%04" PRIxn "  gs: 0x%04" PRIxn "  fs: 0x%04" PRIxn "\n",
	            curr_kdbg_frame->cs, curr_kdbg_frame->ss, curr_kdbg_frame->gs, curr_kdbg_frame->fs);
	kprintf(LOG_KDBG, "int_no: %" PRIun "  err_code: %" PRIun "  rflags: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_KDBG, "rax: 0x%016" PRIxn "  rbx: 0x%016" PRIxn "  rcx: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx);
	kprintf(LOG_KDBG, "rdx: 0x%016" PRIxn "  rdi: 0x%016" PRIxn "  rsi: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->dx, curr_kdbg_frame->di, curr_kdbg_frame->si);
	kprintf(LOG_KDBG, "rbp: 0x%016" PRIxn "  r8:  0x%016" PRIxn "  r9:  0x%016" PRIxn "\n",
	            curr_kdbg_frame->bp, curr_kdbg_frame->r8, curr_kdbg_frame->r9);
	kprintf(LOG_KDBG, "r10: 0x%016" PRIxn "  r11: 0x%016" PRIxn "  r12: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r10, curr_kdbg_frame->r11, curr_kdbg_frame->r12);
	kprintf(LOG_KDBG, "r13: 0x%016" PRIxn "  r14: 0x%016" PRIxn "  r15: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r13, curr_kdbg_frame->r14, curr_kdbg_frame->r15);
	kprintf(LOG_KDBG, "rip: 0x%016" PRIxn "  rsp: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ip, curr_kdbg_frame->sp);
#else
	kprintf(LOG_KDBG, "cs: 0x%04" PRIxn "  ds: 0x%04" PRIxn "  es: 0x%04" PRIxn "  fs: 0x%04" PRIxn "  gs: 0x%04" PRIxn "\n",
	            curr_kdbg_frame->cs, curr_kdbg_frame->ds, curr_kdbg_frame->es, curr_kdbg_frame->fs, curr_kdbg_frame->gs);
	if(curr_kdbg_frame->cs & 3) {
		kprintf(LOG_KDBG, "ss: 0x%04" PRIxn "  ", curr_kdbg_frame->ss);
	}
	kprintf(LOG_KDBG, "int_no: %" PRIun "  err_code: %" PRIun "  eflags: 0x%08" PRIxn "\n",
	            curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_KDBG, "eax: 0x%08" PRIxn "  ebx: 0x%08" PRIxn "  ecx: 0x%08" PRIxn "  edx: 0x%08" PRIxn "\n",
	            curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx, curr_kdbg_frame->dx);
	kprintf(LOG_KDBG, "edi: 0x%08" PRIxn "  esi: 0x%08" PRIxn "  ebp: 0x%08" PRIxn "  eip: 0x%08" PRIxn "\n",
	            curr_kdbg_frame->di, curr_kdbg_frame->si, curr_kdbg_frame->bp, curr_kdbg_frame->ip);
	kprintf(LOG_KDBG, "esp: 0x%08" PRIxn "\n", (curr_kdbg_frame->cs & 3) ? curr_kdbg_frame->sp : curr_kdbg_frame->ksp);
#endif
	return KDBG_OK;
}
