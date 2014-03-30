/*
 * Copyright (C) 2008-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel debugger.
 *
 * Please, keep this code safe! Specifically, make sure it doesn't:
 *  - Use any allocators apart from the KDB allocator.
 *  - Prod at pointers without CHECKING them (this is sort-of OK to do within
 *    commands because commands have a failure mechanism which causes them to
 *    fail if a fault occurs during their execution).
 *  - Use interrupts.
 *  - Use anything related to the scheduler, or any synchronization primitives.
 *
 * @todo		Change command line parsing so that it recognises stuff
 *			in quotes as all one argument. Will require some changes
 *			to how the expression parser handles strings.
 */

#include <arch/frame.h>
#include <arch/kdb.h>
#include <arch/page.h>

#include <lib/ctype.h>
#include <lib/fixed_heap.h>
#include <lib/printf.h>
#include <lib/string.h>

#include <mm/safe.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/spinlock.h>

#include <console.h>
#include <cpu.h>
#include <elf.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <setjmp.h>

/** KDB heap size. */
#define KDB_HEAP_SIZE		16384

/** Maximum line length. */
#define KDB_MAX_LINE_LEN	256

/** Number of lines to store in the history. */
#define KDB_HISTORY_SIZE	8

/** Maximum number of arguments to a function. */
#define KDB_ARGS_MAX		16

/** KDB command arguments structure. */
typedef struct kdb_args {
	list_t header;			/**< For internal use. */

	int count;			/**< Number of arguments. */
	char *args[KDB_ARGS_MAX];	/**< Array of arguments. */
} kdb_args_t;

/** Structure describing a KDB command. */
typedef struct kdb_command_desc {
	list_t header;			/**< Link to the command list. */
	const char *name;		/**< Name of the command. */
	const char *description;	/**< Description of the command. */
	kdb_command_t func;		/**< Command handler function. */
} kdb_command_desc_t;

/** Notifier to be called when entering/exiting KDB. */
NOTIFIER_DEFINE(kdb_entry_notifier, NULL);
NOTIFIER_DEFINE(kdb_exit_notifier, NULL);

/** Whether KDB is currently running on any CPU. */
atomic_t kdb_running = 0;

/** Interrupt frame that KDB was entered with. */
intr_frame_t *curr_kdb_frame = NULL;

/** Currently remaining steps. */
static size_t kdb_steps_remaining = 0;

/** Context used to handle faults during command execution. */
static jmp_buf kdb_fault_context;

/** Statically allocated heap for use within KDB. */
static char kdb_heap_area[KDB_HEAP_SIZE] __aligned(PAGE_SIZE);
static fixed_heap_t kdb_heap;

/** List of registered commands. */
static LIST_DEFINE(kdb_commands);
static SPINLOCK_DEFINE(kdb_commands_lock);

/** Pointers to lines in the command history. */
static char *kdb_history[KDB_HISTORY_SIZE];
static size_t kdb_history_len = 0;

/** Current input line buffer. */
static char current_input_line[KDB_MAX_LINE_LEN];

/** Buffer for the current line of output. */
static char current_output_line[KDB_MAX_LINE_LEN];
static size_t current_output_pos = 0;

/** Current output filter. */
static kdb_filter_t *current_filter = NULL;

/** Whether to output to the KBoot log. */
static bool use_kboot_log = false;

/**
 * Utility functions.
 */

/** Look up a command.
 * @param name		Name of command to look up.
 * @return		Pointer to command structure, or null if not found. */
static kdb_command_desc_t *lookup_command(const char *name) {
	kdb_command_desc_t *cmd;

	LIST_FOREACH(&kdb_commands, iter) {
		cmd = list_entry(iter, kdb_command_desc_t, header);

		if(strcmp(name, cmd->name) == 0)
			return cmd;
	}

	return NULL;
}

/** Print a character.
 * @param ch		Character to print. */
static void kdb_putc(char ch) {
	if(debug_console.out)
		debug_console.out->putc(ch);
	if(main_console.out)
		main_console.out->putc(ch);
	if(use_kboot_log)
		kboot_log_write(ch);
}

/** Helper for kdb_printf(). */
static void kdb_printf_helper(char ch, void *data, int *total) {
	size_t i;

	*total = *total + 1;

	if(current_filter) {
		if(ch == '\n') {
			current_output_line[current_output_pos] = 0;

			/* Pass the line to the filter. */
			if(current_filter) {
				if(!current_filter->func(current_output_line, current_filter->data)) {
					current_output_pos = 0;
					return;
				}
			}

			/* Output it. */
			for(i = 0; i < current_output_pos; i++)
				kdb_putc(current_output_line[i]);

			kdb_putc('\n');
			current_output_pos = 0;
		} else {
			if(current_output_pos < (KDB_MAX_LINE_LEN - 1))
				current_output_line[current_output_pos++] = ch;
		}
	} else {
		kdb_putc(ch);
	}
}

/** Output function to use within KDB.
 * @param fmt		Format string.
 * @param args		Arguments to substitute into format string. */
void kdb_vprintf(const char *fmt, va_list args) {
	do_vprintf(kdb_printf_helper, NULL, fmt, args);
}

/** Output function to use within KDB.
 * @param fmt		Format string.
 * @param ...		Arguments to substitute into format string. */
void kdb_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	kdb_vprintf(fmt, args);
	va_end(args);
}

/**
 * Print out details of a symbol corresponding to an address.
 *
 * Looks up the symbol corresponding to the given address and prints out
 * details of it. The delta argument is applied to the address before looking
 * it up (and is not applied when actually printing). This is useful when
 * printing symbols in backtraces, as backtraces use the return address of a
 * call which may not yield the correct symbol if the compiler has produced a
 * tail call to a noreturn function.
 *
 * @param addr		Address of symbol.
 * @param delta		Delta to apply on lookup.
 */
