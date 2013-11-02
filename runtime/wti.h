/* Definition of the worker thread instance (wti) class.
 *
 * Copyright 2008-2012 Adiscon GmbH.
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
#define ACT_STATE_DIED 7	/* action permanently failed and now disabled */
/* note: 3 bit bit field --> highest value is 7! */

typedef struct actWrkrInfo {
	action_t *pAction;
	void *actWrkrData;
	uint16_t uResumeOKinRow;/* number of times in a row that resume said OK with an immediate failure following */
	int	iNbrResRtry;	/* number of retries since last suspend */
	struct {
		unsigned actState : 3;
	} flags;
	/* following are caches to save allocs if not absolutely necessary */
	uchar *staticActStrings[CONF_OMOD_NUMSTRINGS_MAXSIZE]; /**< for strings */
				/* a cache to save malloc(), if not absolutely necessary */
	void *staticActParams[CONF_OMOD_NUMSTRINGS_MAXSIZE]; /**< for anything else */
	size_t staticLenStrings[CONF_OMOD_NUMSTRINGS_MAXSIZE];
				/* and the same for the message length (if used) */
	/* end action work variables */
} actWrkrInfo_t;

/* the worker thread instance class */
struct wti_s {
	BEGINobjInstance;
	pthread_t thrdID; 	/* thread ID */
	int bIsRunning;	/* is this thread currently running? (must be int for atomic op!) */
	sbool bAlwaysRunning;	/* should this thread always run? */
	wtp_t *pWtp; /* my worker thread pool (important if only the work thread instance is passed! */
	batch_t batch; /* pointer to an object array meaningful for current user pointer (e.g. queue pUsr data elemt) */
	uchar *pszDbgHdr;	/* header string for debug messages */
	actWrkrInfo_t *actWrkrInfo; /* *array* of action wrkr infos for all actions (sized for max nbr of actions in config!) */
	DEF_ATOMIC_HELPER_MUT(mutIsRunning);
};


/* prototypes */
rsRetVal wtiConstruct(wti_t **ppThis);
rsRetVal wtiConstructFinalize(wti_t *pThis);
rsRetVal wtiDestruct(wti_t **ppThis);
rsRetVal wtiWorker(wti_t *pThis);
rsRetVal wtiSetDbgHdr(wti_t *pThis, uchar *pszMsg, size_t lenMsg);
rsRetVal wtiCancelThrd(wti_t *pThis);
rsRetVal wtiSetAlwaysRunning(wti_t *pThis);
rsRetVal wtiSetState(wti_t *pThis, sbool bNew);
rsRetVal wtiWakeupThrd(wti_t *pThis);
sbool wtiGetState(wti_t *pThis);
PROTOTYPEObjClassInit(wti);
PROTOTYPEpropSetMeth(wti, pszDbgHdr, uchar*);
PROTOTYPEpropSetMeth(wti, pWtp, wtp_t*);

static inline uint8_t
getActionStateByNbr(wti_t *pWti, int iActNbr)
{
	return((uint8_t) pWti->actWrkrInfo[iActNbr].flags.actState);
}

static inline uint8_t
getActionState(wti_t *pWti, action_t *pAction)
{
	return((uint8_t) pWti->actWrkrInfo[pAction->iActionNbr].flags.actState);
}

static inline void
setActionState(wti_t *pWti, action_t *pAction, uint8_t newState)
{
	pWti->actWrkrInfo[pAction->iActionNbr].flags.actState = newState;
}

static inline uint16_t
getActionResumeInRow(wti_t *pWti, action_t *pAction)
{
	return(pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow);
}

static inline void
setActionResumeInRow(wti_t *pWti, action_t *pAction, uint16_t val)
{
	pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow = val;
}

static inline void
incActionResumeInRow(wti_t *pWti, action_t *pAction)
{
	pWti->actWrkrInfo[pAction->iActionNbr].uResumeOKinRow++;
}

static inline int
getActionNbrResRtry(wti_t *pWti, action_t *pAction)
{
	return(pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry);
}

static inline void
setActionNbrResRtry(wti_t *pWti, action_t *pAction, uint16_t val)
{
	pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry = val;
}

static inline void
incActionNbrResRtry(wti_t *pWti, action_t *pAction)
{
	pWti->actWrkrInfo[pAction->iActionNbr].iNbrResRtry++;
}
#endif /* #ifndef WTI_H_INCLUDED */
