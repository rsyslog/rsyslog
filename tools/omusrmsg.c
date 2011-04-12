/* omusrmsg.c
 * This is the implementation of the build-in output module for sending
 * user messages.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * rgerhards, 2008-07-04 (happy Independence Day!): rsyslog inherited the
 * wall functionality from sysklogd. Sysklogd was single-threaded and could
 * not afford to spent a lot of time inside a single action. Thus, it forked
 * off a new process to do the wall. In rsyslog, however, this creates some
 * grief with the threading model. Also, we do not really need to de-couple
 * processing, because we have ample ways to do it in rsyslog. Plus, the
 * default main message queue will care for a somewhat longer execution time.
 * So in short, the real fix to the problem is an architecture change. From
 * now on, we will not fork off a new process but rather do the notification
 * within the current one. This also reduces system overhead.
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
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/param.h>
#ifdef HAVE_UTMP_H
#  include <utmp.h>
#  define STRUCTUTMP struct utmp
#  define UTNAME ut_name
#else
#  include <utmpx.h>
#  define STRUCTUTMP struct utmpx
#  define UTNAME ut_user
#endif
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <errno.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/msgbuf.h>
#endif
#if HAVE_PATHS_H
#include <paths.h>
#endif
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "conf.h"
#include "omusrmsg.h"
#include "module-template.h"
#include "errmsg.h"


/* portability: */
#ifndef _PATH_DEV
#	define _PATH_DEV	"/dev/"
#endif


MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	int bIsWall; /* 1- is wall, 0 - individual users */
	char uname[MAXUNAMES][UNAMESZ+1];
} instanceData;


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* TODO: free the instance pointer (currently a leak, will go away) */
ENDfreeInstance


BEGINdbgPrintInstInfo
	register int i;
CODESTARTdbgPrintInstInfo
	for (i = 0; i < MAXUNAMES && *pData->uname[i]; i++)
		dbgprintf("%s, ", pData->uname[i]);
ENDdbgPrintInstInfo


/**
 * BSD setutent/getutent() replacement routines
 * The following routines emulate setutent() and getutent() under
 * BSD because they are not available there. We only emulate what we actually
 * need! rgerhards 2005-03-18
 */
#ifdef OS_BSD
/* Since version 900007, FreeBSD has a POSIX compliant <utmpx.h> */
#if defined(__FreeBSD__) && (__FreeBSD_version >= 900007)
#  define setutent(void) setutxent(void)
#  define getutent(void) getutxent(void)
#  define endutent(void) endutxent(void)
#else
static FILE *BSD_uf = NULL;
void setutent(void)
{
	assert(BSD_uf == NULL);
	if ((BSD_uf = fopen(_PATH_UTMP, "r")) == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "%s", _PATH_UTMP);
		return;
	}
}

STRUCTUTMP* getutent(void)
{
	static STRUCTUTMP st_utmp;

	if(fread((char *)&st_utmp, sizeof(st_utmp), 1, BSD_uf) != 1)
		return NULL;

	return(&st_utmp);
}

void endutent(void)
{
	fclose(BSD_uf);
	BSD_uf = NULL;
}
#endif /* if defined(__FreeBSD__) */
#endif  /* #ifdef OS_BSD */


/*  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 *
 * rgerhards, 2005-10-19: applying the following sysklogd patch:
 * Tue May  4 16:52:01 CEST 2004: Solar Designer <solar@openwall.com>
 *	Adjust the size of a variable to prevent a buffer overflow
 *	should _PATH_DEV ever contain something different than "/dev/".
 * rgerhards, 2008-07-04: changing the function to no longer use fork() but
 * 	continue run on its thread instead.
 */
static rsRetVal wallmsg(uchar* pMsg, instanceData *pData)
{
  
	uchar szErr[512];
	char p[sizeof(_PATH_DEV) + UNAMESZ];
	register int i;
	int errnoSave;
	int ttyf;
	int wrRet;
	STRUCTUTMP ut;
	STRUCTUTMP *uptr;
	struct stat statb;
	DEFiRet;

	assert(pMsg != NULL);

	/* open the user login file */
	setutent();

	/* scan the user login file */
	while((uptr = getutent())) {
		memcpy(&ut, uptr, sizeof(ut));
		/* is this slot used? */
		if(ut.UTNAME[0] == '\0')
			continue;
#ifndef OS_BSD
		if(ut.ut_type != USER_PROCESS)
			continue;
#endif
		if(!(strncmp (ut.UTNAME,"LOGIN", 6))) /* paranoia */
			continue;

		/* should we send the message to this user? */
		if(pData->bIsWall == 0) {
			for(i = 0; i < MAXUNAMES; i++) {
				if(!pData->uname[i][0]) {
					i = MAXUNAMES;
					break;
				}
				if(strncmp(pData->uname[i], ut.UTNAME, UNAMESZ) == 0)
					break;
			}
			if(i == MAXUNAMES) /* user not found? */
				continue; /* on to next user! */
		}

		/* compute the device name */
		strcpy(p, _PATH_DEV);
		strncat(p, ut.ut_line, UNAMESZ);

		/* we must be careful when writing to the terminal. A terminal may block
		 * (for example, a user has pressed <ctl>-s). In that case, we can not
		 * wait indefinitely. So we need to use non-blocking I/O. In case we would
		 * block, we simply do not send the message, because that's the best we can
		 * do. -- rgerhards, 2008-07-04
		 */

		/* open the terminal */
		if((ttyf = open(p, O_WRONLY|O_NOCTTY|O_NONBLOCK)) >= 0) {
			if(fstat(ttyf, &statb) == 0 && (statb.st_mode & S_IWRITE)) {
				wrRet = write(ttyf, pMsg, strlen((char*)pMsg));
				if(Debug && wrRet == -1) {
					/* we record the state to the debug log */
					errnoSave = errno;
					rs_strerror_r(errno, (char*)szErr, sizeof(szErr));
					dbgprintf("write to terminal '%s' failed with [%d]:%s\n",
						  p, errnoSave, szErr);
				}
			}
			close(ttyf);
		}
	}

	/* close the user login file */
	endutent();
	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	dbgprintf("\n");
	iRet = wallmsg(ppString[0], pData);
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	   /* User names must begin with a gnu e-regex:
	    *   [a-zA-Z0-9_.]
	    * plus '*' for wall
	    */
	if(!*p || !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
	   || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' || *p == '*'))
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

	CHKiRet(createInstance(&pData));

	if(*p == '*') { /* wall */
		dbgprintf("write-all");
		++p; /* eat '*' */
		pData->bIsWall = 1; /* write to all users */
		CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " WallFmt"));
	} else {
		/* everything else beginning with the regex above
		 * is currently treated as a user name -- TODO: is this portable?
		 */
		dbgprintf("users: %s\n", p);	/* ASP */
		pData->bIsWall = 0; /* write to individual users */
		for (i = 0; i < MAXUNAMES && *p && *p != ';'; i++) {
			for (q = p; *q && *q != ',' && *q != ';'; )
				q++;
			(void) strncpy((char*) pData->uname[i], (char*) p, UNAMESZ);
			if ((q - p) > UNAMESZ)
				pData->uname[i][UNAMESZ] = '\0';
			else
				pData->uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		/* done, on to the template
		 * TODO: we need to handle the case where i >= MAXUNAME!
		 */
		if((iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*)" StdUsrMsgFmt"))
			!= RS_RET_OK)
			goto finalize_it;
	}
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(UsrMsg)
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
ENDmodInit

/* vim:set ai:
 */
