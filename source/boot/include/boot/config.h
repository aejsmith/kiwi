/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Bootloader configuration functions.
 */

#ifndef __BOOT_CONFIG_H
#define __BOOT_CONFIG_H

#include <lib/list.h>

struct value;
struct command;

/** Structure containing a list of commands. */
typedef list_t command_list_t;

/** Structure containing a list of values. */
typedef struct value_list {
	struct value *values;		/**< Array of values. */
	size_t count;			/**< Number of arguments. */
} value_list_t;

/** Structure containing a value used in the configuration.  */
typedef struct value {
	/** Type of the value. */
	enum {
		/** Types that can be set from the configuration file. */
		VALUE_TYPE_INTEGER,		/**< Integer. */
		VALUE_TYPE_BOOLEAN,		/**< Boolean. */
		VALUE_TYPE_STRING,		/**< String. */
		VALUE_TYPE_LIST,		/**< List. */
		VALUE_TYPE_COMMAND_LIST,	/**< Command list. */

		/** Types used internally. */
		VALUE_TYPE_POINTER,		/**< Pointer. */
	} type;

	/** Actual value. */
	union {
		int integer;			/**< Integer. */
		bool boolean;			/**< Boolean. */
		char *string;			/**< String. */
		value_list_t *list;		/**< List. */
		command_list_t *cmds;		/**< Command list. */
		void *pointer;			/**< Pointer. */
	};
} value_t;

/** Structure containing an environment. */
typedef list_t environ_t;

/** Structure describing a command that can be used in a command list. */
typedef struct command {
	const char *name;		/**< Name of the command. */

	/** Execute the command.
	 * @param args		List of arguments.
	 * @param env		Environment to execute the command in.
	 * @return		Whether the command completed successfully. */
	bool (*func)(value_list_t *args, environ_t *env);
} command_t;

extern char *config_file_override;
extern environ_t *root_environ;

extern bool command_list_exec(command_list_t *list, command_t *commands, int count, environ_t *env);

extern void value_list_insert(value_list_t *list, value_t *value);

extern environ_t *environ_create(void);
extern value_t *environ_lookup(environ_t *env, const char *name);
extern void environ_insert(environ_t *env, const char *name, value_t *value);

extern bool config_cmd_set(value_list_t *args, environ_t *env);

extern void config_init(void);

#endif /* __BOOT_CONFIG_H */
