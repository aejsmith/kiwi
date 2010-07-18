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

%{

#include <stdlib.h>
#include "sysgen.h"

#define YYERROR_VERBOSE
#define YYDEBUG 1

static void yyerror(const char *msg);

%}

%union {
	char *str;
	unsigned long num;
	parameter_t *param;
}

%token TOK_SERVICE
%token TOK_TYPE

%token <str> TOK_IDENTIFIER;
%token <num> TOK_NUMBER;

%type <param> parameter_list;

%start input

%%

input
	: statement input
	| /* Empty. */
	;

statement
	: service_stmt ';'
	| type_stmt ';'
	| syscall_stmt ';'
	;

service_stmt
	: TOK_SERVICE TOK_NUMBER
		{ set_service_number($2); }
	;

type_stmt
	: TOK_TYPE TOK_IDENTIFIER TOK_IDENTIFIER
		{ add_type($2, $3); }
	;

syscall_stmt
	: TOK_IDENTIFIER TOK_IDENTIFIER '(' parameter_list ')' '=' TOK_NUMBER
		{ add_syscall($1, $2, $4, $7); }
	| TOK_IDENTIFIER TOK_IDENTIFIER '(' ')' '=' TOK_NUMBER
		{ add_syscall($1, $2, NULL, $6); }
	| TOK_IDENTIFIER TOK_IDENTIFIER '(' parameter_list ')'
		{ add_syscall($1, $2, $4, -1); }
	| TOK_IDENTIFIER TOK_IDENTIFIER '(' ')'
		{ add_syscall($1, $2, NULL, -1); }
	;

parameter_list
	: TOK_IDENTIFIER ',' parameter_list
		{ $$ = new_parameter($1, $3); }
	| TOK_IDENTIFIER
		{ $$ = new_parameter($1, NULL); }
	;

%%

static void yyerror(const char *msg) {
	fprintf(stderr, "%s:%zu: %s\n", current_file, current_line, msg);
	exit(EXIT_FAILURE);
}
