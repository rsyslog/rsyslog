/**\file srUtils.c
 * \brief General utilties that fit nowhere else.
 *
 * The namespace for this file is "srUtil".
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
#include "config.h"


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "rsyslog.h"	/* THIS IS A MODIFICATION FOR RSYSLOG! 2004-11-18 rgerards */
#include "liblogging-stub.h"	/* THIS IS A MODIFICATION FOR RSYSLOG! 2004-11-18 rgerards */
#define TRUE 1
#define FALSE 0
#include "srUtils.h"
#include "assert.h"


/* ################################################################# *
 * private members                                                   *
 * ################################################################# */

/* As this is not a "real" object, there won't be any private
 * members in this file.
 */

/* ################################################################# *
 * public members                                                    *
 * ################################################################# */

rsRetVal srUtilItoA(char *pBuf, int iLenBuf, int iToConv)
{
	int i;
	int bIsNegative;
	char szBuf[32];	/* sufficiently large for my lifespan and those of my children... ;) */

	assert(pBuf != NULL);
	assert(iLenBuf > 1);	/* This is actually an app error and as thus checked for... */

	if(iToConv < 0)
	{
		bIsNegative = TRUE;
		iToConv *= -1;
	}
	else
		bIsNegative = FALSE;

	/* first generate a string with the digits in the reverse direction */
	i = 0;
	do
	{
		szBuf[i] = iToConv % 10 + '0';
		iToConv /= 10;
	} while(iToConv > 0);	/* warning: do...while()! */

	/* make sure we are within bounds... */
	if(i + 2 > iLenBuf)	/* +2 because: a) i starts at zero! b) the \0 byte */
		return RS_RET_PROVIDED_BUFFER_TOO_SMALL;

	/* then move it to the right direction... */
	if(bIsNegative == TRUE)
		*pBuf++ = '-';
	while(i >= 0)
		*pBuf++ = szBuf[i--];
	*pBuf = '\0';	/* terminate it!!! */

	return RS_RET_OK;
}

uchar *srUtilStrDup(uchar *pOld, size_t len)
{
	uchar *pNew;

	assert(pOld != NULL);
	assert(len >= 0);
	
	if((pNew = malloc(len + 1)) != NULL)
		memcpy(pNew, pOld, len + 1);

	return pNew;
}


/* creates a path recursively
 * Return 0 on success, -1 otherwise. On failure, errno
 * hold the last OS error.
 * Param "mode" holds the mode that all non-existing directories
 * are to be created with.
 */
int makeFileParentDirs(uchar *szFile, size_t lenFile, mode_t mode)
{
        uchar *p;
        uchar *pszWork;
        size_t len;

	assert(szFile != NULL);
	assert(len > 0);

        len = strlen(szFile) + 1; /* add one for '\0'-byte */
	if((pszWork = malloc(sizeof(uchar) * len)) == NULL)
		return -1;
        memcpy(pszWork, szFile, len);
        for(p = pszWork+1 ; *p ; p++)
                if(*p == '/') {
			/* temporarily terminate string, create dir and go on */
                        *p = '\0';
                        if(access(pszWork, F_OK))
                                if(mkdir(pszWork, mode) != 0) {
					int eSave = errno;
					free(pszWork);
					errno = eSave;
					return -1;
				}
                        *p = '/';
                }
	free(pszWork);
}
