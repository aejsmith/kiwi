/*
 * Copyright (C) 2010 Alex Smith
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