void kdb_print_symbol(ptr_t addr, int delta) {
	int width;
	symbol_t sym;
	size_t off;
	elf_image_t *image;
	bool ret;

	/* Zero pad up to the width of a pointer. */
	width = (sizeof(void *) * 2) + 2;

	ret = symbol_from_addr(addr + delta, &sym, &off);
	if(!ret && !sym.image) {
		if(is_user_address((void *)addr) && curr_thread && curr_cpu->aspace) {
			/* Look up in loaded userspace images. */
			LIST_FOREACH(&curr_proc->images, iter) {
				image = list_entry(iter, elf_image_t, header);

				ret = elf_symbol_from_addr(image, addr + delta, &sym, &off);
				if(ret || sym.image)
					break;
			}
		}
	}

	kdb_printf("[%0*p] %s+0x%zx", width, addr, sym.name, (ret) ? off - delta : 0);
	if(sym.image && sym.image->load_base)
		kdb_printf(" (%s+0x%zx)", sym.image->name, addr - sym.image->load_base);
}

/** Backtrace callback.
 * @param addr		Address of backtrace entry. */
static void kdb_backtrace_cb(ptr_t addr) {
	/* See above. */
	kdb_print_symbol(addr, -1);
	kdb_printf("\n");
}

/** Read a character from the console.
 * @return		Character/special key code read. */
uint16_t kdb_getc(void) {
	uint16_t ch;

	while(true) {
		if(debug_console.in) {
			ch = debug_console.in->poll();
			if(ch)
				return ch;
		}
		if(main_console.in) {
			ch = main_console.in->poll();
			if(ch)
				return ch;
		}
	}
}

/** Allocate memory for use within KDB.
 * @param size		Size to allocate.
 * @return		Address of allocation. */
void *kdb_malloc(size_t size) {
	void *ret;

	ret = fixed_heap_alloc(&kdb_heap, size);
	if(!ret) {
		/* The KDB heap can be used outside of KDB when registering
		 * commands. */
		if(atomic_get(&kdb_running)) {
			kdb_printf("Exhausted KDB heap");
			arch_cpu_halt();
		} else {
			fatal("Exhausted KDB heap");
		}
	}

	return ret;
}

/** Free memory allocated by kdb_malloc().
 * @param addr		Address to free. */
void kdb_free(void *addr) {
	fixed_heap_free(&kdb_heap, addr);
}

/**
 * Expression parser.
 */

/** Check if a character is a supported operator.
 * @param ch		Character to check.
 * @return		Whether a supported operator. */
static inline bool isoperator(char ch) {
	return ((ch) == '+' || (ch) == '-' || (ch) == '*' || (ch) == '/');
}

/**
 * Parse a KDB expression.
 *
 * Parses the given expression string and returns the value it evaluates to.
 * If the expression is a string (surrounded by double quotes) then the pointer
 * pointed to by strp will be modified to point to the content of the
 * string, with quotes removed. Otherwise, the value of the expression will
 * be worked out and placed in the value pointed to by valp.
 *
 * @param exp		Expression to parse. Will be modified.
 * @param valp		Where to store parsed value.
 * @param strp		Where to store string (can be null).
 *
 * @return		KDB_SUCCESS on success, KDB_FAILURE on failure.
 */
kdb_status_t kdb_parse_expression(char *exp, uint64_t *valp, char **strp) {
	static char namebuf[128];

	uint64_t val = 0, current;
	unsigned long reg;
	symbol_t sym;
	char oper = 0;
	size_t len;

	/* Check for a string. */
	if(exp[0] == '"') {
		if(!strp) {
			kdb_printf("KDB: Unexpected string expression.\n");
			return KDB_FAILURE;
		} else if(strlen(exp) < 2 || exp[strlen(exp) - 1] != '"') {
			kdb_printf("KDB: Missing \" while parsing string expression.\n");
			return KDB_FAILURE;
		}

		exp[strlen(exp) - 1] = 0;
		*strp = exp + 1;
		return KDB_SUCCESS;
	}

	/* Loop through each component of the expression. Components are
	 * separated by an operator (+/-), and should begin with a %, & or a
	 * digit. */
	while(*exp) {
		if(!isdigit(exp[0]) && exp[0] != '%' && exp[0] != '&') {
			kdb_printf("KDB: Syntax error in expression.\n");
			return KDB_FAILURE;
		}

		/* Find the length of the component. */
		len = 0;
		while(!isoperator(exp[len]) && exp[len] != 0)
			len++;

		if(exp[0] == '%') {
			/* Register name. */
			if((len - 1) == 0) {
				kdb_printf("KDB: Expected register name after '%%'\n");
				return KDB_FAILURE;
			}

			if(!arch_kdb_register_value(exp + 1, len - 1, &reg)) {
				kdb_printf("KDB: Invalid register name '%.*s'.\n", len, exp + 1);
				return KDB_FAILURE;
			}

			current = (uint64_t)reg;
		} else if(exp[0] == '&') {
			if((len - 1) >= 128) {
				kdb_printf("KDB: Symbol name too long.\n");
				return KDB_FAILURE;
			}


			strncpy(namebuf, exp + 1, len - 1);
			namebuf[len - 1] = 0;

			if(!symbol_lookup(namebuf, false, false, &sym)) {
				kdb_printf("KDB: Symbol '%s' not found.\n", namebuf);
				return KDB_FAILURE;
			}

			current = (uint64_t)sym.addr;
		} else {
			/* Safe to use strtoull() here - it'll stop after it
			 * encounters a non-digit character. */
			current = strtoull(exp, NULL, 0);
		}

		/* Combine the temporary value with the final value. */
		switch(oper) {
		case '+':	val += current; break;
		case '-':	val -= current; break;
		case '*':	val *= current; break;
		case '/':	val /= current; break;
		default:	val  = current; break;
		}

		/* Check for the next operator. */
		exp += len;
		if(isoperator(exp[0])) {
			oper = *(exp++);
			if(exp[0] == 0) {
				kdb_printf("KDB: Expected operand after operator\n");
				return KDB_FAILURE;
			}
		}
	}

	*valp = val;
	return KDB_SUCCESS;
}

/**
 * Input parsing functions.
 */

/** Line reading state structure. */
typedef struct kdb_read_line {
	char *buffer;			/**< Buffer being read into. */
	size_t length;			/**< Total length of the buffer. */
	size_t position;		/**< Current position in the buffer. */
} kdb_read_line_t;

/** Insert a character to the buffer at the current position.
 * @param state		Pointer to state structure.
 * @param ch		Character to insert. */
