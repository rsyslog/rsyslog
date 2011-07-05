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
#include <libestr.h>
#include "parserif.h"

void
cstrPrint(char *text, es_str_t *estr)
{
	char *str;
	str = es_str2cstr(estr, NULL);
	printf("%s%s", text, str);
	free(str);
}


int
main(int argc, char *argv[])
{
	int r;

	cnfSetLexFile(argc == 1 ? NULL : argv[1]);
	yydebug = 0;
	r = yyparse();
	printf("yyparse() returned %d\n", r);
	return r;
}
