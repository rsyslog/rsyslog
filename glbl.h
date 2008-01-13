/* Definition of globally-accessible data items.
 *
 * This module provides access methods to items of global scope. Most often,
 * these globals serve as defaults to initialize local settings. Currently,
 * many of them are either constants or global variable references. However,
 * this module provides the necessary hooks to change that at any time.
 *
 * Please note that there currently is no glbl.c file as we do not yet
 * have any implementations.
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

#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED

#define glblGetIOBufSize() 4096 /* size of the IO buffer, e.g. for strm class */

extern uchar *pszWorkDir;
#define glblGetWorkDir() (pszWorkDir == NULL ? (uchar*) "" : pszWorkDir)

#endif /* #ifndef GLOBALS_H_INCLUDED */
