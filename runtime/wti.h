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


/* the worker thread instance class */
struct wti_s {
	BEGINobjInstance;
	pthread_t thrdID; 	/* thread ID */
	int bIsRunning;	/* is this thread currently running? (must be int for atomic op!) */
	sbool bAlwaysRunning;	/* should this thread always run? */
	wtp_t *pWtp; /* my worker thread pool (important if only the work thread instance is passed! */
	batch_t batch; /* pointer to an object array meaningful for current user pointer (e.g. queue pUsr data elemt) */
	uchar *pszDbgHdr;	/* header string for debug messages */
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

#endif /* #ifndef WTI_H_INCLUDED */
