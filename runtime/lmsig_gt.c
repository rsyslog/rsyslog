/* lmsig_gt.c
 *
 * An implementation of the sigprov interface for GuardTime.
 * 
 * Copyright 2013 Rainer Gerhards and Adiscon GmbH.
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
#include "config.h"

#include "rsyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "sigprov.h"
#include "lmsig_gt.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)


/* Standard-Constructor
 */
BEGINobjConstruct(lmsig_gt) /* be sure to specify the object type also in END macro! */
	dbgprintf("DDDD: lmsig_gt: called construct\n");
ENDobjConstruct(lmsig_gt)


/* destructor for the lmsig_gt object */
BEGINobjDestruct(lmsig_gt) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(lmsig_gt)
	dbgprintf("DDDD: lmsig_gt: called destruct\n");
ENDobjDestruct(lmsig_gt)

static rsRetVal
OnFileOpen(void *pT, uchar *fn)
{
	lmsig_gt_t *pThis = (lmsig_gt_t*) pT;
	DEFiRet;
dbgprintf("DDDD: onFileOpen: %s\n", fn);
	pThis->ctx = rsgtCtxNew(fn);
	sigblkInit(pThis->ctx);

	RETiRet;
}

static rsRetVal
OnRecordWrite(void *pT, uchar *rec, rs_size_t lenRec)
{
	lmsig_gt_t *pThis = (lmsig_gt_t*) pT;
	DEFiRet;
dbgprintf("DDDD: onRecordWrite (%d): %s\n", lenRec, rec);
	sigblkAddRecord(pThis->ctx, rec, lenRec);

	RETiRet;
}

static rsRetVal
OnFileClose(void *pT)
{
	lmsig_gt_t *pThis = (lmsig_gt_t*) pT;
	DEFiRet;
dbgprintf("DDDD: onFileClose\n");
	rsgtCtxDel(pThis->ctx);

	RETiRet;
}

BEGINobjQueryInterface(lmsig_gt)
CODESTARTobjQueryInterface(lmsig_gt)
	if(pIf->ifVersion != sigprovCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct = (rsRetVal(*)(void*)) lmsig_gtConstruct;
	pIf->Destruct = (rsRetVal(*)(void*)) lmsig_gtDestruct;
	pIf->OnFileOpen = OnFileOpen;
	pIf->OnRecordWrite = OnRecordWrite;
	pIf->OnFileClose = OnFileClose;
finalize_it:
ENDobjQueryInterface(lmsig_gt)


BEGINObjClassExit(lmsig_gt, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(lmsig_gt)
	/* release objects we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);

	rsgtExit();
ENDObjClassExit(lmsig_gt)


BEGINObjClassInit(lmsig_gt, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	rsgtInit("rsyslogd " VERSION);
ENDObjClassInit(lmsig_gt)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	lmsig_gtClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(lmsig_gtClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
