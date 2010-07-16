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
#include "krpcgen.h"

#define YYERROR_VERBOSE
#define YYDEBUG 1

static void yyerror(const char *msg);

%}

%union {
	char *str;
	unsigned long num;
	variable_t *var;
}

%token TOK_SERVICE
%token TOK_VERSION
%token TOK_TYPE
%token TOK_STRUCT
%token TOK_FUNCTION
%token TOK_EVENT
%token TOK_OUT

%token <str> TOK_NAMESPACE;
%token <str> TOK_IDENTIFIER;
%token <num> TOK_NUMBER;

%type <var> struct_body;
%type <var> struct_entry;
%type <var> func_param_list;
%type <var> func_param;
%type <var> event_param_list;
%type <var> event_param;

%start input

%%

input
	: statement input
	| /* Empty. */
	;

statement
	: service_stmt ';'
	| version_stmt ';'
	| type_stmt ';'
	| struct_stmt ';'
	| function_stmt ';'
	| event_stmt ';'
	;

service_stmt
	: TOK_SERVICE TOK_NAMESPACE				{ set_service_name($2); }
	;

version_stmt
	: TOK_VERSION TOK_NUMBER				{ set_service_version($2); }
	;

type_stmt
	: TOK_TYPE TOK_IDENTIFIER TOK_IDENTIFIER		{ add_type($2, $3); }
	;

struct_stmt
	: TOK_STRUCT TOK_IDENTIFIER '{' struct_body '}'		{ add_struct($2, $4); }
	;

struct_body
	: struct_entry struct_body				{ $$ = $1; $$->next = $2; }
	| struct_entry						{ $$ = $1; }
	;

struct_entry
	: TOK_IDENTIFIER TOK_IDENTIFIER ';'			{ $$ = new_variable($2, $1, false); }
	;

function_stmt
	: TOK_FUNCTION TOK_IDENTIFIER '(' func_param_list ')'	{ add_function($2, $4); }
	| TOK_FUNCTION TOK_IDENTIFIER '(' ')'			{ add_function($2, NULL); }
	;

func_param_list
	: func_param ',' func_param_list			{ $$ = $1; $$->next = $3; }
	| func_param						{ $$ = $1; }
	;

func_param
	: TOK_IDENTIFIER TOK_IDENTIFIER				{ $$ = new_variable($2, $1, false); }
	| TOK_OUT TOK_IDENTIFIER TOK_IDENTIFIER			{ $$ = new_variable($3, $2, true); }
	;

event_stmt
	: TOK_EVENT TOK_IDENTIFIER '(' event_param_list ')'	{ add_event($2, $4); }
	;

event_param_list
	: event_param ',' event_param_list			{ $$ = $1; $$->next = $3; }
	| event_param						{ $$ = $1; }
	;

event_param
	: TOK_IDENTIFIER TOK_IDENTIFIER				{ $$ = new_variable($2, $1, false); }
	;

%%

static void yyerror(const char *msg) {
	fprintf(stderr, "%s:%zu: %s\n", current_file, current_line, msg);
	exit(EXIT_FAILURE);
}
