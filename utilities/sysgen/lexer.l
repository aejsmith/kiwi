/*
 * Copyright (C) 2009-2022 Alex Smith
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
