/*
    klogd.h - main header file for Linux kernel log daemon.
    Copyright (c) 1995  Dr. G.W. Wettstein <greg@wind.rmcc.com>

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

/*
 * Symbols and definitions needed by klogd.
 *
 * Thu Nov 16 12:45:06 CST 1995:  Dr. Wettstein
 *	Initial version.
 */

/* Useful include files. */
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#undef syslog
#undef vsyslog

/* Function prototypes. */
extern int InitKsyms(char *);
extern int InitMsyms(void);
extern char * ExpandKadds(char *, char *);
extern void SetParanoiaLevel(int);
extern void Syslog(int priority, char *fmt, ...);
extern void vsyslog(int pri, const char *fmt, va_list ap);
extern void openlog(const char *ident, int logstat, int logfac);
