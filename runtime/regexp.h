/* The regexp object. It encapsulates the C regexp functionality. The primary
 * purpose of this wrapper class is to enable rsyslogd core to be build without
 * regexp libraries.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_REGEXP_H
#define INCLUDED_REGEXP_H

#include <regex.h>

/* interfaces */
BEGINinterface(regexp) /* name must also be changed in ENDinterface macro! */
	int (*regcomp)(regex_t *preg, const char *regex, int cflags);
	int (*regexec)(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
	size_t (*regerror)(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);
	void (*regfree)(regex_t *preg);
ENDinterface(regexp)
#define regexpCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(regexp);

/* the name of our library binary */
#define LM_REGEXP_FILENAME "lmregexp"

#endif /* #ifndef INCLUDED_REGEXP_H */
