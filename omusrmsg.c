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
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include <utmp.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <setjmp.h>
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
#include "syslogd.h"
#include "omusrmsg.h"
#include "module-template.h"
#include "errmsg.h"


/* portability: */
#ifndef _PATH_DEV
#	define _PATH_DEV	"/dev/"
#endif


MODULE_TYPE_OUTPUT

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


static jmp_buf ttybuf;

static void endtty()
{
	longjmp(ttybuf, 1);
}

/**
 * BSD setutent/getutent() replacement routines
 * The following routines emulate setutent() and getutent() under
 * BSD because they are not available there. We only emulate what we actually
 * need! rgerhards 2005-03-18
 */
#ifdef BSD
static FILE *BSD_uf = NULL;
void setutent(void)
{
	assert(BSD_uf == NULL);
	if ((BSD_uf = fopen(_PATH_UTMP, "r")) == NULL) {
		errmsg.LogError(NO_ERRCODE, "%s", _PATH_UTMP);
		return;
	}
}

struct utmp* getutent(void)
{
	static struct utmp st_utmp;

	if(fread((char *)&st_utmp, sizeof(st_utmp), 1, BSD_uf) != 1)
		return NULL;

	return(&st_utmp);
}

void endutent(void)
{
	fclose(BSD_uf);
	BSD_uf = NULL;
}
#endif


/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 *
 * rgerhards, 2005-10-19: applying the following sysklogd patch:
 * Tue May  4 16:52:01 CEST 2004: Solar Designer <solar@openwall.com>
 *	Adjust the size of a variable to prevent a buffer overflow
 *	should _PATH_DEV ever contain something different than "/dev/".
 */
static rsRetVal wallmsg(uchar* pMsg, instanceData *pData)
{
  
	char p[sizeof(_PATH_DEV) + UNAMESZ];
	register int i;
	int ttyf;
	static int reenter = 0;
	struct utmp ut;
	struct utmp *uptr;
	struct sigaction sigAct;

	assert(pMsg != NULL);

	if (reenter++)
		return RS_RET_OK;

	/* open the user login file */
	setutent();

	/*
	 * Might as well fork instead of using nonblocking I/O
	 * and doing notty().
	 */
	if (fork() == 0) {
		memset(&sigAct, 0, sizeof(sigAct));
		sigemptyset(&sigAct.sa_mask);
		sigAct.sa_handler = SIG_DFL;
		sigaction(SIGTERM, &sigAct, NULL);
		alarm(0);

#		ifdef		SIGTTOU
		sigAct.sa_handler = SIG_DFL;
		sigaction(SIGTERM, &sigAct, NULL);
#		endif
		/* It is save to call sigprocmask here, as we are now executing the child (no threads) */
		sigprocmask(SIG_SETMASK, &sigAct.sa_mask, NULL);
	/* TODO: find a way to limit the max size of the message. hint: this
	 * should go into the template!
	 */
	
	/* rgerhards 2005-10-24: HINT: this code might be run in a seperate thread
	 * instead of a seperate process once we have multithreading...
	 */

		/* scan the user login file */
		while ((uptr = getutent())) {
			memcpy(&ut, uptr, sizeof(ut));
			/* is this slot used? */
			if (ut.ut_name[0] == '\0')
				continue;
#ifndef BSD
			if (ut.ut_type != USER_PROCESS)
			        continue;
#endif
			if (!(strncmp (ut.ut_name,"LOGIN", 6))) /* paranoia */
			        continue;

			/* should we send the message to this user? */
			if (pData->bIsWall == 0) {
				for (i = 0; i < MAXUNAMES; i++) {
					if (!pData->uname[i][0]) {
						i = MAXUNAMES;
						break;
					}
					if (strncmp(pData->uname[i],
					    ut.ut_name, UNAMESZ) == 0)
						break;
				}
				if (i >= MAXUNAMES)
					continue;
			}

			/* compute the device name */
			strcpy(p, _PATH_DEV);
			strncat(p, ut.ut_line, UNAMESZ);

			if (setjmp(ttybuf) == 0) {
				sigAct.sa_handler = endtty;
				sigaction(SIGALRM, &sigAct, NULL);
				(void) alarm(15);
				/* open the terminal */
				ttyf = open(p, O_WRONLY|O_NOCTTY);
				if (ttyf >= 0) {
					struct stat statb;

					if (fstat(ttyf, &statb) == 0 &&
					    (statb.st_mode & S_IWRITE)) {
						(void) write(ttyf, pMsg, strlen((char*)pMsg));
					}
					close(ttyf);
					ttyf = -1;
				}
			}
			(void) alarm(0);
		}
		exit(0); /* "good" exit - this terminates the child forked just for message delivery */
	}
	/* close the user login file */
	endutent();
	reenter = 0;
	return RS_RET_OK;
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
       if (!*p || !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
                    || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' || *p == '*'))
	       ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);

	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;


	if(*p == '*') { /* wall */
		dbgprintf("write-all");
		++p; /* eat '*' */
		pData->bIsWall = 1; /* write to all users */
		if((iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " WallFmt"))
		    != RS_RET_OK)
			goto finalize_it;
	} else {
                /* everything else beginning with the regex above
                 * is currently treated as a user name
                 * TODO: is this portable?
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

/*
 * vi:set ai:
 */
