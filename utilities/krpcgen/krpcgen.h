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
 * @brief		Kiwi RPC interface compiler.
 */

#ifndef __KRPCGEN_H
#define __KRPCGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

/** Structure used to represent a variable during parsing. */
typedef struct variable {
	struct variable *next;		/**< Next variable in the list. */
	char *type;			/**< Type name for the variable. */
	char *name;			/**< Name of the variable. */
	bool out;			/**< Whether this is an output variable. */
	size_t line;			/**< Line that the variable was declared on. */
} variable_t;

extern FILE *yyin;
extern const char *current_file;
extern size_t current_line;

extern variable_t *new_variable(const char *name, const char *type, bool out);

extern void set_service_name(const char *name);
extern void set_service_version(unsigned long ver);
extern void add_type(const char *name, const char *target);
extern void add_struct(const char *name, variable_t *entries);
extern void add_function(const char *name, variable_t *params);
extern void add_event(const char *name, variable_t *params);

extern int yylex(void);
extern int yyparse(void);

#ifdef __cplusplus
}
#endif

#endif /* __KRPCGEN_H */