static void kdb_line_insert(kdb_read_line_t *state, char ch) {
	size_t i;

	if(state->length < (KDB_MAX_LINE_LEN - 1)) {
		kdb_putc(ch);
		if(state->position == state->length) {
			state->buffer[state->length++] = ch;
			state->buffer[state->length] = 0;
			state->position++;
		} else {
			memmove(&state->buffer[state->position + 1],
				&state->buffer[state->position],
				state->length - state->position + 1);
			state->buffer[state->position++] = ch;
			state->length++;

			/* Reprint everything after the character, maintaining
			 * the current cursor position. */
			for(i = 0; i < (state->length - state->position); i++)
				kdb_putc(state->buffer[state->position + i]);
			while(i--)
				kdb_putc('\b');
		}
	}
}

/** Erase a character from the current position.
 * @param state		Pointer to state structure.
 * @param forward	If true, will erase the character at the current cursor
 *			position, else will erase the previous one. */
static void kdb_line_erase(kdb_read_line_t *state, bool forward) {
	size_t i;

	if(forward) {
		if(state->position == state->length)
			return;
	} else {
		if(state->position == 0) {
			return;
		} else if(state->position == state->length) {
			state->buffer[--state->length] = 0;
			state->position--;
			kdb_putc('\b');
			kdb_putc(' ');
			kdb_putc('\b');
			return;
		} else {
			/* Decrement position and fall through. */
			state->position--;
			kdb_putc('\b');
		}
	}

	memmove(&state->buffer[state->position], &state->buffer[state->position + 1],
		state->length - state->position);
	state->length--;

	/* Reprint everything, maintaining cursor position. */
	for(i = 0; i < (state->length - state->position); i++)
		kdb_putc(state->buffer[state->position + i]);
	kdb_putc(' ');
	i++;
	while(i--)
		kdb_putc('\b');
}

/** Replace the current line with another.
 * @param state		Pointer to state structure.
 * @param line		Line to replace with. */
static void kdb_line_replace(kdb_read_line_t *state, const char *line) {
	size_t i, len;

	/* First need to clear the line off the screen. If we're in the middle
	 * of a line clear to the end, then clear backwards. */
	for(i = 0; i < (state->length - state->position); i++)
		kdb_putc(' ');
	while(i--)
		kdb_putc('\b');
	while(state->position--) {
		kdb_putc('\b');
		kdb_putc(' ');
		kdb_putc('\b');
	}

	/* Copy in the new string. */
	if(line) {
		len = strlen(line);
		memcpy(state->buffer, line, len + 1);
		state->length = len;
		state->position = len;
		kdb_printf("%s", line);
	} else {
		state->length = 0;
		state->position = 0;
		state->buffer[0] = 0;
	}
}

/** Perform tab completion.
 * @param state		Pointer to state structure.
 * @return		Whether to reprint the current line. */
static bool kdb_line_complete(kdb_read_line_t *state) {
	kdb_command_desc_t *cmd, *first = NULL;
	size_t matches, end, i, printed;

	/* Laziness. Only support tab completion at end of buffer. */
	if(state->position != state->length)
		return false;

	/* If there's a space we can't tab complete, can only complete command
	 * names. */
	if(strchr(state->buffer, ' '))
		return false;

	/* First find all the matches. */
	matches = 0;
	LIST_FOREACH(&kdb_commands, iter) {
		cmd = list_entry(iter, kdb_command_desc_t, header);

		if(state->length >= strlen(cmd->name)) {
			continue;
		} else if(strncmp(state->buffer, cmd->name, state->length) == 0) {
			if(++matches == 1)
				first = cmd;
		}
	}

	/* If we only have one match, go and find it and complete. If we have
	 * multiple matches, print a list. */
	if(matches == 1) {
		end = state->length + strlen(&first->name[state->length]);

		for(i = state->length; i < end; i++)
			kdb_line_insert(state, first->name[i]);

		kdb_line_insert(state, ' ');
		return false;
	} else if(matches > 1) {
		kdb_putc('\n');

		printed = 0;
		LIST_FOREACH(&kdb_commands, iter) {
			cmd = list_entry(iter, kdb_command_desc_t, header);

			if(state->length >= strlen(cmd->name)) {
				continue;
			} else if(strncmp(state->buffer, cmd->name, state->length) != 0) {
				continue;
			}

			if(printed >= 5) {
				kdb_putc('\n');
				printed = 0;
			}

			kdb_printf("%-12s ", cmd->name);
			printed++;
		}

		if(printed)
			kdb_putc('\n');

		return true;
	}

	return false;
}

/** Read a line of input.
 * @param count		Number to print in the prompt.
 * @return		Pointer to the buffer containing the input, null if no
 *			input. */
static char *kdb_read_line(int count) {
	kdb_read_line_t state;
	size_t hist, next, pos;
	uint16_t ch;

	/* Initialize the state. */
	state.length = state.position = 0;
	state.buffer = current_input_line;
	state.buffer[0] = 0;

	/* Current history position. */
	hist = kdb_history_len;

	/* Print the prompt. */
	kdb_printf("KDB:%03d> ", count);

	/* Handle input. */
	while(true) {
		ch = kdb_getc();

		if(ch == '\n') {
			kdb_putc('\n');
			break;
		} else if(ch == '\t') {
			if(kdb_line_complete(&state)) {
				kdb_printf("KDB:%03d> %s", count, state.buffer);
			}
		} else if(ch == '\b') {
			kdb_line_erase(&state, false);
		} else if(ch == 0x7F) {
			kdb_line_erase(&state, true);
		} else if(ch == CONSOLE_KEY_LEFT) {
			if(state.position) {
				kdb_putc('\b');
				state.position--;
			}
		} else if(ch == CONSOLE_KEY_RIGHT) {
			if(state.position != state.length) {
				kdb_putc(state.buffer[state.position]);
				state.position++;
			}
		} else if(ch == CONSOLE_KEY_UP) {
			if(hist)
				kdb_line_replace(&state, kdb_history[--hist]);
		} else if(ch == CONSOLE_KEY_DOWN) {
			if(hist < kdb_history_len) {
				next = ++hist;
				kdb_line_replace(&state, (next < kdb_history_len) ? kdb_history[next] : NULL);
			}
		} else if(ch == CONSOLE_KEY_HOME) {
			while(state.position) {
				kdb_putc('\b');
				state.position--;
			}
		} else if(ch == CONSOLE_KEY_END) {
			while(state.position < state.length) {
				kdb_putc(state.buffer[state.position]);
				state.position++;
			}
		} else {
			/* Ignore non-printing characters. */
			if(isprint(ch))
				kdb_line_insert(&state, (char)ch);
		}
	}

	if(state.length) {
		/* Allocate a new history entry. */
		if(kdb_history_len == KDB_HISTORY_SIZE) {
			kdb_free(kdb_history[0]);
			memmove(kdb_history, &kdb_history[1], sizeof(char *) * (KDB_HISTORY_SIZE - 1));
			pos = KDB_HISTORY_SIZE - 1;
		} else {
			pos = kdb_history_len++;
		}

		kdb_history[pos] = kdb_malloc(state.length + 1);
		strcpy(kdb_history[pos], state.buffer);
		return state.buffer;
	} else {
		return NULL;
	}
}

