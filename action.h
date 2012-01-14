/* action.h
 * Header file for the action object
 *
 * File begun on 2007-08-06 by RGerhards (extracted from syslogd.c, which
 * was under BSD license at the time of rsyslog fork)
 *
 * Copyright 2007-2012 Adiscon GmbH.
 *
 * This file is part of rsyslog.
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


typedef enum {
	ACT_STATE_DIED = 0,	/* action permanently failed and now disabled  - MUST BE ZEO! */
	ACT_STATE_RDY  = 1,	/* action ready, waiting for new transaction */
	ACT_STATE_ITX  = 2,	/* transaction active, waiting for new data or commit */
	ACT_STATE_COMM = 3, 	/* transaction finished (a transient state) */
	ACT_STATE_RTRY = 4,	/* failure occured, trying to restablish ready state */
	ACT_STATE_SUSP = 5	/* suspended due to failure (return fail until timeout expired) */
} action_state_t;

/* the following struct defines the action object data structure
 */
typedef struct action_s action_t;
struct action_s {
	time_t	f_time;		/* used for "message repeated n times" - be careful, old, old code */
	time_t	tActNow;	/* the current time for an action execution. Initially set to -1 and
				   populated on an as-needed basis. This is a performance optimization. */
	time_t	tLastExec;	/* time this action was last executed */
	sbool	bExecWhenPrevSusp;/* execute only when previous action is suspended? */
	sbool	bWriteAllMarkMsgs;/* should all mark msgs be written (not matter how recent the action was executed)? */
	int	iSecsExecOnceInterval; /* if non-zero, minimum seconds to wait until action is executed again */
	action_state_t eState;	/* current state of action */
	sbool	bHadAutoCommit;	/* did an auto-commit happen during doAction()? */
	time_t	ttResumeRtry;	/* when is it time to retry the resume? */
	int	iResumeOKinRow;	/* number of times in a row that resume said OK with an immediate failure following */
	int	iResumeInterval;/* resume interval for this action */
	int	iResumeRetryCount;/* how often shall we retry a suspended action? (-1 --> eternal) */
	int	iNbrResRtry;	/* number of retries since last suspend */
	int	iNbrNoExec;	/* number of matches that did not yet yield to an exec */
	int	iExecEveryNthOccur;/* execute this action only every n-th occurence (with n=0,1 -> always) */
	int  	iExecEveryNthOccurTO;/* timeout for n-th occurence feature */
	time_t  tLastOccur;	/* time last occurence was seen (for timing them out) */
	struct modInfo_s *pMod;/* pointer to output module handling this selector */
	void	*pModData;	/* pointer to module data - content is module-specific */
	sbool	bRepMsgHasMsg;	/* "message repeated..." has msg fragment in it (0-no, 1-yes) */
	short	f_ReduceRepeated;/* reduce repeated lines 0 - no, 1 - yes */
	int	f_prevcount;	/* repetition cnt of prevline */
	int	f_repeatcount;	/* number of "repeated" msgs */
	rsRetVal (*submitToActQ)(action_t *, batch_t *);/* function submit message to action queue */
	rsRetVal (*qConstruct)(struct queue_s *pThis);
	enum 	{ ACT_STRING_PASSING = 0, ACT_ARRAY_PASSING = 1, ACT_MSG_PASSING }
		eParamPassing;	/* mode of parameter passing to action */
	int	iNumTpls;	/* number of array entries for template element below */
	struct template **ppTpl;/* array of template to use - strings must be passed to doAction
				 * in this order. */
	msg_t *f_pMsg;		/* pointer to the message (this will replace the other vars with msg
				 * content later). This is preserved after the message has been
				 * processed - it is also used to detect duplicates.
				 */
	qqueue_t *pQueue;	/* action queue */
	SYNC_OBJ_TOOL;		/* required for mutex support */
	pthread_mutex_t mutActExec; /* mutex to guard actual execution of doAction for single-threaded modules */
	uchar *pszName;		/* action name (for documentation) */
	DEF_ATOMIC_HELPER_MUT(mutCAS);
};


/* function prototypes
 */
rsRetVal actionConstruct(action_t **ppThis);
rsRetVal actionConstructFinalize(action_t *pThis);
rsRetVal actionDestruct(action_t *pThis);
rsRetVal actionDbgPrint(action_t *pThis);
rsRetVal actionSetGlobalResumeInterval(int iNewVal);
rsRetVal actionDoAction(action_t *pAction);
rsRetVal actionWriteToAction(action_t *pAction);
rsRetVal actionCallHUPHdlr(action_t *pAction);
rsRetVal actionClassInit(void);
rsRetVal addAction(action_t **ppAction, modInfo_t *pMod, void *pModData, omodStringRequest_t *pOMSR, int bSuspended);
rsRetVal actionNewScope(void);
rsRetVal actionRestoreScope(void);

#endif /* #ifndef ACTION_H_INCLUDED */
