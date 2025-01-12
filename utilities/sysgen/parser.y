/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    identifier_t *ident;
}

%token TOK_TYPE
%token TOK_SYSCALL

%token <str> TOK_IDENTIFIER;
%token <num> TOK_NUMBER;

%type <ident> parameter_list;
%type <ident> attribute_list;

%start input

%%

input
    : statement input
    | /* Empty. */
    ;

statement
    : type_stmt ';'
    | syscall_stmt ';'
    ;

type_stmt
    : TOK_TYPE TOK_IDENTIFIER TOK_IDENTIFIER
        { add_type($2, $3); }
    ;

/* This is a bit crap, could probably improve it. */
syscall_stmt
    : TOK_SYSCALL TOK_IDENTIFIER '(' parameter_list ')' attribute_list '=' TOK_NUMBER
        { add_syscall($2, $4, $6, $8); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' parameter_list ')' '=' TOK_NUMBER
        { add_syscall($2, $4, NULL, $7); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' ')' attribute_list '=' TOK_NUMBER
        { add_syscall($2, NULL, $5, $7); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' ')' '=' TOK_NUMBER
        { add_syscall($2, NULL, NULL, $6); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' parameter_list ')' attribute_list
        { add_syscall($2, $4, $6, -1); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' parameter_list ')'
        { add_syscall($2, $4, NULL, -1); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' ')' attribute_list
        { add_syscall($2, NULL, $5, -1); }
    | TOK_SYSCALL TOK_IDENTIFIER '(' ')'
        { add_syscall($2, NULL, NULL, -1); }
    ;

parameter_list
    : TOK_IDENTIFIER ',' parameter_list
        { $$ = new_identifier($1, $3); }
    | TOK_IDENTIFIER
        { $$ = new_identifier($1, NULL); }
    ;

attribute_list
    : TOK_IDENTIFIER attribute_list
        { $$ = new_identifier($1, $2); }
    | TOK_IDENTIFIER
        { $$ = new_identifier($1, NULL); }
    ;

%%

static void yyerror(const char *msg) {
    fprintf(stderr, "%s:%zu: %s\n", current_file, current_line, msg);
    exit(EXIT_FAILURE);
}
