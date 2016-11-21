/* lmsig_ksi.c
 *
 * An implementation of the sigprov interface for KSI.
 * 
 * Copyright 2013-2016 Rainer Gerhards and Adiscon GmbH.
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
#include "lmsig_ksi.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfpdescr[] = {
	{ "sig.hashfunction", eCmdHdlrGetWord, 0 },
	{ "sig.aggregator.uri", eCmdHdlrGetWord, CNFPARAM_REQUIRED },
	{ "sig.aggregator.user", eCmdHdlrGetWord, CNFPARAM_REQUIRED },
	{ "sig.aggregator.key", eCmdHdlrGetWord, CNFPARAM_REQUIRED },
	{ "sig.block.sizelimit", eCmdHdlrSize, 0 },
	{ "sig.keeprecordhashes", eCmdHdlrBinary, 0 },
	{ "sig.keeptreehashes", eCmdHdlrBinary, 0 },
	{ "dirowner", eCmdHdlrUID, 0 }, /* legacy: dirowner */
	{ "dirownernum", eCmdHdlrInt, 0 }, /* legacy: dirownernum */
	{ "dirgroup", eCmdHdlrGID, 0 }, /* legacy: dirgroup */
	{ "dirgroupnum", eCmdHdlrInt, 0 }, /* legacy: dirgroupnum */
	{ "fileowner", eCmdHdlrUID, 0 }, /* legacy: fileowner */
	{ "fileownernum", eCmdHdlrInt, 0 }, /* legacy: fileownernum */
	{ "filegroup", eCmdHdlrGID, 0 }, /* legacy: filegroup */
	{ "filegroupnum", eCmdHdlrInt, 0 }, /* legacy: filegroupnum */
	{ "dircreatemode", eCmdHdlrFileCreateMode, 0 }, /* legacy: dircreatemode */
	{ "filecreatemode", eCmdHdlrFileCreateMode, 0 } /* legacy: filecreatemode */
};
static struct cnfparamblk pblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfpdescr)/sizeof(struct cnfparamdescr),
	  cnfpdescr
	};


static void
errfunc(__attribute__((unused)) void *usrptr, uchar *emsg)
{
	errmsg.LogError(0, RS_RET_SIGPROV_ERR, "KSI Signature Provider"
		"Error: %s", emsg);
}

static void
logfunc(__attribute__((unused)) void *usrptr, uchar *emsg)
{
	errmsg.LogMsg(0, RS_RET_NO_ERRCODE, LOG_INFO,
		"KSI Signature Provider: %s", emsg);
}


/* Standard-Constructor
 */
BEGINobjConstruct(lmsig_ksi)
	pThis->ctx = rsksiCtxNew();
	rsksisetErrFunc(pThis->ctx, errfunc, NULL);
	rsksisetLogFunc(pThis->ctx, logfunc, NULL);
ENDobjConstruct(lmsig_ksi)


/* destructor for the lmsig_ksi object */
BEGINobjDestruct(lmsig_ksi) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(lmsig_ksi)
	rsksiCtxDel(pThis->ctx);
ENDobjDestruct(lmsig_ksi)


/* apply all params from param block to us. This must be called
 * after construction, but before the OnFileOpen() entry point.
 * Defaults are expected to have been set during construction.
 */