/** Structure containing parsed line information. */
typedef struct kdb_line {
	kdb_args_t call;		/**< Primary command call. */
	list_t filters;			/**< List of filter commands. */
	int filter_count;		/**< Number of filters. */
} kdb_line_t;

/** Destroy a parsed line.
 * @param data		Data for the line to destroy. */
static void kdb_line_destroy(kdb_line_t *data) {
	kdb_args_t *call;

	LIST_FOREACH_SAFE(&data->filters, iter) {
		call = list_entry(iter, kdb_args_t, header);
		list_remove(&call->header);
		kdb_free(call);
	}
}

/** Parse the command line.
 * @param line		Line to parse.
 * @param data		Data structure to fill in.
 * @return		Whether the command line was parsed successfully. */ 
static bool kdb_line_parse(char *line, kdb_line_t *data) {
	kdb_args_t *call = NULL;
	char *subcmd, *arg;

	list_init(&data->filters);
	data->filter_count = 0;

	/* First split up by pipe. */
	while(line) {
		subcmd = strsep(&line, "|");
		while(isspace(*subcmd))
			subcmd++;

		if(strlen(subcmd) == 0) {
			kdb_printf("KDB: Error: Empty pipe command\n");
			kdb_line_destroy(data);
			return false;
		}

		/* Get an arguments structure to fill in. */
		if(call) {
			call = kdb_malloc(sizeof(kdb_args_t));
			list_init(&call->header);
			list_append(&data->filters, &call->header);
			data->filter_count++;
		} else {
			call = &data->call;
		}

		call->count = 0;

		/* Split into arguments. */
		while(subcmd) {
			arg = strsep(&subcmd, " ");
			if(strlen(arg) == 0)
				continue;

			if(call->count == KDB_ARGS_MAX) {
				kdb_printf("KDB: Error: Maximum of %d arguments supported\n", KDB_ARGS_MAX);
				kdb_line_destroy(data);
				return false;
			}

			call->args[call->count++] = arg;
		}
	}

	return true;
}

/**
 * Main functions.
 */

/** Handle an exception during KDB command execution.
 * @param name		Name of the exception.
 * @param frame		Exception frame. */
void kdb_except_handler(const char *name, intr_frame_t *frame) {
	current_filter = NULL;
	kdb_printf("KDB: %s exception occurred during command (%p)\n", name, frame->ip);
	longjmp(kdb_fault_context, 1);
}

/** Perform a call.
 * @param call		Call to perform.
 * @param filter	Output filter to use.
 * @param filter_arg	Filter argument to pass to function.
 * @return		KDB status code. */
static kdb_status_t perform_call(kdb_args_t *call, kdb_filter_t *filter, kdb_filter_t *filter_arg) {
	kdb_command_desc_t *cmd;
	kdb_status_t ret;

	/* Look up the command. */
	cmd = lookup_command(call->args[0]);
	if(!cmd) {
		kdb_printf("KDB: Error: Unknown command '%s'\n", call->args[0]);
		return KDB_FAILURE;
	}

	/* Set kdb_running to 2 to signal that we're in a command. */
	atomic_set(&kdb_running, 2);
	current_filter = filter;

	/* Save the current context to resume from if a fault occurs. */
	if(setjmp(kdb_fault_context)) {
		ret = KDB_FAILURE;
	} else {
		ret = cmd->func(call->count, call->args, filter_arg);
	}

	current_filter = NULL;
	atomic_set(&kdb_running, 1);
	return ret;
}

/**
 * Main function for the kernel debugger.
 *
 * Main function for the kernel debugger. This should not be called directly,
 * it is called from arch-specific code to enter the debugger. Use kdb_enter()
 * instead.
 *
 * @param reason	Reason for entry to the debugger.
 * @param frame		Interrupt frame.
 * @param index		Index of breakpoint or watchpoint that caused entry.
 *
 * @return		Status code indicating what action to perform.
 */
