/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

%option noyywrap
%{

#include <stdlib.h>
#include "sysgen.h"
#include "parser.h"

%}

%%

"type"                  { return TOK_TYPE; }
"syscall"               { return TOK_SYSCALL; }

[A-Za-z_][A-Za-z0-9_]*  { yylval.str = strdup(yytext); return TOK_IDENTIFIER; }
[0-9]+                  { yylval.num = strtoul(yytext, NULL, 10); return TOK_NUMBER; }

#.*$                    { /* Comment, do nothing. */ }
[ \t]                   { /* Whitespace, do nothing. */ }
\n                      { current_line++; }
.                       { return *yytext; }

<<EOF>>                 { return EOF; }

%%