static rsRetVal
SetCnfParam(void *pT, struct nvlst *lst)
{
	char *ag_uri = NULL, *ag_loginid = NULL, *ag_key = NULL;
	lmsig_ksi_t *pThis = (lmsig_ksi_t*) pT;
	int i;
	uchar *cstr;
	struct cnfparamvals *pvals;
	DEFiRet;
	pvals = nvlstGetParams(lst, &pblk, NULL);
	if(pvals == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	if(Debug) {
		dbgprintf("sig param blk in lmsig_ksi:\n");
		cnfparamsPrint(&pblk, pvals);
	}

	for(i = 0 ; i < pblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(pblk.descr[i].name, "sig.hashfunction")) {
			cstr = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
			if(rsksiSetHashFunction(pThis->ctx, (char*)cstr) == 2) {
				errmsg.LogError(0, RS_RET_ERR, "Hash function "
					"'%s' has been removed due to insecurity - "
					"using default", cstr);
			} else if(rsksiSetHashFunction(pThis->ctx, (char*)cstr) != 0) {
				errmsg.LogError(0, RS_RET_ERR, "Hash function "
					"'%s' unknown - using default", cstr);
			}
			free(cstr);
		} else if(!strcmp(pblk.descr[i].name, "sig.aggregator.uri")) {
			ag_uri = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(pblk.descr[i].name, "sig.aggregator.user")) {
			ag_loginid = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(pblk.descr[i].name, "sig.aggregator.key")) {
			ag_key = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(pblk.descr[i].name, "sig.block.sizelimit")) {
			rsksiSetBlockSizeLimit(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeprecordhashes")) {
			rsksiSetKeepRecordHashes(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeptreehashes")) {
			rsksiSetKeepTreeHashes(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "dirowner")) {
			rsksiSetDirUID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "dirownernum")) {
			rsksiSetDirUID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "dirgroup")) {
			rsksiSetDirGID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "dirgroupnum")) {
			rsksiSetDirGID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "fileowner")) {
			rsksiSetFileUID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "fileownernum")) {
			rsksiSetFileUID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "filegroup")) {
			rsksiSetFileGID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "filegroupnum")) {
			rsksiSetFileGID(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "dircreatemode")) {
			rsksiSetDirCreateMode(pThis->ctx, pvals[i].val.d.n);
		} else if (!strcmp(pblk.descr[i].name, "filecreatemode")) {
			rsksiSetCreateMode(pThis->ctx, pvals[i].val.d.n);
		} else {
			DBGPRINTF("lmsig_ksi: program error, non-handled "
			  "param '%s'\n", pblk.descr[i].name);
		}
	}

	if(rsksiSetAggregator(pThis->ctx, ag_uri, ag_loginid, ag_key) != KSI_OK)
		ABORT_FINALIZE(RS_RET_KSI_ERR);

	free(ag_uri);
	free(ag_loginid);
	free(ag_key);
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &pblk);
	RETiRet;
}


static rsRetVal
OnFileOpen(void *pT, uchar *fn, void *pGF)
{
	lmsig_ksi_t *pThis = (lmsig_ksi_t*) pT;
	ksifile *pgf = (ksifile*) pGF;
	DEFiRet;
	DBGPRINTF("lmsig_ksi: onFileOpen: %s\n", fn);
	/* note: if *pgf is set to NULL, this auto-disables GT functions */
	*pgf = rsksiCtxOpenFile(pThis->ctx, fn);
	sigblkInitKSI(*pgf);
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
	DBGPRINTF("lmsig_ksi: onRecordWrite (%d): %s\n", lenRec-1, rec);
	sigblkAddRecordKSI(pF, rec, lenRec-1);

	RETiRet;
}

static rsRetVal
OnFileClose(void *pF)
{
	DEFiRet;
	DBGPRINTF("lmsig_ksi: onFileClose\n");
	rsksifileDestruct(pF);

	RETiRet;
}

BEGINobjQueryInterface(lmsig_ksi)
CODESTARTobjQueryInterface(lmsig_ksi)
	 if(pIf->ifVersion != sigprovCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct = (rsRetVal(*)(void*)) lmsig_ksiConstruct;
	pIf->SetCnfParam = SetCnfParam;
	pIf->Destruct = (rsRetVal(*)(void*)) lmsig_ksiDestruct;
	pIf->OnFileOpen = OnFileOpen;
	pIf->OnRecordWrite = OnRecordWrite;
	pIf->OnFileClose = OnFileClose;
finalize_it:
ENDobjQueryInterface(lmsig_ksi)


BEGINObjClassExit(lmsig_ksi, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(lmsig_ksi)
	/* release objects we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
ENDObjClassExit(lmsig_ksi)


BEGINObjClassInit(lmsig_ksi, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDObjClassInit(lmsig_ksi)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	lmsig_ksiClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
	CHKiRet(lmsig_ksiClassInit(pModInfo));
ENDmodInit
