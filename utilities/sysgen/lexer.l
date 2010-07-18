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

%option noyywrap
%{

#include <stdlib.h>
#include "sysgen.h"
#include "parser.h"

%}

%%

"service"		{ return TOK_SERVICE; }
"type"			{ return TOK_TYPE; }

[A-Za-z_][A-Za-z0-9_]*	{ yylval.str = strdup(yytext); return TOK_IDENTIFIER; }
[0-9]+			{ yylval.num = strtoul(yytext, NULL, 10); return TOK_NUMBER; }

#.*$			{ /* Comment, do nothing. */ }
[ \t]			{ /* Whitespace, do nothing. */ }
\n			{ current_line++; }
.			{ return *yytext; }

<<EOF>>			{ return EOF; }

%%
