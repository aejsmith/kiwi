/* Kiwi kernel debugger
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
 * @brief		Kernel debugger.
 */

#include <console/console.h>

#include <cpu/context.h>
#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/do_printf.h>

#include <mm/aspace.h>
#include <mm/slab.h>
#include <mm/vmem.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <kdbg.h>
#include <module.h>
#include <symtab.h>

/* Please, keep this code safe! Specifically, make sure it doesn't:
 * - Use any allocators.
 * - Prod at pointers without CHECKING them (this is sort-of OK to do within
 *   commands because commands have a failure mechanism which causes them to
 *   fail if a fault occurs during their execution).
 * - Use interrupts.
 * - Use anything related to the scheduler (including any form of
 *   synchronization based on it).
 * This code and all the commands should use as little external code as
 * possible.
 */

extern void arch_reboot(void);

/** Whether KDBG is currently running on any CPU. */
atomic_t kdbg_running = 0;

/** Registers structure that KDBG was entered with. */
intr_frame_t *curr_kdbg_frame = NULL;

static context_t kdbg_fault_context;			/**< Context to restore upon exceptions. */
static char kdbg_in_buffer[KDBG_INPUT_BUF_SIZE];	/**< User input buffer. */
static size_t kdbg_step_count = 0;			/**< Instructions remaining to single-step. */

/** Prototypes for core commands. */
static int kdbg_cmd_continue(int argc, char **argv);
static int kdbg_cmd_examine(int argc, char **argv);
static int kdbg_cmd_help(int argc, char **argv);
static int kdbg_cmd_print(int argc, char **argv);
static int kdbg_cmd_reboot(int argc, char **argv);
static int kdbg_cmd_step(int argc, char **argv);

/** Array of command definitions. */
static struct {
	const char *command;
	const char *description;
	int (*function)(int, char **);
} kdbg_commands[] = {
	{ "aspace",	"Dump out an address space.",			kdbg_cmd_aspace },
	{ "backtrace",	"Print a backtrace.",				kdbg_cmd_backtrace },
	{ "bdelete",	"Delete a breakpoint.",				kdbg_cmd_bdelete },
	{ "bdisable",	"Disable a breakpoint.",			kdbg_cmd_bdisable },
	{ "benable",	"Enable a breakpoint.",				kdbg_cmd_benable },
	{ "break",	"Create/list breakpoints.",			kdbg_cmd_break },
	{ "continue",	"Exit KDBG and continue execution.",		kdbg_cmd_continue },
	{ "cpus",	"Print a list of CPUs.",			kdbg_cmd_cpus },
	{ "examine",	"Examine the contents of memory.",		kdbg_cmd_examine },
	{ "help",	"Display this help.",				kdbg_cmd_help },
	{ "modules",	"Print a list of kernel modules.",		kdbg_cmd_modules },
	{ "print",	"Print the value of an expression.",		kdbg_cmd_print },
	{ "process",	"Print a list of processes.",			kdbg_cmd_process },
	{ "reboot",	"Reboot the system.",				kdbg_cmd_reboot },
	{ "regs",	"Print the value of all registers.",		kdbg_cmd_regs },
	{ "slab",	"Print out a list of all slab caches.",		kdbg_cmd_slab },
	{ "step",	"Step a certain number of instructions.",	kdbg_cmd_step },
	{ "thread",	"Print a list of threads.",			kdbg_cmd_thread },
	{ "vmem",	"Show information about Vmem arenas.",		kdbg_cmd_vmem },
};

/*
 * Built-in commands.
 */

/** Exit KDBG and resume execution.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_CONTINUE. */
static int kdbg_cmd_continue(int argc, char **argv) {
	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_NONE, "Leaves KDBG and resumes normal execution. This is not possible if KDBG was\n");
		kprintf(LOG_NONE, "entered due to a fatal error.\n");
		return KDBG_OK;
	}
	return KDBG_CONTINUE;
}

