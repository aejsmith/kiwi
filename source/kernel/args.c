/* Kiwi kernel argument functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Kernel argument functions.
 */

#include <console/kprintf.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <args.h>

/** Structure containing an argument. */
typedef struct arg {
	const char *name;		/**< Name of argument. */
	const char *val;		/**< Argument value. */
} arg_t;

/** Array of kernel arguments. */
static arg_t *args_array = NULL;
static size_t args_count = 0;

/** Get the index of an argument in the argument array.
 * @param name		Name of argument to get.
 * @return		Index of argument or -1 if not found. */
static int args_get_internal(const char *name) {
	size_t i;

	for(i = 0; i < args_count; i++) {
		if(strcmp(args_array[i].name, name) == 0) {
			return (int)i;
		}
	}

	return -1;
}

/** Get a kernel argument.
 *
 * Gets the value of a kernel command line argument.
 *
 * @param name		Name of argument to get.
 *
 * @return		Argument value, or NULL if argument not found. The
 *			string may be zero-length, if only the name was
 *			specified on the command line.
 */
const char *args_get(const char *name) {
	int i;
	return ((i = args_get_internal(name)) >= 0) ? args_array[i].val : NULL;
}

/** Initialise the kernel argument system.
 * @param cmdline	Kernel command line. */
void __init_text args_init(const char *cmdline) {
	char *tok, *dup, *name, *val;
	int i;

	/* Duplicate command line and skip over the kernel path. */
	dup = kstrdup(cmdline, MM_FATAL);
	tok = strsep(&dup, " ");

	while((tok = strsep(&dup, " "))) {
		name = tok;
		if((val = strchr(name, '='))) {
			*(val++) = 0;
		} else {
			/* Point it at the end of the string if no = character,
			 * so it will just be zero-length. */
			val = name + strlen(name);
		}

		/* If this argument has already been set overwrite it. */
		if((i = args_get_internal(name)) >= 0) {
			args_array[i].val = val;
		} else {
			args_array = krealloc(args_array, sizeof(arg_t) * (args_count + 1), MM_FATAL);
			args_array[args_count].name = name;
			args_array[args_count].val = val;
			args_count++;
		}
	}
}
