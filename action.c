/* action.c
 *
 * Implementation of the action object.
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "syslogd.h"
#include "template.h"
#include "action.h"
#include "modules.h"
#include "sync.h"


/* object static data (once for all instances) */
static int glbliActionResumeInterval = 30;

/* destructs an action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionDestruct(action_t *pThis)
{
	assert(pThis != NULL);

	if(pThis->pMod != NULL)
		pThis->pMod->freeInstance(pThis->pModData);

	if(pThis->f_pMsg != NULL)
		MsgDestruct(pThis->f_pMsg);

	SYNC_OBJ_TOOL_EXIT(pThis);
	if(pThis->ppTpl != NULL)
		free(pThis->ppTpl);
	if(pThis->ppMsgs != NULL)
		free(pThis->ppMsgs);
	free(pThis);
	
	return RS_RET_OK;
}


/* create a new action descriptor object
 * rgerhards, 2007-08-01
 */
rsRetVal actionConstruct(action_t **ppThis)
{
	DEFiRet;
	action_t *pThis;

	assert(ppThis != NULL);
	
	if((pThis = (action_t*) calloc(1, sizeof(action_t))) == NULL) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	pThis->iResumeInterval = glbliActionResumeInterval;
	SYNC_OBJ_TOOL_INIT(pThis);

finalize_it:
	*ppThis = pThis;
	return iRet;
}


/* set an action back to active state -- rgerhards, 2007-08-02
 */
static rsRetVal actionResume(action_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	pThis->bSuspended = 0;

	return iRet;
}


/* set the global resume interval
 */
rsRetVal actionSetGlobalResumeInterval(int iNewVal)
{
	glbliActionResumeInterval = iNewVal;
	return RS_RET_OK;
}


/* suspend an action -- rgerhards, 2007-08-02
 */
rsRetVal actionSuspend(action_t *pThis)
{
	DEFiRet;

	assert(pThis != NULL);
	pThis->bSuspended = 1;
	pThis->ttResumeRtry = time(NULL) + pThis->iResumeInterval;
	pThis->iNbrResRtry = 0; /* tell that we did not yet retry to resume */

	return iRet;
}

/* try to resume an action -- rgerhards, 2007-08-02
 * returns RS_RET_OK if resumption worked, RS_RET_SUSPEND if the
 * action is still suspended.
 */
rsRetVal actionTryResume(action_t *pThis)
{
	DEFiRet;
	time_t ttNow;

	assert(pThis != NULL);

	ttNow = time(NULL); /* do the system call just once */

	/* first check if it is time for a re-try */
	if(ttNow > pThis->ttResumeRtry) {
		iRet = pThis->pMod->tryResume(pThis->pModData);
		if(iRet == RS_RET_SUSPENDED) {
			/* set new tryResume time */
			++pThis->iNbrResRtry;
			/* if we have more than 10 retries, we prolong the
			 * retry interval. If something is really stalled, it will
			 * get re-tried only very, very seldom - but that saves
			 * CPU time. TODO: maybe a config option for that?
			 * rgerhards, 2007-08-02
			 */
			pThis->ttResumeRtry = ttNow + pThis->iResumeInterval * (pThis->iNbrResRtry / 10 + 1);
		}
	} else {
		/* it's too early, we are still suspended --> indicate this */
		iRet = RS_RET_SUSPENDED;
	}

	if(iRet == RS_RET_OK)
		actionResume(pThis);

	dbgprintf("actionTryResume: iRet: %d, next retry (if applicable): %u [now %u]\n",
		iRet, (unsigned) pThis->ttResumeRtry, (unsigned) ttNow);

	return iRet;
}


/* debug-print the contents of an action object
 * rgerhards, 2007-08-02
 */
rsRetVal actionDbgPrint(action_t *pThis)
{
	DEFiRet;

	printf("%s: ", modGetStateName(pThis->pMod));
	pThis->pMod->dbgPrintInstInfo(pThis->pModData);
	printf("\n\tInstance data: 0x%lx\n", (unsigned long) pThis->pModData);
	printf("\tRepeatedMsgReduction: %d\n", pThis->f_ReduceRepeated);
	printf("\tResume Interval: %d\n", pThis->iResumeInterval);
	printf("\tSuspended: %d", pThis->bSuspended);
	if(pThis->bSuspended) {
		printf(" next retry: %u, number retries: %d", (unsigned) pThis->ttResumeRtry, pThis->iNbrResRtry);
	}
	printf("\n");
	printf("\tDisabled: %d\n", !pThis->bEnabled);
	printf("\tExec only when previous is suspended: %d\n", pThis->bExecWhenPrevSusp);
	printf("\n");

	return iRet;
}


/*
 * vi:set ai:
 */
