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
 * Copyright (C) 2009-2012 by Rainer Gerhards and Adiscon GmbH
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
