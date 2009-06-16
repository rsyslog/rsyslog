/* This is the header file for unicode support.
 * 
 * Currently, this is a dummy module. 
 * The following functions are wrappers which hopefully enable us to move
 * from 8-bit chars to unicode with relative ease when we finally attack this
 *
 * Note: while we prefer inline functions, this leads to invalid references in
 * core dumps. So in a debug build, we use macros where appropriate...
 *
 * Begun 2009-05-21 RGerhards
 *
 * Copyright (C) 2009 by Rainer Gerhards and Adiscon GmbH
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
#ifndef INCLUDED_UNICODE_HELPER_H
#define INCLUDED_UNICODE_HELPER_H

#include <string.h>

#ifdef DEBUG
#	define ustrncpy(psz1, psz2, len) strncpy((char*)(psz1), (char*)(psz2), (len))
#	define ustrdup(psz) (uchar*)strdup((char*)(psz))
#else
	static inline uchar* ustrncpy(uchar *psz1, uchar *psz2, size_t len)
	{
		return (uchar*) strncpy((char*) psz1, (char*) psz2, len);
	}

	static inline uchar* ustrdup(uchar *psz)
	{
		return (uchar*) strdup((char*)psz);
	}

#endif /* #ifdef DEBUG */

static inline int ustrcmp(uchar *psz1, uchar *psz2)
{
	return strcmp((char*) psz1, (char*) psz2);
}

static inline int ustrlen(uchar *psz)
{
	return strlen((char*) psz);
}


#define UCHAR_CONSTANT(x) ((uchar*) (x))
#define CHAR_CONVERT(x) ((char*) (x))

#endif /* multi-include protection */
/* vim:set ai:
 */
