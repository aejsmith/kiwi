/* Kiwi IA32 kernel debugger functions
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
 * @brief		IA32 kernel debugger functions.
 */

#include <arch/memmap.h>
#include <arch/page.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <symtab.h>
#include <kdbg.h>

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
	KDBG_REGISTER_CHECK(name, len, regp, "gs", 2, curr_kdbg_frame->gs);
	KDBG_REGISTER_CHECK(name, len, regp, "fs", 2, curr_kdbg_frame->fs);
	KDBG_REGISTER_CHECK(name, len, regp, "cs", 2, curr_kdbg_frame->cs);
	KDBG_REGISTER_CHECK(name, len, regp, "int_no", 6, curr_kdbg_frame->int_no);
	KDBG_REGISTER_CHECK(name, len, regp, "err_code", 8, curr_kdbg_frame->err_code);
	KDBG_REGISTER_CHECK(name, len, regp, "es", 2, curr_kdbg_frame->es);
	KDBG_REGISTER_CHECK(name, len, regp, "ds", 2, curr_kdbg_frame->ds);
	KDBG_REGISTER_CHECK(name, len, regp, "edi", 3, curr_kdbg_frame->di);
	KDBG_REGISTER_CHECK(name, len, regp, "esi", 3, curr_kdbg_frame->si);
	KDBG_REGISTER_CHECK(name, len, regp, "ebp", 3, curr_kdbg_frame->bp);
	KDBG_REGISTER_CHECK(name, len, regp, "ebx", 3, curr_kdbg_frame->bx);
	KDBG_REGISTER_CHECK(name, len, regp, "edx", 3, curr_kdbg_frame->dx);
	KDBG_REGISTER_CHECK(name, len, regp, "ecx", 3, curr_kdbg_frame->cx);
	KDBG_REGISTER_CHECK(name, len, regp, "eax", 3, curr_kdbg_frame->ax);
	KDBG_REGISTER_CHECK(name, len, regp, "eip", 3, curr_kdbg_frame->ip);
	KDBG_REGISTER_CHECK(name, len, regp, "eflags", 6, curr_kdbg_frame->flags);
	if(curr_kdbg_frame->cs & 3) {
		KDBG_REGISTER_CHECK(name, len, regp, "esp", 3, curr_kdbg_frame->sp);
		KDBG_REGISTER_CHECK(name, len, regp, "ss", 2, curr_kdbg_frame->ss);
	} else {
		KDBG_REGISTER_CHECK(name, len, regp, "esp", 3, curr_kdbg_frame->ksp);
	}
	kprintf(LOG_NONE, "KDBG: Invalid register name '%.*s'\n", len, name);
	return KDBG_FAIL;
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
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out the values contained in the current CPU register set. If you wish\n");
		kprintf(LOG_NONE, "to get the value of a single register, use the 'print' command instead.\n");

		return KDBG_OK;
	}

	kprintf(LOG_NONE, "cs: 0x%04" PRIxn "  ds: 0x%04" PRIxn "  es: 0x%04" PRIxn
	                  "  fs: 0x%04" PRIxn "  gs: 0x%04" PRIxn "\n",
	                  curr_kdbg_frame->cs, curr_kdbg_frame->ds, curr_kdbg_frame->es,
	                  curr_kdbg_frame->fs, curr_kdbg_frame->gs);
	if(curr_kdbg_frame->cs & 3) {
		kprintf(LOG_NONE, "ss: 0x%04" PRIxn "  ", curr_kdbg_frame->ss);
	}
	kprintf(LOG_NONE, "int_no: %" PRIun "  err_code: %" PRIun "  eflags: 0x%08" PRIxn "\n",
	                  curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_NONE, "eax: 0x%08" PRIxn "  ebx: 0x%08" PRIxn "  ecx: 0x%08" PRIxn "  edx: 0x%08" PRIxn "\n",
	                  curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx, curr_kdbg_frame->dx);
	kprintf(LOG_NONE, "edi: 0x%08" PRIxn "  esi: 0x%08" PRIxn "  ebp: 0x%08" PRIxn "  eip: 0x%08" PRIxn "\n",
	                  curr_kdbg_frame->di, curr_kdbg_frame->si, curr_kdbg_frame->bp, curr_kdbg_frame->ip);
	kprintf(LOG_NONE, "esp: 0x%08" PRIxn "\n",
	                  (curr_kdbg_frame->cs & 3) ? curr_kdbg_frame->sp : curr_kdbg_frame->ksp);
	return KDBG_OK;
}
