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

/** Structure used to represent a statement during parsing. */
typedef struct statement {
	struct statement *next;		/**< Next statement in the list. */
	size_t line;			/**< Line the statement finished on. */

	/** Type of the statement. */
	enum {
		STATEMENT_SERVICE,
		STATEMENT_TYPE,
		STATEMENT_STRUCT,
		STATEMENT_FUNCTION,
		STATEMENT_EVENT,
	} type;

	/** Data for the statement. */
	union {
		struct {
			char *name;
			unsigned long version;
			struct statement *stmts;
		} service;
		struct {
			char *name;
			char *target;
		} type;
		struct {
			char *name;
			variable_t *entries;
		} struc;
		struct {
			char *name;
			variable_t *params;
		} function;
	} data;
} statement_t;

extern FILE *yyin;
extern const char *current_file;
extern size_t current_line;

extern variable_t *new_variable(const char *name, const char *type, bool out);

extern statement_t *new_service_stmt(const char *name, unsigned long ver, statement_t *stmts);
extern statement_t *new_type_stmt(const char *name, const char *target);
extern statement_t *new_struct_stmt(const char *name, variable_t *entries);
extern statement_t *new_function_stmt(const char *name, variable_t *params);
extern statement_t *new_event_stmt(const char *name, variable_t *params);

extern void set_service(statement_t *stmt);

extern int yylex(void);
extern int yyparse(void);

#ifdef __cplusplus
}
#endif

#endif /* __KRPCGEN_H */
