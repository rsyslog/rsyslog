/* lmsig_gt.c
 *
 * An implementation of the sigprov interface for GuardTime.
 * 
 * Copyright 2013-2015 Rainer Gerhards and Adiscon GmbH.
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

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfpdescr[] = {
	{ "sig.hashfunction", eCmdHdlrGetWord, 0 },
	{ "sig.timestampservice", eCmdHdlrGetWord, 0 },
	{ "sig.block.sizelimit", eCmdHdlrSize, 0 },
	{ "sig.keeprecordhashes", eCmdHdlrBinary, 0 },
	{ "sig.keeptreehashes", eCmdHdlrBinary, 0 }
};
static struct cnfparamblk pblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfpdescr)/sizeof(struct cnfparamdescr),
	  cnfpdescr
	};


static void
errfunc(__attribute__((unused)) void *usrptr, uchar *emsg)
{
	errmsg.LogError(0, RS_RET_SIGPROV_ERR, "Signature Provider"
		"Error: %s - disabling signatures", emsg);
}

/* Standard-Constructor
 */
BEGINobjConstruct(lmsig_gt)
	pThis->ctx = rsgtCtxNew();
	rsgtsetErrFunc(pThis->ctx, errfunc, NULL);
ENDobjConstruct(lmsig_gt)


/* destructor for the lmsig_gt object */
BEGINobjDestruct(lmsig_gt) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(lmsig_gt)
	rsgtCtxDel(pThis->ctx);
ENDobjDestruct(lmsig_gt)


/* apply all params from param block to us. This must be called
 * after construction, but before the OnFileOpen() entry point.
 * Defaults are expected to have been set during construction.
 */
static rsRetVal
SetCnfParam(void *pT, struct nvlst *lst)
{
	lmsig_gt_t *pThis = (lmsig_gt_t*) pT;
	int i;
	uchar *cstr;
	struct cnfparamvals *pvals;
	DEFiRet;
	pvals = nvlstGetParams(lst, &pblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	if(Debug) {
		dbgprintf("sig param blk in lmsig_gt:\n");
		cnfparamsPrint(&pblk, pvals);
	}

	for(i = 0 ; i < pblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(pblk.descr[i].name, "sig.hashfunction")) {
			cstr = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
			if(rsgtSetHashFunction(pThis->ctx, (char*)cstr) != 0) {
				errmsg.LogError(0, RS_RET_ERR, "Hash function "
					"'%s' unknown - using default", cstr);
			}
			free(cstr);
		} else if(!strcmp(pblk.descr[i].name, "sig.timestampservice")) {
			cstr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			rsgtSetTimestamper(pThis->ctx, (char*) cstr);
			free(cstr);
		} else if(!strcmp(pblk.descr[i].name, "sig.block.sizelimit")) {
			rsgtSetBlockSizeLimit(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeprecordhashes")) {
			rsgtSetKeepRecordHashes(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeptreehashes")) {
			rsgtSetKeepTreeHashes(pThis->ctx, pvals[i].val.d.n);
		} else {
			DBGPRINTF("lmsig_gt: program error, non-handled "
			  "param '%s'\n", pblk.descr[i].name);
		}
	}
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &pblk);
	RETiRet;
}


static rsRetVal
OnFileOpen(void *pT, uchar *fn, void *pGF)
{
	lmsig_gt_t *pThis = (lmsig_gt_t*) pT;
	gtfile *pgf = (gtfile*) pGF;
	DEFiRet;
	DBGPRINTF("lmsig_gt: onFileOpen: %s\n", fn);
	/* note: if *pgf is set to NULL, this auto-disables GT functions */
	*pgf = rsgtCtxOpenFile(pThis->ctx, fn);
	sigblkInit(*pgf);
	RETiRet;
}

/* Note: we assume that the record is terminated by a \n.
 * As of the GuardTime paper, \n is not part of the signed
 * message, so we subtract one from the record size. This
 * may cause issues with non-standard formats, but let's 
 * see how things evolve (the verifier will not work in
 * any case when the records are not \n delimited...).
 * rgerhards, 2013-03-17
 */
static rsRetVal
OnRecordWrite(void *pF, uchar *rec, rs_size_t lenRec)
{
	DEFiRet;
	DBGPRINTF("lmsig_gt: onRecordWrite (%d): %s\n", lenRec-1, rec);
	sigblkAddRecord(pF, rec, lenRec-1);

	RETiRet;
}

static rsRetVal
OnFileClose(void *pF)
{
	DEFiRet;
	DBGPRINTF("lmsig_gt: onFileClose\n");
	rsgtfileDestruct(pF);

	RETiRet;
}

BEGINobjQueryInterface(lmsig_gt)
CODESTARTobjQueryInterface(lmsig_gt)
	 if(pIf->ifVersion != sigprovCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct = (rsRetVal(*)(void*)) lmsig_gtConstruct;
	pIf->SetCnfParam = SetCnfParam;
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

	if(rsgtInit((char*)"rsyslogd " VERSION) != 0) {
		errmsg.LogError(0, RS_RET_SIGPROV_ERR, "error initializing "
			"signature provider - cannot sign");
		ABORT_FINALIZE(RS_RET_SIGPROV_ERR);
	}
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
