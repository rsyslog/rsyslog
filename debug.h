/* debug.h
 *
 * Definitions for the debug and run-time analysis support module.
 * Contains a lot of macros.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

/* external static data elements (some time to be replaced) */
extern int Debug;		/* debug flag  - read-only after startup */
extern int debugging_on;	 /* read-only, except on sig USR1 */

/* prototypes */
void sigsegvHdlr(int signum);
void dbgprintf(char *fmt, ...) __attribute__((format(printf,1, 2)));

#endif /* #ifndef DEBUG_H_INCLUDED */
