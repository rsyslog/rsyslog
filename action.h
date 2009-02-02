/* action.h
 * Header file for the action object
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c)
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
#ifndef	ACTION_H_INCLUDED
#define	ACTION_H_INCLUDED 1

#include "syslogd-types.h"
#include "sync.h"
#include "queue.h"

/* external data - this is to be removed when we change the action
 * object interface (will happen some time..., at latest when the
 * config file format is changed). -- rgerhards, 2008-01-28
 */
extern int glbliActionResumeRetryCount;


/* the following struct defines the action object data structure
 */
struct action_s {
	time_t	f_time;		/* used for "message repeated n times" - be careful, old, old code */
	time_t	tActNow;	/* the current time for an action execution. Initially set to -1 and
				   populated on an as-needed basis. This is a performance optimization. */
	time_t	tLastExec;	/* time this action was last executed */
	int	bExecWhenPrevSusp;/* execute only when previous action is suspended? */
	int	iSecsExecOnceInterval; /* if non-zero, minimum seconds to wait until action is executed again */
	short	bEnabled;	/* is the related action enabled (1) or disabled (0)? */
	short	bSuspended;	/* is the related action temporarily suspended? */
	time_t	ttResumeRtry;	/* when is it time to retry the resume? */
	int	iResumeInterval;/* resume interval for this action */
	int	iResumeRetryCount;/* how often shall we retry a suspended action? (-1 --> eternal) */
	int	iNbrResRtry;	/* number of retries since last suspend */
	int	iNbrNoExec;	/* number of matches that did not yet yield to an exec */
	int	iExecEveryNthOccur;/* execute this action only every n-th occurence (with n=0,1 -> always) */
	int  	iExecEveryNthOccurTO;/* timeout for n-th occurence feature */
	time_t  tLastOccur;	/* time last occurence was seen (for timing them out) */
	struct modInfo_s *pMod;/* pointer to output module handling this selector */
	void	*pModData;	/* pointer to module data - content is module-specific */
	short	bRepMsgHasMsg;	/* "message repeated..." has msg fragment in it (0-no, 1-yes) */
	short	f_ReduceRepeated;/* reduce repeated lines 0 - no, 1 - yes */
	int	f_prevcount;	/* repetition cnt of prevline */
	int	f_repeatcount;	/* number of "repeated" msgs */
	int	iNumTpls;	/* number of array entries for template element below */
	struct template **ppTpl;/* array of template to use - strings must be passed to doAction
				 * in this order. */
	struct msg* f_pMsg;	/* pointer to the message (this will replace the other vars with msg
				 * content later). This is preserved after the message has been
				 * processed - it is also used to detect duplicates.
				 */
	queue_t *pQueue;	/* action queue */
	SYNC_OBJ_TOOL;		/* required for mutex support */
	pthread_mutex_t mutActExec; /* mutex to guard actual execution of doAction for single-threaded modules */
};
typedef struct action_s action_t;


/* function prototypes
 */
rsRetVal actionConstruct(action_t **ppThis);
rsRetVal actionConstructFinalize(action_t *pThis);
rsRetVal actionDestruct(action_t *pThis);
rsRetVal actionAddCfSysLineHdrl(void);
rsRetVal actionDbgPrint(action_t *pThis);
rsRetVal actionSetGlobalResumeInterval(int iNewVal);
rsRetVal actionDoAction(action_t *pAction);
rsRetVal actionCallAction(action_t *pAction, msg_t *pMsg);
rsRetVal actionWriteToAction(action_t *pAction);
rsRetVal actionCallHUPHdlr(action_t *pAction);
rsRetVal actionClassInit(void);
rsRetVal addAction(action_t **ppAction, modInfo_t *pMod, void *pModData, omodStringRequest_t *pOMSR, int bSuspended);

#if 1
#define actionIsSuspended(pThis) ((pThis)->bSuspended == 1)
#else
/* The function is a debugging aid */
inline int actionIsSuspended(action_t *pThis)
{
	int i;
	ASSERT(pThis != NULL);
	i =  pThis->bSuspended == 1;
	dbgprintf("in IsSuspend(), returns %d\n", i);
	return i;
}
#endif

#endif /* #ifndef ACTION_H_INCLUDED */
/*
 * vi:set ai:
 */
