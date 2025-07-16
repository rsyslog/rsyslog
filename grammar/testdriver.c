/* This is a stand-alone test driver for grammar processing. We try to
 * keep this separate as it simplyfies grammer development.
 *
 * Copyright 2011 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libestr.h>
#include "rainerscript.h"
#include "parserif.h"

extern int yylineno;
int Debug = 1;

void parser_errmsg(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("error on or before line %d: ", yylineno);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

int yyerror(char *s) {
    parser_errmsg("%s", s);
    return 0;
}

void dbgprintf(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

void cnfDoObj(struct cnfobj *o) {
    dbgprintf("global:obj: ");
    cnfobjPrint(o);
    cnfobjDestruct(o);
}

void cnfDoRule(struct cnfrule *rule) {
    dbgprintf("global:rule processed\n");
    cnfrulePrint(rule);
}

void cnfDoCfsysline(char *ln) {
    dbgprintf("global:cfsysline: %s\n", ln);
}

void cnfDoBSDTag(char *ln) {
    dbgprintf("global:BSD tag: %s\n", ln);
}

void cnfDoBSDHost(char *ln) {
    dbgprintf("global:BSD host: %s\n", ln);
}

int main(int argc, char *argv[]) {
    int r;

    cnfSetLexFile(argc == 1 ? NULL : argv[1]);
    yydebug = 0;
    r = yyparse();
    printf("yyparse() returned %d\n", r);
    return r;
}
