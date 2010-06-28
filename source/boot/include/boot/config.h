/*
 * Copyright (C) 2010 Alex Smith
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
	int count;			/**< Number of arguments. */
} value_list_t;

/** Structure containing a value used in the configuration.  */
typedef struct value {
	/** Type of the value. */
	enum {
		/** Types that can be set from the configuration file. */
		VALUE_TYPE_INTEGER,		/**< Integer. */
		VALUE_TYPE_STRING,		/**< String. */
		VALUE_TYPE_LIST,		/**< List. */
		VALUE_TYPE_COMMAND_LIST,	/**< Command list. */

		/** Types used internally. */
		VALUE_TYPE_POINTER,		/**< Pointer. */
	} type;

	/** Actual value. */
	union {
		int integer;			/**< Integer. */
		char *string;			/**< String. */
		value_list_t *list;		/**< List. */
		command_list_t *cmds;		/**< Command list. */
		void *pointer;			/**< Pointer. */
	};
} value_t;

/** Structure containing an environment. */
typedef list_t environ_t;

extern char *config_file_override;

extern value_t *environ_lookup(const char *name);
extern void environ_insert(const char *name, value_t *value);

extern void config_init(void);

#endif /* __BOOT_CONFIG_H */
