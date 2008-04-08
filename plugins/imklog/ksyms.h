/*  ksym.h - Definitions for symbol table utilities.
 *  Copyright (c) 1995, 1996  Dr. G.W. Wettstein <greg@wind.rmcc.com>
 *  Copyright (c) 1996 Enjellic Systems Development
 *  Copyright (c) 2004-7 Martin Schulze <joey@infodrom.org>
 *  Copyright (c) 2007-2008 Rainer Gerhards <rgerhards@adiscon.com>
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

/* Variables, structures and type definitions static to this module. */

struct symbol
{
	char *name;
	int size;
	int offset;
};


/* Function prototypes. */
extern char * LookupSymbol(unsigned long, struct symbol *);
extern char * LookupModuleSymbol(unsigned long int, struct symbol *);
