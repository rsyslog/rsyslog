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

/* the worker thread instance class */
typedef struct wti_s {
	BEGINobjInstance;
	pthread_t thrdID;  /* thread ID */
	qWrkCmd_t tCurrCmd; /* current command to be carried out by worker */
	obj_t *pUsrp;		/* pointer to an object meaningful for current user pointer (e.g. queue pUsr data elemt) */
	wtp_t *pWtp; /* my worker thread pool (important if only the work thread instance is passed! */
	pthread_cond_t condExitDone; /* signaled when the thread exit is done (once per thread existance) */
	pthread_mutex_t mut;
	bool bShutdownRqtd;	/* shutdown for this thread requested? 0 - no , 1 - yes */
	uchar *pszDbgHdr;	/* header string for debug messages */
	DEF_ATOMIC_HELPER_MUT(mutCurrCmd);
} wti_t;

/* some symbolic constants for easier reference */


/* prototypes */
rsRetVal wtiConstruct(wti_t **ppThis);
rsRetVal wtiConstructFinalize(wti_t *pThis);
rsRetVal wtiDestruct(wti_t **ppThis);
rsRetVal wtiWorker(wti_t *pThis);
rsRetVal wtiProcessThrdChanges(wti_t *pThis, int bLockMutex);
rsRetVal wtiSetDbgHdr(wti_t *pThis, uchar *pszMsg, size_t lenMsg);
rsRetVal wtiSetState(wti_t *pThis, qWrkCmd_t tCmd, int bActiveOnly, int bLockMutex);
rsRetVal wtiJoinThrd(wti_t *pThis);
rsRetVal wtiCancelThrd(wti_t *pThis);
qWrkCmd_t wtiGetState(wti_t *pThis, int bLockMutex);
PROTOTYPEObjClassInit(wti);
PROTOTYPEpropSetMeth(wti, pszDbgHdr, uchar*);
PROTOTYPEpropSetMeth(wti, pWtp, wtp_t*);

#endif /* #ifndef WTI_H_INCLUDED */