kdb_status_t kdb_main(kdb_reason_t reason, intr_frame_t *frame, unsigned index) {
	static unsigned cmd_count = 0;

	kdb_filter_t *filter;
	kdb_status_t ret;
	kdb_line_t data;
	char *line;
	bool state;

	/* Don't want to allow any interrupts while we're running. */
	state = local_irq_disable();

	/* Check if we're already running. If we are, something bad has happened. */
	if(atomic_cas(&kdb_running, 0, 1) != 0) {
		kdb_printf("Multiple entries to KDB.\n");
		local_irq_restore(state);
		return KDB_FAILURE;
	}

	/* Handle single-stepping. */
	if(kdb_steps_remaining) {
		if(reason == KDB_REASON_STEP) {
			if(--kdb_steps_remaining > 0) {
				atomic_set(&kdb_running, 0);
				local_irq_restore(state);
				return KDB_STEP;
			}
		} else {
			kdb_steps_remaining = 0;
			cmd_count = 0;
		}
	}

	/* Ask all other CPUs to pause execution. */
	arch_kdb_trap_cpus();

	curr_kdb_frame = frame;

	/* Run entry notifiers. */
	notifier_run_unlocked(&kdb_entry_notifier, NULL, false);

	/* Print information about why we've entered the debugger and where from. */
	if(reason == KDB_REASON_BREAK) {
		kdb_printf("\nBreakpoint %u at ", index);
		kdb_print_symbol(frame->ip, 0);
	} else if(reason == KDB_REASON_WATCH) {
		kdb_printf("\nWatchpoint %u hit by ");
		kdb_print_symbol(frame->ip, 0);
	} else if(reason == KDB_REASON_STEP) {
		kdb_printf("Stepped to ");
		kdb_print_symbol(frame->ip, 0);
	} else if(reason == KDB_REASON_USER) {
		kdb_printf("\nEntered KDB from ");
		kdb_print_symbol(frame->ip, 0);
	} else if(reason == KDB_REASON_FATAL) {
		/* When coming from a fatal error, enable writing to the KBoot
		 * log temporarily as we want to dump some information there. */
		use_kboot_log = true;
	}

	kdb_printf("\nThread %" PRId32 " (%s) on CPU%u\n",
		(curr_thread) ? curr_thread->id : -1,
		(curr_thread) ? curr_thread->name : "<none>", cpu_id());

	/* Dump some information when we come from a fatal error. */
	if(reason == KDB_REASON_FATAL) {
		arch_kdb_dump_registers();

		kdb_printf("Backtrace:\n");
		kdb_backtrace_cb(frame->ip);

		atomic_set(&kdb_running, 2);
		if(setjmp(kdb_fault_context)) {
			/* Backtrace faulted, just ignore. */
		} else {
			arch_kdb_backtrace(NULL, kdb_backtrace_cb);
		}
		atomic_set(&kdb_running, 1);

		/* Flush and disable writing the KBoot log. */
		kboot_log_flush();
		use_kboot_log = false;
	}

	/* Main loop, get and process input. */
	while(true) {
		line = kdb_read_line(cmd_count++);
		if(!line) {
			kdb_printf("KDB: Please enter a command.\n");
			continue;
		}

		/* Parse the line. */
		if(!kdb_line_parse(line, &data)) {
			continue;
		}

		/* Set up the filter, if any. */
		if(data.filter_count) {
			if(data.filter_count > 1) {
				kdb_printf("KDB: TODO: Multiple filters.\n");
				continue;
			}

			/* Call the command to set up the filter. */
			filter = kdb_malloc(sizeof(kdb_filter_t));
			ret = perform_call(
				list_first(&data.filters, kdb_args_t, header),
				NULL, filter);
			if(ret != KDB_SUCCESS) {
				kdb_free(filter);
				continue;
			}
		} else {
			filter = NULL;
		}

		/* Perform the main call. */
		ret = perform_call(&data.call, filter, NULL);
		if(filter) {
			if(current_output_pos)
				kdb_putc('\n');

			filter->func(NULL, filter->data);
			kdb_free(filter);
		}

		/* Handle the return code. */
		if(ret == KDB_CONTINUE) {
			/* If we've come from fatal(), we cannot continue. */
			if(reason == KDB_REASON_FATAL) {
				kdb_printf("KDB: Continuing is not possible.\n");
			} else {
				kdb_printf("Continuing execution...\n");
				cmd_count = 0;
				ret = KDB_SUCCESS;
				break;
			}
		} else if(ret == KDB_STEP) {
			if(reason == KDB_REASON_FATAL) {
				kdb_printf("KDB: Single-step is not possible.\n");
			} else {
				break;
			}
		}
	}

	/* Run exit notifiers. */
	notifier_run_unlocked(&kdb_exit_notifier, NULL, false);

	atomic_set(&kdb_running, 0);
	local_irq_restore(state);
	return ret;
}

/**
 * Builtin commands.
 */

/** Command to display details of available commands.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		Always return KDB_SUCCESS. */
static kdb_status_t kdb_cmd_help(int argc, char **argv, kdb_filter_t *filter) {
	kdb_command_desc_t *cmd;
	kdb_args_t call;

	/* If we want a specific command, call it with --help as an argument. */
	if(argc > 1 && !kdb_help(argc, argv)) {
		if(!lookup_command(argv[1])) {
			kdb_printf("Requested help on unknown command '%s'\n", argv[1]);
			return KDB_FAILURE;
		}

		call.count = 2;
		call.args[0] = argv[1];
		call.args[1] = (char *)"--help";
		return perform_call(&call, NULL, NULL);
	}

	kdb_printf("Command       Description\n");
	kdb_printf("=======       ===========\n");

	LIST_FOREACH(&kdb_commands, iter) {
		cmd = list_entry(iter, kdb_command_desc_t, header);
		kdb_printf("%-12s  %s\n", cmd->name, cmd->description);
	}

	kdb_printf("\nFor help on a specific command, type \"help <command>\".\n");
	return KDB_SUCCESS;
}

/** Exit KDB and resume execution.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_continue(int argc, char **argv, kdb_filter_t *filter) {
	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Leaves KDB and resumes normal execution. This is not possible if KDB was\n");
		kdb_printf("entered due to a fatal error.\n");
		return KDB_SUCCESS;
	}

	return KDB_CONTINUE;
}

/** Step a certain number of instructions.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_step(int argc, char **argv, kdb_filter_t *filter) {
	uint64_t value;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<count>]\n\n", argv[0]);

		kdb_printf("Steps the given number of CPU instructions and breaks into KDB again. If\n");
		kdb_printf("no instruction count is specified, then will step one instruction. The\n");
		kdb_printf("instruction count is given as an expression.\n");

		return KDB_FAILURE;
	}

	/* If we were given a count, then use it, otherwise default to 1. */
	value = 1;
	if(argc > 1) {
		if(kdb_parse_expression(argv[1], &value, NULL) != KDB_SUCCESS) {
			return KDB_FAILURE;
		} else if(!value) {
			kdb_printf("Instruction count must be non-zero.\n");
			return KDB_FAILURE;
		}
	}

	kdb_steps_remaining = value;
	return KDB_STEP;
}

