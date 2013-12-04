/* Definition of the worker thread instance (wti) class.
 *
 * Copyright 2008-2013 Adiscon GmbH.
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

#ifndef WTI_H_INCLUDED
#define WTI_H_INCLUDED

#include <pthread.h>
#include "wtp.h"
#include "obj.h"
#include "batch.h"
#include "action.h"


#define ACT_STATE_RDY  0	/* action ready, waiting for new transaction */
#define ACT_STATE_ITX  1	/* transaction active, waiting for new data or commit */
#define ACT_STATE_COMM 2 	/* transaction finished (a transient state) */
#define ACT_STATE_RTRY 3	/* failure occured, trying to restablish ready state */
#define ACT_STATE_SUSP 4	/* suspended due to failure (return fail until timeout expired) */
/* note: 3 bit bit field --> highest value is 7! */

/* The following structure defines immutable parameters which need to
 * be passed as action parameters.
 *
 * WARNING: THIS STRUCTURE IS PART OF THE ***OUTPUT MODULE INTERFACE***
 * It is passed into the doCommit() function. Do NOT modify it until
 * absolutely necessary - all output plugins need to be changed!
 *
 * If a change is "just" for internal working, consider adding a
 * separate paramter outside of this structure. Of course, it is
 * best to avoid this as well ;-)
 * rgerhards, 2013-12-04
 */
typedef struct actWrkrIParams {
	void *param[CONF_OMOD_NUMSTRINGS_MAXSIZE];
	uint32_t lenBuf[CONF_OMOD_NUMSTRINGS_MAXSIZE];  /* length of string buffer (if string ptr) */
	uint32_t lenStr[CONF_OMOD_NUMSTRINGS_MAXSIZE];	/* length of current string (if string ptr) */
} actWrkrIParams_t;

typedef struct actWrkrInfo {
	action_t *pAction;
	void *actWrkrData;
	uint16_t uResumeOKinRow;/* number of times in a row that resume said OK with an immediate failure following */
	int	iNbrResRtry;	/* number of retries since last suspend */
	struct {
		unsigned actState : 3;
	} flags;
	actWrkrIParams_t *iparams;	/* dynamically sized array for template parameters */
	int currIParam;
	int maxIParams;	/* current max */
	actWrkrIParams_t actParams;
} actWrkrInfo_t;

/* the worker thread instance class */
struct wti_s {
	BEGINobjInstance;
	pthread_t thrdID; 	/* thread ID */
	int bIsRunning;	/* is this thread currently running? (must be int for atomic op!) */
	sbool bAlwaysRunning;	/* should this thread always run? */
	int *pbShutdownImmediate;/* end processing of this batch immediately if set to 1 */
	wtp_t *pWtp; /* my worker thread pool (important if only the work thread instance is passed! */
	batch_t batch; /* pointer to an object array meaningful for current user pointer (e.g. queue pUsr data elemt) */
	uchar *pszDbgHdr;	/* header string for debug messages */
	actWrkrInfo_t *actWrkrInfo; /* *array* of action wrkr infos for all actions (sized for max nbr of actions in config!) */
	pthread_cond_t pcondBusy; /* condition to wake up the worker, protected by pmutUsr in wtp */
	DEF_ATOMIC_HELPER_MUT(mutIsRunning);
	struct {
		uint8_t bPrevWasSuspended;
		uint8_t bDoAutoCommit; /* do a commit after each message
		                        * this is usually set for batches with 0 element, but may
					* also be added as a user-selectable option (not implemented yet)
					*/
	} execState;	/* state for the execution engine */
};


/* prototypes */
rsRetVal wtiConstruct(wti_t **ppThis);
rsRetVal wtiConstructFinalize(wti_t * const pThis);
rsRetVal wtiDestruct(wti_t **ppThis);
rsRetVal wtiWorker(wti_t * const pThis);
rsRetVal wtiSetDbgHdr(wti_t * const pThis, uchar *pszMsg, size_t lenMsg);
rsRetVal wtiCancelThrd(wti_t * const pThis);
rsRetVal wtiSetAlwaysRunning(wti_t * const pThis);
rsRetVal wtiSetState(wti_t * const pThis, sbool bNew);
rsRetVal wtiWakeupThrd(wti_t * const pThis);
sbool wtiGetState(wti_t * const pThis);
wti_t *wtiGetDummy(void);
PROTOTYPEObjClassInit(wti);
PROTOTYPEpropSetMeth(wti, pszDbgHdr, uchar*);
PROTOTYPEpropSetMeth(wti, pWtp, wtp_t*);

static inline uint8_t
getActionStateByNbr(wti_t * const pWti, const int iActNbr)
{
	return((uint8_t) pWti->actWrkrInfo[iActNbr].flags.actState);
}

static inline uint8_t
getActionState(wti_t * const pWti, action_t * const pAction)
{
	return((uint8_t) pWti->actWrkrInfo[pAction->iActionNbr].flags.actState);
}

static inline void
setActionState(wti_t * const pWti, action_t * const pAction, uint8_t newState)
{
	pWti->actWrkrInfo[pAction->iActionNbr].flags.actState = newState;
}

static inline uint16_t
getActionResumeInRow(wti_t * const pWti, action_t * const pAction)
{
	return(pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow);
}

static inline void
setActionResumeInRow(wti_t * const pWti, action_t * const pAction, uint16_t val)
{
	pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow = val;
}

static inline void
incActionResumeInRow(wti_t * const pWti, action_t * const pAction)
{
	pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow++;
}

static inline int
getActionNbrResRtry(wti_t * const pWti, action_t * const pAction)
{
	return(pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry);
}

static inline void
setActionNbrResRtry(wti_t * const pWti, action_t * const pAction, const uint16_t val)
{
	pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry = val;
}

static inline void
incActionNbrResRtry(wti_t * const pWti, action_t * const pAction)
{
	pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry++;
}

static inline rsRetVal
wtiNewIParam(wti_t * const pWti, action_t * const pAction, actWrkrIParams_t **piparams)
{
	actWrkrInfo_t *wrkrInfo;
	actWrkrIParams_t *iparams;
	int newMax;
	DEFiRet;

	wrkrInfo = &(pWti->actWrkrInfo[pAction->iActionNbr]);
	if(wrkrInfo->currIParam == wrkrInfo->maxIParams) {
		/* we need to extend */
		dbgprintf("DDDD: extending iparams, max %d\n", wrkrInfo->maxIParams);
		newMax = (wrkrInfo->maxIParams == 0) ? CONF_IPARAMS_BUFSIZE : 2 * wrkrInfo->maxIParams;
		CHKmalloc(iparams = realloc(wrkrInfo->iparams, sizeof(actWrkrIParams_t) * newMax));
		memset(iparams+wrkrInfo->maxIParams, 0,
		       sizeof(actWrkrIParams_t) * (newMax - wrkrInfo->maxIParams));
		wrkrInfo->iparams = iparams;
		wrkrInfo->maxIParams = newMax;
	}
dbgprintf("DDDD: adding param  %d for action %d\n", wrkrInfo->currIParam, pAction->iActionNbr);
	iparams = wrkrInfo->iparams + wrkrInfo->currIParam;
	*piparams = iparams;
	++wrkrInfo->currIParam;

finalize_it:
	RETiRet;
}

static inline void
wtiResetExecState(wti_t * const pWti, batch_t * const pBatch)
{
	pWti->execState.bPrevWasSuspended = 0;
	pWti->execState.bDoAutoCommit = (batchNumMsgs(pBatch) == 1);
}
#endif /* #ifndef WTI_H_INCLUDED */
