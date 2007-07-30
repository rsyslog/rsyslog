/*! \file srUtils.h
 *  \brief General, small utilities that fit nowhere else.
 * 
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-09-09
 *          Coding begun.
 *
 * Copyright 2003-2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#ifndef __SRUTILS_H_INCLUDED__
#define __SRUTILS_H_INCLUDED__ 1

/**
 * A reimplementation of itoa(), as this is not available
 * on all platforms. We used the chance to make an interface
 * that fits us well, so it is no longer plain itoa().
 *
 * This method works with the US-ASCII alphabet. If you port this
 * to e.g. EBCDIC, you need to make a small adjustment. Keep in mind,
 * that on the wire it MUST be US-ASCII, so basically all you need
 * to do is replace the constant '0' with 0x30 ;).
 *
 * \param pBuf Caller-provided buffer that will receive the
 *              generated ASCII string.
 *
 * \param iLenBuf Length of the caller-provided buffer.
 *
 * \param iToConv The integer to be converted.
 */
rsRetVal srUtilItoA(char *pBuf, int iLenBuf, int iToConv);

/**
 * A method to duplicate a string for which the length is known.
 * Len must be the length in characters WITHOUT the trailing
 * '\0' byte.
 * rgerhards, 2007-07-10
 */
unsigned char *srUtilStrDup(unsigned char *pOld, size_t len);
/**
 * A method to create a directory and all its missing parents for
 * a given file name. Please not that the rightmost element is
 * considered to be a file name and thus NO directory is being created
 * for it.
 * added 2007-07-17 by rgerhards
 */
int makeFileParentDirs(uchar *szFile, size_t lenFile, mode_t mode, uid_t uid, gid_t gid, int bFailOnChown);

int execProg(uchar *program, int wait, uchar *arg);
void skipWhiteSpace(uchar **pp);
#endif