/** Reboot the system.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_reboot(int argc, char **argv, kdb_filter_t *filter) {
	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Forcibly reboots the system without safely shutting down.\n");
		return KDB_SUCCESS;
	}

	platform_reboot();
	return KDB_FAILURE;
}

/** Dump the register state.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_regs(int argc, char **argv, kdb_filter_t *filter) {
	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints out the values contained in the current CPU register set. If you wish\n");
		kdb_printf("to get the value of a single register, use the 'print' command instead.\n");

		return KDB_SUCCESS;
	}

	arch_kdb_dump_registers();
	return KDB_SUCCESS;
}

/** Examine the contents of memory.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_examine(int argc, char **argv, kdb_filter_t *filter) {
	size_t count, size, i;
	char *str, fmt;
	uint64_t val;
	ptr_t addr;
	int exp;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [/format] <expression>\n\n", argv[0]);

		kdb_printf("Prints out memory content according to the specified format. The format string\n");
		kdb_printf("is made up of an optional count followed by any of the following arguments:\n");
		kdb_printf("  Format modifiers:\n");
		kdb_printf("    x    Hexadecimal (base 16) (default).\n");
		kdb_printf("    i    Signed decimal (base 10).\n");
		kdb_printf("    u    Unsigned decimal (base 10).\n");
		kdb_printf("    o    Octal (base 8).\n");
		kdb_printf("    s    Character string.\n");
		kdb_printf("  Size specifiers:\n");
		kdb_printf("    b    Byte (8-bit) (default).\n");
		kdb_printf("    w    Word (16-bit).\n");
		kdb_printf("    d    Double-word (32-bit).\n");
		kdb_printf("    q    Quad-word (64-bit).\n");
		kdb_printf("The count specifies the number of entries to print, each of the size given by\n");
		kdb_printf("the size specifier. The value of the expression is used as the start address.\n");
		kdb_printf("For help on what can be included in expressions, use 'help print'.\n");
		kdb_printf("If the memory address is invalid, then a fault will occur, in which case the\n");
		kdb_printf("command will fail.\n");

		return KDB_SUCCESS;
	} else if(argc < 2 || (argv[1][0] == '/' && argc < 3)) {
		kdb_printf("Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Look for a format argument. */
	fmt = 'x';
	count = 0;
	size = 1;
	if(argv[1][0] == '/') {
		exp = 2;

		str = &argv[1][1];
		while(isdigit(*str))
			count = (count * 10) + (*(str++) - '0');

		/* Handle remaining format characters. */
		while(*str) {
			switch(*str) {
			case 'x': case 'i': case 'u': case 'o': case 's':
				fmt = *str;
				break;
			case 'b': size = 1; break;
			case 'w': size = 2; break;
			case 'd': size = 4; break;
			case 'q': size = 8; break;
			default:
				kdb_printf("Unknown format character '%c'\n", *str);
				return KDB_FAILURE;
			}
			str++;
		}
	} else {
		exp = 1;
	}

	/* Default value. */
	if(count == 0)
		count = 1;

	/* Parse the expression. */
	if(kdb_parse_expression(argv[exp], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;
	addr = (ptr_t)val;

	/* Check overflow. */
	if(addr != val) {
		kdb_printf("Expression value too big\n");
		return KDB_FAILURE;
	} else if((addr + (size * count)) < addr) {
		kdb_printf("Address overflow\n");
		return KDB_FAILURE;
	}

	/* Print each required entry. */
	for(i = 0; i < count; i++) {
		/* Get the value. */
		switch(size) {
		case 1:	val = (uint64_t)(*(uint8_t  *)addr); break;
		case 2:	val = (uint64_t)(*(uint16_t  *)addr); break;
		case 4:	val = (uint64_t)(*(uint32_t  *)addr); break;
		case 8:	val = (uint64_t)(*(uint64_t  *)addr); break;
		}

		/* Print it out. Don't put a newline between each value for
		 * strings. */
		switch(fmt) {
		case 'x':	kdb_printf("%p: 0x%" PRIx64 "\n", addr, val); break;
		case 'i':	kdb_printf("%p: %"   PRId64 "\n", addr, val); break;
		case 'o':	kdb_printf("%p: 0%"  PRIo64 "\n", addr, val); break;
		case 'u':	kdb_printf("%p: %"   PRIu64 "\n", addr, val); break;
		case 's':	kdb_printf("%c", (char)val); break;
		}

		addr += size;
	}

	/* Print a newline if we've just printed a string. */
	if(fmt == 's')
		kdb_printf("\n");

	return KDB_SUCCESS;
}

/** Print the value of an expression.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_print(int argc, char **argv, kdb_filter_t *filter) {
	uint64_t value;
	char fmt;
	int exp;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [/format] <expression>\n\n", argv[0]);

		kdb_printf("Prints the value of an expression in the specified format. Possible formats are:\n");
		kdb_printf("  x    Hexadecimal (base 16).\n");
		kdb_printf("  i    Signed decimal (base 10) (default).\n");
		kdb_printf("  u    Unsigned decimal (base 10).\n");
		kdb_printf("  o    Octal (base 8).\n");
		kdb_printf("The expression can be made up of constant values, registers and symbol\n");
		kdb_printf("addresses. It can also contain basic arithmetic operations (+-*/). To use the\n");
		kdb_printf("value contained in a register, prefix the name with a %% character. To use a\n");
		kdb_printf("symbol address, prefix its name with an & character.\n");

		return KDB_SUCCESS;
	} else if(argc < 2 || (argv[1][0] == '/' && argc < 3)) {
		kdb_printf("Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Look for a format argument. */
	fmt = 'i';
	if(argv[1][0] == '/') {
		exp = 2;

		switch(argv[1][1]) {
		case 'x': case 'i': case 'u': case 'o':
			fmt = argv[1][1];
			break;
		default:
			kdb_printf("Unknown format character '%c'.\n", argv[1][1]);
			return KDB_FAILURE;
		}
	} else {
		exp = 1;
	}

	/* Parse the expression and print it. */
	if(kdb_parse_expression(argv[exp], &value, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	switch(fmt) {
	case 'x': kdb_printf("0x%" PRIx64 "\n", value); break;
	case 'i': kdb_printf("%" PRId64 "\n", value); break;
	case 'o': kdb_printf("0%" PRIo64 "\n", value); break;
	case 'u': kdb_printf("%" PRIu64 "\n", value); break;
	}

	return KDB_SUCCESS;
}

/** Perform a backtrace.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_backtrace(int argc, char **argv, kdb_filter_t *filter) {
	thread_t *thread;
	uint64_t tid;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<thread ID>]\n\n", argv[0]);

		kdb_printf("Prints out a backtrace for a thread, or the current thread if no\n");
		kdb_printf("thread specified.\n");
		return KDB_SUCCESS;
	} else if(argc != 1 && argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(argc == 2) {
		if(kdb_parse_expression(argv[1], &tid, NULL) != KDB_SUCCESS) {
			return KDB_FAILURE;
		} else if(!(thread = thread_lookup_unsafe(tid))) {
			kdb_printf("Invalid thread ID.\n");
			return KDB_FAILURE;
		}

		if(thread->state == THREAD_RUNNING) {
			kdb_printf("Cannot backtrace running thread.\n");
			return KDB_FAILURE;
		}
	} else {
		thread = NULL;

		kdb_printf("--- Interrupt ---\n");
		kdb_backtrace_cb(curr_kdb_frame->ip);
		kdb_printf("--- Stacktrace ---\n");
	}

	arch_kdb_backtrace(thread, kdb_backtrace_cb);
	return KDB_SUCCESS;
}

/** Look up a symbol from an address.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_symbol(int argc, char **argv, kdb_filter_t *filter) {
	uint64_t addr;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<addr>]\n\n", argv[0]);

		kdb_printf("Look up a symbol from an address and display details about it.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	kdb_print_symbol(addr, 0);
	kdb_printf("\n");
	return KDB_SUCCESS;
}

/** Data for the wc command. */
struct wc_data {
	/** Mode that the function is running in. */
	enum {
		WC_WORDS,
		WC_LINES,
	} mode;

	size_t count;		/**< Current count. */
};

/** Function for the word count filter.
 * @param line		Line being output.
 * @param _data		Data pointer set for the filter.
 * @return		Whether to output the line. */
static bool wc_filter_func(const char *line, void *_data) {
	struct wc_data *data = (struct wc_data *)_data;
	size_t len;

	if(!line) {
		/* Command has completed, output the final count. */
		kdb_printf("%zu\n", data->count);
		kdb_free(data);
		return false;
	}

	if(data->mode == WC_WORDS) {
		len = 0;
		while(true) {
			if(!*line || isspace(*line)) {
				if(len) {
					data->count++;
					len = 0;
				}
				if(!*line)
					break;
			} else {
				len++;
			}
			line++;
		}
	} else {
		data->count++;
	}

	return false;
}

/** Perform a line or word count of the output of a command.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_wc(int argc, char **argv, kdb_filter_t *filter) {
	struct wc_data *data;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: <cmd> | %s [-l]\n\n", argv[0]);

		kdb_printf("Counts the number of words, or lines if -l is specified, in the output of a\n");
		kdb_printf("command.\n");
		return KDB_SUCCESS;
	} else if(!filter) {
		kdb_printf("This command is to be used as a filter. See 'help %s'.\n", argv[0]);
		return KDB_FAILURE;
	}

	data = kdb_malloc(sizeof(*data));
	data->mode = (argc > 1 && strcmp(argv[1], "-l") == 0) ? WC_LINES : WC_WORDS;
	data->count = 0;

	filter->func = wc_filter_func;
	filter->data = data;
	return KDB_SUCCESS;
}

/** Data for the grep command. */
struct grep_data {
	char *expr;			/**< Expression to search for. */
};

/** Function for the grep filter.
 * @param line		Line being output.
 * @param _data		Data pointer set for the filter.
 * @return		Whether to output the line. */
static bool grep_filter_func(const char *line, void *_data) {
	struct grep_data *data = (struct grep_data *)_data;

	if(!line) {
		/* Command has completed. */
		kdb_free(data);
		return false;
	}

	if(strstr(line, data->expr)) {
		return true;
	} else {
		return false;
	}
}

/** Search for matches in the output of a command.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_grep(int argc, char **argv, kdb_filter_t *filter) {
	struct grep_data *data;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: <cmd> | %s <match>\n\n", argv[0]);

		kdb_printf("Searches the output of a command for lines that contain the specified\n");
		kdb_printf("string.\n");
		return KDB_SUCCESS;
	} else if(!filter) {
		kdb_printf("This command is to be used as a filter. See 'help %s'.\n", argv[0]);
		return KDB_FAILURE;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s'.\n", argv[0]);
		return KDB_FAILURE;
	}

	data = kdb_malloc(sizeof(*data));
	data->expr = argv[1];

	filter->func = grep_filter_func;
	filter->data = data;
	return KDB_SUCCESS;
}

#if KDB_BREAKPOINT_COUNT
/** Add, remove or list breakpoints.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_break(int argc, char **argv, kdb_filter_t *filter) {
	uint64_t value;
	unsigned i;
	ptr_t addr;
	int ret;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s create <address>\n", argv[0]);
		kdb_printf("       %s list\n", argv[0]);
		kdb_printf("       %s delete <id>\n\n", argv[0]);

		kdb_printf("The first form of the command creates a new breakpoint at the given address.\n");
		kdb_printf("The address is treated as an expression. Be warned that older versions of QEMU\n");
		kdb_printf("do not support hardware breakpoints well.\n\n");

		kdb_printf("The second form lists all breakpoints that have been set up.\n\n");

		kdb_printf("The final form deletes the breakpoint with the given ID. IDs are given by\n");
		kdb_printf("the 'create' and 'list' subcommands.\n");
		return KDB_SUCCESS;
	} else if(argc < 2) {
		kdb_printf("Subcommand not given. See 'help %s' for more information.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(strcmp(argv[1], "create") == 0) {
		if(argc != 3) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		/* Get the address to add. */
		if(kdb_parse_expression(argv[2], &value, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;
		addr = (ptr_t)value;

		/* Install the breakpoint. */
		ret = arch_kdb_install_breakpoint(addr);
		if(ret < 0)
			return KDB_FAILURE;

		kdb_printf("Created breakpoint %d %pS\n", ret, addr);
	} else if(strcmp(argv[1], "list") == 0) {
		if(argc != 2) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		for(i = 0; i < KDB_BREAKPOINT_COUNT; i++) {
			if(!arch_kdb_get_breakpoint(i, &addr))
				continue;

			kdb_printf("%u: %pS\n", i, addr);
		}
	} else if(strcmp(argv[1], "delete") == 0) {
		if(argc != 3) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		i = strtoul(argv[2], NULL, 0);
		return arch_kdb_remove_breakpoint(i) ? KDB_SUCCESS : KDB_FAILURE;
	} else {
		kdb_printf("Unknown subcommand '%s'.\n", argv[1]);
		return KDB_FAILURE;
	}

	return KDB_SUCCESS;
}
#endif

#if KDB_WATCHPOINT_COUNT
/** Add, remove or list watchpoints.
 * @param argc		Number of arguments.
 * @param argv		Arguments passed to the command.
 * @param filter	Ignored.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_watch(int argc, char **argv, kdb_filter_t *filter) {
	size_t size;
	uint64_t value;
	unsigned i;
	ptr_t addr;
	bool rw;
	int ret;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s create [--rw] <address> <size>\n", argv[0]);
		kdb_printf("       %s list\n", argv[0]);
		kdb_printf("       %s delete <id>\n\n", argv[0]);

		kdb_printf("The first form of the command creates a new watchpoint at the given address.\n");
		kdb_printf("The address is treated as an expression. If the '--rw' argument is given, the\n");
		kdb_printf("watchpoint will trigger on reads and writes, rather than just writes. Be\n");
		kdb_printf("warned that older versions of QEMU do not support hardware watchpoints well.\n\n");

		kdb_printf("The second form lists all watchpoints that have been set up.\n\n");

		kdb_printf("The final form deletes the watchpoint with the given ID. IDs are given by\n");
		kdb_printf("the 'create' and 'list' subcommands.\n");
		return KDB_SUCCESS;
	} else if(argc < 2) {
		kdb_printf("Subcommand not given. See 'help %s' for more information.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(strcmp(argv[1], "create") == 0) {
		if(argc != 4 && argc != 5) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		/* Get the read-write argument. */
		i = 2;
		rw = false;
		if(strcmp(argv[2], "--rw") == 0) {
			rw = true;
			i++;
		}

		/* Get the address to add. */
		if(kdb_parse_expression(argv[i++], &value, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;
		addr = (ptr_t)value;

		/* Get the size. */
		if(kdb_parse_expression(argv[i++], &value, NULL) != KDB_SUCCESS)
			return KDB_FAILURE;
		size = (size_t)value;

		/* Install the watchpoint. */
		ret = arch_kdb_install_watchpoint(addr, size, rw);
		if(ret < 0)
			return KDB_FAILURE;

		kdb_printf("Created %zu byte %swrite watchpoint %d %pS\n",
			size, (rw) ? "read-" : "", ret, addr);
	} else if(strcmp(argv[1], "list") == 0) {
		if(argc != 2) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		for(i = 0; i < KDB_WATCHPOINT_COUNT; i++) {
			if(!arch_kdb_get_watchpoint(i, &addr, &size, &rw))
				continue;

			kdb_printf("%u: %zu byte %swrite %pS\n", i, size,
				(rw) ? "read-" : "", addr);
		}
	} else if(strcmp(argv[1], "delete") == 0) {
		if(argc != 3) {
			kdb_printf("Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
			return KDB_FAILURE;
		}

		i = strtoul(argv[2], NULL, 0);
		return arch_kdb_remove_watchpoint(i) ? KDB_SUCCESS : KDB_FAILURE;
	} else {
		kdb_printf("Unknown subcommand '%s'.\n", argv[1]);
		return KDB_FAILURE;
	}

	return KDB_SUCCESS;
}
#endif

/**
 * Command registration.
 */

/** Register a new debugger command.
 * @param name		Name of the command to register.
 * @param description	Short (one-line) description of the command used in
 *			help text.
 * @param func		Function implementing the command. */
void kdb_register_command(const char *name, const char *description, kdb_command_t func) {
	kdb_command_desc_t *cmd, *exist;
	list_t *pos;
	int ret;

	spinlock_lock(&kdb_commands_lock);

	cmd = kdb_malloc(sizeof(*cmd));
	list_init(&cmd->header);
	cmd->name = name;
	cmd->description = description;
	cmd->func = func;

	/* Keep the command list sorted alphabetically. */
	pos = kdb_commands.next;
	while(pos != &kdb_commands) {
		exist = list_entry(pos, kdb_command_desc_t, header);

		ret = strcmp(name, exist->name);
		if(ret == 0) {
			kdb_free(cmd);
			spinlock_unlock(&kdb_commands_lock);
			return;
		} else if(ret < 0) {
			break;
		}

		pos = pos->next;
	}

	list_add_before(pos, &cmd->header);

	spinlock_unlock(&kdb_commands_lock);
}

/** Unregister a debugger command.
 * @param name		Name of the command to remove. */
void kdb_unregister_command(const char *name) {
	kdb_command_desc_t *cmd;

	spinlock_lock(&kdb_commands_lock);

	LIST_FOREACH(&kdb_commands, iter) {
		cmd = list_entry(iter, kdb_command_desc_t, header);

		if(strcmp(name, cmd->name) == 0) {
			list_remove(&cmd->header);
			kdb_free(cmd);
			return;
		}
	}

	spinlock_unlock(&kdb_commands_lock);
}

/** Initialize the kernel debugger. */
__init_text void kdb_init(void) {
	/* Initialize the heap. */
	fixed_heap_init(&kdb_heap, kdb_heap_area, KDB_HEAP_SIZE);

	/* Register architecture-specific commands. */
	arch_kdb_init();

	/* Register our own commands. */
	kdb_register_command("help", "Display usage information for KDB commands.", kdb_cmd_help);
	kdb_register_command("continue", "Exit KDB and continue execution.", kdb_cmd_continue);
	kdb_register_command("step", "Single-step over instructions.", kdb_cmd_step);
	kdb_register_command("reboot", "Forcibly reboot the system.", kdb_cmd_reboot);
	kdb_register_command("regs", "Print the values of all CPU registers.", kdb_cmd_regs);
	kdb_register_command("examine", "Examine the contents of memory.", kdb_cmd_examine);
	kdb_register_command("print", "Print out the value of an expression.", kdb_cmd_print);
	kdb_register_command("backtrace", "Print out a backtrace.", kdb_cmd_backtrace);
	kdb_register_command("symbol", "Look up a symbol from an address.", kdb_cmd_symbol);
	kdb_register_command("wc", "Count words or lines in the output of a command.", kdb_cmd_wc);
	kdb_register_command("grep", "Search the output of commands.", kdb_cmd_grep);
	#if KDB_BREAKPOINT_COUNT
	kdb_register_command("break", "Create, list and remove breakpoints.", kdb_cmd_break);
	#endif
	#if KDB_WATCHPOINT_COUNT
	kdb_register_command("watch", "Create, list and remove watchpoints.", kdb_cmd_watch);
	#endif
}
