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

#include "rsyslog.h"	/* THIS IS A MODIFICATION FOR RSYSLOG! 2004-11-18 rgerards */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <wait.h>
#include <ctype.h>
#include "liblogging-stub.h"	/* THIS IS A MODIFICATION FOR RSYSLOG! 2004-11-18 rgerards */
#define TRUE 1
#define FALSE 0
#include "srUtils.h"
#include "syslogd.h"


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
int makeFileParentDirs(uchar *szFile, size_t lenFile, mode_t mode,
		       uid_t uid, gid_t gid, int bFailOnChownFail)
{
        uchar *p;
        uchar *pszWork;
        size_t len;
	int bErr = 0;

	assert(szFile != NULL);
	assert(lenFile > 0);

        len = lenFile + 1; /* add one for '\0'-byte */
	if((pszWork = malloc(sizeof(uchar) * len)) == NULL)
		return -1;
        memcpy(pszWork, szFile, len);
        for(p = pszWork+1 ; *p ; p++)
                if(*p == '/') {
			/* temporarily terminate string, create dir and go on */
                        *p = '\0';
                        if(access((char*)pszWork, F_OK)) {
                                if(mkdir((char*)pszWork, mode) == 0) {
					if(uid != (uid_t) -1 || gid != (gid_t) -1) {
						/* we need to set owner/group */
						if(chown((char*)pszWork, uid, gid) != 0)
							if(bFailOnChownFail)
								bErr = 1;
							/* silently ignore if configured
							 * to do so.
							 */
					}
				} else
					bErr = 1;
				if(bErr) {
					int eSave = errno;
					free(pszWork);
					errno = eSave;
					return -1;
				}
			}
                        *p = '/';
                }
	free(pszWork);
	return 0;
}


/* execute a program with a single argument
 * returns child pid if everything ok, 0 on failure. if
 * it fails, errno is set. if it fails after the fork(), the caller
 * can not be notfied for obvious reasons. if bwait is set to 1,
 * the code waits until the child terminates - that potentially takes
 * a lot of time.
 * implemented 2007-07-20 rgerhards
 */
int execProg(uchar *program, int bWait, uchar *arg)
{
        int pid;
	int sig;

	dbgprintf("exec program '%s' with param '%s'\n", program, arg);
        pid = fork();
        if (pid < 0) {
                return 0;
        }

        if(pid) {       /* Parent */
		if(bWait)
			if(waitpid(pid, NULL, 0) == -1)
				if(errno != ECHILD) {
					/* we do not use logerror(), because
					 * that might bring us into an endless
					 * loop. At some time, we may
					 * reconsider this behaviour.
					 */
					dbgprintf("could not wait on child after executing '%s'",
					        (char*)program);
				}
                return pid;
	}
        /* Child */
	alarm(0); /* create a clean environment before we exec the real child */
	for(sig = 0 ; sig < 32 ; ++sig)
		signal(sig, SIG_DFL);
	execlp((char*)program, (char*) program, (char*)arg, NULL);
	/* In the long term, it's a good idea to implement some enhanced error
	 * checking here. However, it can not easily be done. For starters, we
	 * may run into endless loops if we log to syslog. The next problem is
	 * that output is typically not seen by the user. For the time being,
	 * we use no error reporting, which is quite consitent with the old
	 * system() way of doing things. rgerhards, 2007-07-20
	 */
	perror("exec");
	exit(1); /* not much we can do in this case */
}


/* skip over whitespace in a standard C string. The
 * provided pointer is advanced to the first non-whitespace
 * charater or the \0 byte, if there is none. It is never
 * moved past the \0.
 */
void skipWhiteSpace(uchar **pp)
{
	register uchar *p;

	assert(pp != NULL);
	assert(*pp != NULL);

	p = *pp;
	while(*p && isspace((int) *p))
		++p;
	*pp = p;
}


/*
 * vi:set ai:
 */