/** Examine the contents of memory.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Command status. */
static int kdbg_cmd_examine(int argc, char **argv) {
	size_t count = 0, size = 1, i;
	uint64_t val = 0;
	unative_t temp;
	char fmt = 'x';
	int exp = 1;
	ptr_t addr;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [/format] expression\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out memory content according to the specified format. The format string\n");
		kprintf(LOG_NONE, "is made up of an optional count followed by any of the following arguments:\n");
		kprintf(LOG_NONE, "  Format modifiers\n");
		kprintf(LOG_NONE, "    x    Hexadecimal (base 16) (default).\n");
		kprintf(LOG_NONE, "    i    Signed decimal (base 10).\n");
		kprintf(LOG_NONE, "    u    Unsigned decimal (base 10).\n");
		kprintf(LOG_NONE, "    o    Octal (base 8).\n");
		kprintf(LOG_NONE, "    s    Character string.\n");
		kprintf(LOG_NONE, "  Size specifiers:\n");
		kprintf(LOG_NONE, "    b    Byte (8-bit).\n");
		kprintf(LOG_NONE, "    w    Word (16-bit).\n");
		kprintf(LOG_NONE, "    d    Double-word (32-bit).\n");
		kprintf(LOG_NONE, "    q    Quad-word (64-bit).\n");
		kprintf(LOG_NONE, "The count specifies the number of entries to print, each of the size given by\n");
		kprintf(LOG_NONE, "the size specifier. The value of the expression is used as the start address.\n");
		kprintf(LOG_NONE, "For help on what can be included in expressions, use 'help print'.\n");
		kprintf(LOG_NONE, "If the memory address is invalid, then a fault will occur, in which case the\n");
		kprintf(LOG_NONE, "command will fail.\n");

		return KDBG_OK;
	} else if(argc < 2 || (argv[1][0] == '/' && argc < 3)) {
		kprintf(LOG_NONE, "Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Look for a format argument. */
	if(argv[1][0] == '/') {
		exp = 2;
		argv[1]++;

		while(isdigit(*argv[1])) {
			count = (count * 10) + (*(argv[1]++) - '0');
		}

		/* Handle remaining format characters. */
		while(*argv[1]) {
			switch(*argv[1]) {
			case 'x': case 'i': case 'u': case 'o': case 's':
				fmt = *argv[1];
				break;
			case 'b': size = 1; break;
			case 'w': size = 2; break;
			case 'd': size = 4; break;
			case 'q': size = 8; break;
			default:
				kprintf(LOG_NONE, "Unknown format character '%c'\n", *argv[1]);
				return KDBG_FAIL;
			}
			argv[1]++;
		}
	}

	/* Default value. */
	if(count == 0) {
		count = 1;
	}

	/* Parse the expression. */
	if(kdbg_parse_expression(argv[exp], &temp, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}
	addr = (ptr_t)temp;

	/* Print each required entry. */
	for(i = 0; i < count; i++) {
		/* Get the value. */
		switch(size) {
		case 1:	val = (uint64_t)(*(uint8_t  *)addr); break;
		case 2:	val = (uint64_t)(*(uint16_t *)addr); break;
		case 4:	val = (uint64_t)(*(uint32_t *)addr); break;
		case 8:	val = (uint64_t)(*(uint64_t *)addr); break;
		}

		/* Print it out. Don't put a newline between each value for
		 * strings. */
		switch(fmt) {
		case 'x':	kprintf(LOG_NONE, "0x%p: 0x%" PRIx64 "\n", addr, val); break;
		case 'i':	kprintf(LOG_NONE, "0x%p: %"   PRId64 "\n", addr, val); break;
		case 'o':	kprintf(LOG_NONE, "0x%p: 0%"  PRIo64 "\n", addr, val); break;
		case 'u':	kprintf(LOG_NONE, "0x%p: %"   PRIu64 "\n", addr, val); break;
		case 's':	kprintf(LOG_NONE, "%c", (uint8_t)val); break;
		}

		addr += size;
	}

	/* Print a newline if we've just printed a string. */
	if(fmt == 's') {
		kprintf(LOG_NONE, "\n");
	}
	return KDBG_OK;
}

/** Get KDBG usage information.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Command status. */
static int kdbg_cmd_help(int argc, char **argv) {
	const char *nargs[] = { NULL, "--help", NULL };
	size_t i;

	/* If we want a specific command, call it with --help as an argument. */
	if(argc > 1 && !KDBG_HELP(argc, argv)) {
		nargs[0] = argv[1];
		for(i = 0; i < ARRAYSZ(kdbg_commands); i++) {
			if(strcmp(argv[1], kdbg_commands[i].command) == 0) {
				kdbg_commands[i].function(2, (char **)nargs);
				return KDBG_OK;
			}
		}

		kprintf(LOG_NONE, "Requested help on invalid command '%s'\n", argv[1]);
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "%-12s  %s\n", "Command", "Info");
	kprintf(LOG_NONE, "%-12s  %s\n", "=======", "====");

	for(i = 0; i < ARRAYSZ(kdbg_commands); i++) {
		kprintf(LOG_NONE, "%-12s  %s\n", kdbg_commands[i].command, kdbg_commands[i].description);
	}

	kprintf(LOG_NONE, "\nFor help on a specific command, type \"help <command>\".\n");
	return KDBG_OK;
}

/** Print the value of an expression.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Command status. */
static int kdbg_cmd_print(int argc, char **argv) {
	unative_t value;
	char fmt = 'i';
	int exp = 1;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [/format] expression\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints the value of an expression in the specified format. Possible formats are:\n");
		kprintf(LOG_NONE, "  x    Hexadecimal (base 16).\n");
		kprintf(LOG_NONE, "  i    Signed decimal (base 10) (default).\n");
		kprintf(LOG_NONE, "  u    Unsigned decimal (base 10).\n");
		kprintf(LOG_NONE, "  o    Octal (base 8).\n");
		kprintf(LOG_NONE, "The expression can be made up of constant values, registers and symbol\n");
		kprintf(LOG_NONE, "addresses. It can also contain basic arithmetic operations (+-*/). To use the\n");
		kprintf(LOG_NONE, "value contained in a register, prefix the name with a %% character. To use a\n");
		kprintf(LOG_NONE, "symbol address, prefix its name with an & character.\n");

		return KDBG_OK;
	} else if(argc < 2 || (argv[1][0] == '/' && argc < 3)) {
		kprintf(LOG_NONE, "Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Look for a format argument. */
	if(argv[1][0] == '/') {
		exp = 2;
		switch(argv[1][1]) {
		case 'x': case 'i': case 'u': case 'o':
			fmt = argv[1][1];
			break;
		default:
			kprintf(LOG_NONE, "Unknown format character '%c'\n", argv[1][1]);
			return KDBG_FAIL;
		}
	}

	/* Parse the expression and print it. */
	if(kdbg_parse_expression(argv[exp], &value, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}

	switch(fmt) {
	case 'x':	kprintf(LOG_NONE, "0x%" PRIxn "\n", value); break;
	case 'i':	kprintf(LOG_NONE, "%" PRIdn "\n", value); break;
	case 'o':	kprintf(LOG_NONE, "0%" PRIon "\n", value); break;
	case 'u':	kprintf(LOG_NONE, "%" PRIun "\n", value); break;
	}
	return KDBG_OK;
}

/** Reboot the system
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Doesn't return on success, KDBG_FAIL on failure. */
static int kdbg_cmd_reboot(int argc, char **argv) {
	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_NONE, "Reboots the system.\n");
		return KDBG_OK;
	}

	arch_reboot();
	return KDBG_FAIL;
}

/** Step a certain number of instructions.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Command status. */
static int kdbg_cmd_step(int argc, char **argv) {
	unative_t value = 1;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [count]\n\n", argv[0]);

		kprintf(LOG_NONE, "Steps the given number of CPU instructions and breaks into KDBG again. If\n");
		kprintf(LOG_NONE, "no instruction count is specified, then will step one instruction. The\n");
		kprintf(LOG_NONE, "instruction count is given as an expression.\n");

		return KDBG_OK;
	}

	/* If we were given a count, then use it, otherwise default to 1. */
	if(argc > 1) {
		if(kdbg_parse_expression(argv[1], &value, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!value) {
			kprintf(LOG_NONE, "Instruction count must be non-zero\n");
			return KDBG_FAIL;
		}
	}

	kdbg_step_count = value;
	return KDBG_STEP;
}

/*
 * Main functions.
 */

/** Find all tab completion matches for the current input buffer.
 * @param lenp		Pointer to current buffer length.
 * @return		Whether to reprint the current line. */
static bool kdbg_tab_complete(size_t *lenp) {
	size_t matches = 0, printed = 0, len = *lenp, i;

	/* First find all the matches. */
	for(i = 0; i < ARRAYSZ(kdbg_commands); i++) {
		if(len >= strlen(kdbg_commands[i].command)) {
			continue;
		}
		if(strncmp(kdbg_in_buffer, kdbg_commands[i].command, len) == 0) {
			matches++;
		}
	}

	/* If we only have one match, go and find it and complete. If we have
	 * multiple matches, print a list. */
	if(matches == 1) {
		for(i = 0; i < ARRAYSZ(kdbg_commands); i++) {
			if(len >= strlen(kdbg_commands[i].command)) {
				continue;
			} else if(strncmp(kdbg_in_buffer, kdbg_commands[i].command, len) != 0) {
				continue;
			}

			/* Check if it fits. */
			if((strlen(kdbg_commands[i].command) - len) >= (KDBG_INPUT_BUF_SIZE - 1)) {
				return false;
			}

			strcpy(kdbg_in_buffer + len, kdbg_commands[i].command + len);
			kprintf(LOG_NONE, "%s ", kdbg_commands[i].command + len);
			*lenp = len + strlen(kdbg_commands[i].command + len) + 1;
			kdbg_in_buffer[*lenp - 1] = ' ';
		}

		return false;
	} else if(matches > 1) {
		kprintf(LOG_NONE, "\n");
		for(i = 0; i < ARRAYSZ(kdbg_commands); i++) {
			if(len >= strlen(kdbg_commands[i].command)) {
				continue;
			} else if(strncmp(kdbg_in_buffer, kdbg_commands[i].command, len) != 0) {
				continue;
			}

			if(printed >= 5) {
				kprintf(LOG_NONE, "\n");
				printed = 0;
			}
			kprintf(LOG_NONE, "%-12s ", kdbg_commands[i].command);
			printed++;
		}

		kprintf(LOG_NONE, "\n");
		return true;
	}

	return false;
}

/** Get a line of input. Overwrites data from previous call.
 * @param count		Current command number.
 * @return		Pointer to start of line. */
static char *kdbg_get_input(int count) {
	size_t len = 0, spaces = 0;
	unsigned char ch;

	kprintf(LOG_NONE, "KDBG:%03d> ", count);

	memset(kdbg_in_buffer, 0, sizeof(kdbg_in_buffer));

	while(1) {
		ch = kdbg_get_char();
		if(ch == '\n') {
			kprintf(LOG_NONE, "%c", ch);
			break;
		} else if(ch == '\b') {
			if(len > 0) {
				if(kdbg_in_buffer[len - 1] == ' ') {
					spaces--;
				}

				kdbg_in_buffer[--len] = 0;
				kprintf(LOG_NONE, "\b \b");
			}
		} else if(len < (KDBG_INPUT_BUF_SIZE - 1)) {
			if(ch == '\t') {
				kdbg_in_buffer[len] = 0;

				/* Handle tab completion and reprint the line
				 * if needed. */
				if(kdbg_tab_complete(&len)) {
					kprintf(LOG_NONE, "KDBG:%03d> %s", count, kdbg_in_buffer);
				}
			} else {
				kprintf(LOG_NONE, "%c", ch);
				kdbg_in_buffer[len++] = ch;
				if(ch == ' ') {
					spaces++;
				}
			}
		}
	}

	kdbg_in_buffer[len] = 0;
	return kdbg_in_buffer;
}

/** Process a line of input. At most 16 command arguments allowed.
 * @param input		Line of input read from the console.
 * @return		Status returned by command. */
static int kdbg_process_input(char *input) {
	int argc = 0, ret;
	char *next, *cur;
	char *argv[16];
	size_t s;

	/* Loop through the input and seperate all the arguments out. We use
	 * a static array to store arguments because we cannot use any
	 * allocators. */
	next = input;
	while(next != NULL) {
		cur = strsep(&next, " ");
		if(strlen(cur) == 0) {
			continue;
		}

		argv[argc++] = cur;
		if(argc >= 16) {
			kprintf(LOG_NONE, "KDBG: Error: Too many arguments, only 16 allowed\n");
			return KDBG_FAIL;
		}
	}

	argv[argc] = NULL;

	/* Search for the command and run it. */
	for(s = 0; s < (sizeof(kdbg_commands) / sizeof(kdbg_commands[0])); s++) {
		if(strcmp(kdbg_commands[s].command, argv[0]) != 0) {
			continue;
		}

		/* Set kdbg_running to 2 to signify that we're in a command. */
		atomic_set(&kdbg_running, 2);

		ret = context_save(&kdbg_fault_context);
		if(ret != 0) {
			/* Fault occurred. */
			ret = KDBG_FAIL;
		} else {
			ret = kdbg_commands[s].function(argc, argv);
		}

		atomic_set(&kdbg_running, 1);
		return ret;
	}

	kprintf(LOG_NONE, "KDBG: Unknown command '%s'\n", argv[0]);
	return KDBG_FAIL;
}

/** Check if a character is a supported operator. */
#define isoperator(ch)		((ch) == '+' || (ch) == '-' || (ch) == '*' || (ch) == '/')

/** Parse a KDBG expression.
 *
 * Parses the given expression string and returns the value it evaluates to.
 * If the expression is a string (surrounded by double quotes) then the pointer
 * pointed to by strp will be modified to point to the content of the
 * string, with quotes removed. Otherwise, the value of the expression will
 * be worked out and placed in the value pointed to by valp.
 *
 * @todo		This could be better...
 *
 * @param exp		Expression to parse.
 * @param valp		Where to store parsed value.
 * @param strp		Where to store string (can be NULL).
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_parse_expression(char *exp, unative_t *valp, char **strp) {
	static char namebuf[64];
	unative_t val = 0, temp;
	char operator = 0;
	symbol_t *sym;
	size_t len;

	/* Check for a string. */
	if(exp[0] == '"') {
		if(strp == NULL) {
			kprintf(LOG_NONE, "KDBG: Unexpected string expression\n");
			return KDBG_FAIL;
		} else if(strlen(exp) < 2 || exp[strlen(exp) - 1] != '"') {
			kprintf(LOG_NONE, "KDBG: Missing \" while parsing string expression\n");
			return KDBG_FAIL;
		}

		exp[strlen(exp) - 1] = 0;
		*strp = exp + 1;
		return KDBG_OK;
	}

	/* Loop through each component of the expression. Components are
	 * separated by an operator (+/-), and should begin with a %, & or a
	 * digit. */
	while(*exp) {
		if(!isdigit(exp[0]) && exp[0] != '%' && exp[0] != '&') {
			kprintf(LOG_NONE, "KDBG: Syntax error in expression\n");
			return KDBG_FAIL;
		}

		/* Find the length of the component. */
		len = 0;
		while(!isoperator(exp[len]) && exp[len] != 0) {
			len++;
		}

		if(exp[0] == '%') {
			/* Register name. */
			if((len - 1) == 0) {
				kprintf(LOG_NONE, "KDBG: Expected register name after '%'\n");
				return KDBG_FAIL;
			}

			if(kdbg_register_value(exp + 1, len - 1, &temp) != KDBG_OK) {
				return KDBG_FAIL;
			}
		} else if(exp[0] == '&') {
			if((len - 1) >= 64) {
				kprintf(LOG_NONE, "KDBG: Symbol name too long\n");
				return KDBG_FAIL;
			}

			strncpy(namebuf, exp + 1, len - 1);
			namebuf[len - 1] = 0;

			sym = symtab_lookup_name(&kernel_symtab, namebuf, false, false);
			if(sym == NULL) {
				kprintf(LOG_NONE, "KDBG: Symbol '%s' not found\n", namebuf);
				return KDBG_FAIL;
			}

			temp = (unative_t)sym->addr;
		} else {
			/* Safe to use strtoull() here - it'll stop after it
			 * encounters a non-digit character. */
			temp = (unative_t)strtoull(exp, NULL, 0);
		}

		/* Combine the temporary value with the final value. */
		switch(operator) {
		case '+':	val += temp; break;
		case '-':	val -= temp; break;
		case '*':	val *= temp; break;
		case '/':	val /= temp; break;
		default:	val  = temp; break;
		}

		/* Check for the next operator. */
		exp += len;
		if(isoperator(exp[0])) {
			operator = *(exp++);
			if(exp[0] == 0) {
				kprintf(LOG_NONE, "KDBG: Expected operand after operator\n");
				return KDBG_FAIL;
			}
		}
	}

	*valp = val;
	return KDBG_OK;
}

/** Handle an exception during KDBG execution.
 *
 * Handles any CPU exceptions that occur while a KDBG command is executing.
 * Modifies the registers structure to return to the saved CPU context.
 *
 * @param num		Interrupt number.
 * @param name		String representation of exception.
 * @param frame		Interrupt stack frame.
 */
void kdbg_except_handler(unative_t num, const char *name, intr_frame_t *frame) {
	kprintf(LOG_NONE, "KDBG: Exception %" PRIun " (%s) occurred during command (%p)\n",
	                  num, name, frame->ip);
	context_restore_frame(&kdbg_fault_context, frame);
}

/** Debugger main function.
 *
 * Main function for the kernel debugger. Never, EVER call this function
 * directly. Use kdbg_enter() instead.
 *
 * @param reason	Reason for entry to KDBG.
 * @param frame		Interrupt stack frame.
 *
 * @return		Return status.
 */
int kdbg_main(int reason, intr_frame_t *frame) {
	bool state = intr_disable();
	static int pcount = 0;
	symbol_t *sym;
	char *input;
	size_t off;
	int ret;

	/* Double check that we have a registers structure. */
	if(!frame) {
		kprintf(LOG_NONE, "KDBG: Error: No registers structure provided\n");
		intr_restore(state);
		return KDBG_FAIL;
	}

	/* If we're in KDBG multiple times, bad bad bad! */
	if(!atomic_cmp_set(&kdbg_running, 0, 1)) {
		/* A value of 3 means KDBG is disabled. */
		if(atomic_get(&kdbg_running) != 3) {
			kprintf(LOG_NONE, "KDBG: Error: Multiple entries to KDBG!\n");
		}
		intr_restore(state);
		return KDBG_FAIL;
	}

	/* If we still have steps remaining, process them. */
	if(kdbg_step_count) { 
		if(reason == KDBG_ENTRY_STEPPED) {
			if(--kdbg_step_count > 0) {
				atomic_set(&kdbg_running, 0);
				intr_restore(state);
				return KDBG_STEP;
			}
		} else {
			kprintf(LOG_NONE, "KDBG: Warning: Non-step entry with %" PRIs " steps remaining\n",
			        kdbg_step_count);
			kdbg_step_count = 0;
			pcount = 0;
		}
	} else if(reason == KDBG_ENTRY_STEPPED) {
		kprintf(LOG_NONE, "KDBG: Warning: Stepped but no step in progress?\n");
	}

	/* Ask all other CPUs to pause execution. */
	cpu_pause_all();

	curr_kdbg_frame = frame;

	sym = symtab_lookup_addr(&kernel_symtab, frame->ip, &off);
	if(reason == KDBG_ENTRY_BREAK) {
		kprintf(LOG_NONE, "\nBreakpoint at [%p] %s+0x%" PRIxs "\n",
		        frame->ip, (sym) ? sym->name : "<unknown>", off);
	} else if(reason == KDBG_ENTRY_STEPPED) {
		kprintf(LOG_NONE, "Stepped to [%p] %s+0x%" PRIxs "\n",
		        frame->ip, (sym) ? sym->name : "<unknown>", off);
	} else {
		kprintf(LOG_NONE, "\nEntered KDBG from [%p] %s+0x%" PRIxs "\n",
		        frame->ip, (sym) ? sym->name : "<unknown>", off);
	}

	/* Main loop - print a prompt, get a command and process it. */
	while(1) {
		input = kdbg_get_input(pcount++);
		if(strlen(input) == 0) {
			kprintf(LOG_NONE, "KDBG: Please enter a command.\n");
			continue;
		}

		ret = kdbg_process_input(input);
		if(ret == KDBG_CONTINUE) {
			/* If we've come from fatal(), we cannot continue. */
			if(reason == KDBG_ENTRY_FATAL) {
				kprintf(LOG_NONE, "KDBG: Continuing is not possible.\n");
			} else {
				kprintf(LOG_NONE, "Continuing execution...\n");
				pcount = 0;
				ret = KDBG_OK;
				break;
			}
		} else if(ret == KDBG_STEP) {
			if(reason == KDBG_ENTRY_FATAL) {
				kprintf(LOG_NONE, "KDBG: Single-step is not possible.\n");
			} else {
				break;
			}
		}
	}

	/* Resume other CPUs. */
	cpu_resume_all();

	atomic_set(&kdbg_running, 0);
	intr_restore(state);
	return ret;
}
