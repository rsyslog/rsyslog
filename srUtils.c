/**\file srUtils.c
 * \brief General utilties that fit nowhere else.
 *
 * The namespace for this file is "srUtil".
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-09-09
 *          Coding begun.
 *
 * Copyright 2002-2003 
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Adiscon GmbH or Rainer Gerhards
 *       nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

srRetVal srUtilItoA(char *pBuf, int iLenBuf, int iToConv)
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
		return SR_RET_PROVIDED_BUFFER_TOO_SMALL;

	/* then move it to the right direction... */
	if(bIsNegative == TRUE)
		*pBuf++ = '-';
	while(i >= 0)
		*pBuf++ = szBuf[i--];
	*pBuf = '\0';	/* terminate it!!! */

	return SR_RET_OK;
}
