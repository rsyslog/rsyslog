/* lmcry_ossl.c
 *
 * An implementation of the cryprov interface for openssl.
 *
 * Copyright 2013-2017 Rainer Gerhards and Adiscon GmbH.
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
#include <errno.h>

#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "cryprov.h"
#include "parserif.h"
#include "libossl.h"
#include "lmcry_ossl.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(glbl)

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfpdescrRegular[] = {
	{ "cry.key", eCmdHdlrGetWord, 0 },
	{ "cry.keyfile", eCmdHdlrGetWord, 0 },
	{ "cry.mode", eCmdHdlrGetWord, 0 },
	{ "cry.algo", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk pblkRegular =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfpdescrRegular)/sizeof(struct cnfparamdescr),
	  cnfpdescrRegular
	};

static struct cnfparamdescr cnfpdescrQueue[] = {
	{ "queue.cry.key", eCmdHdlrGetWord, 0 },
	{ "queue.cry.keyfile", eCmdHdlrGetWord, 0 },
	{ "queue.cry.mode", eCmdHdlrGetWord, 0 },
	{ "queue.cry.algo", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk pblkQueue =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfpdescrQueue)/sizeof(struct cnfparamdescr),
	  cnfpdescrQueue
	};


/* Standard-Constructor
 */
BEGINobjConstruct(lmcry_ossl)
	CHKmalloc(pThis->ctx = osslCtxNew());
finalize_it:
ENDobjConstruct(lmcry_ossl)


/* destructor for the lmcry_ossl object */
BEGINobjDestruct(lmcry_ossl) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(lmcry_ossl)
	rsosslCtxDel(pThis->ctx);
ENDobjDestruct(lmcry_ossl)


/* apply all params from param block to us. This must be called
 * after construction, but before the OnFileOpen() entry point.
 * Defaults are expected to have been set during construction.
 */
static rsRetVal
SetCnfParam(void *pT, struct nvlst *lst, int paramType)
{
	lmcry_ossl_t *pThis = (lmcry_ossl_t*) pT;
	int i, r;
	unsigned keylen = 0;
	uchar *key = NULL;
	uchar *keyfile = NULL;
	uchar *algomode = NULL;
	int nKeys; /* number of keys (actually methods) specified */
	struct cnfparamvals *pvals;
	struct cnfparamblk *pblk;
	DEFiRet;

	pblk = (paramType == CRYPROV_PARAMTYPE_REGULAR ) ?  &pblkRegular : &pblkQueue;
	nKeys = 0;
	pvals = nvlstGetParams(lst, pblk, NULL);
	if(pvals == NULL) {
		parser_errmsg("error crypto provider ossl config parameters");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	if(Debug) {
		dbgprintf("param blk in lmcry_ossl:\n");
		cnfparamsPrint(pblk, pvals);
	}

	for(i = 0 ; i < pblk->nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(pblk->descr[i].name, "cry.key") ||
		   !strcmp(pblk->descr[i].name, "queue.cry.key")) {
			key = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
			++nKeys;
		} else if(!strcmp(pblk->descr[i].name, "cry.keyfile") ||
		          !strcmp(pblk->descr[i].name, "queue.cry.keyfile")) {
			keyfile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			++nKeys;
		} else if(!strcmp(pblk->descr[i].name, "cry.algo") ||
		          !strcmp(pblk->descr[i].name, "queue.cry.algo")) {
			algomode = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			DBGPRINTF("lmcry_ossl: program error, non-handled "
			  "param '%s'\n", pblk->descr[i].name);
		}
	}
	if (algomode != NULL) {
		iRet = rsosslSetAlgoMode(pThis->ctx, algomode);
		if(iRet != RS_RET_OK) {
			LogError(0, iRet, "cry.algo '%s' is not know/supported", algomode);
			FINALIZE;
		}
	}

	/* note: key must be set AFTER algo/mode is set (as it depends on them) */
	if(nKeys != 1) {
		LogError(0, RS_RET_INVALID_PARAMS, "excactly one of the following "
			"parameters can be specified: cry.key, cry.keyfile\n");
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	}

	if(key != NULL) {
		LogError(0, RS_RET_ERR, "Note: specifying an actual key directly from the "
			"config file is highly insecure - DO NOT USE FOR PRODUCTION");
		keylen = strlen((char*)key);
	}
	if(keyfile != NULL) {
		r = osslGetKeyFromFile((char*)keyfile, (char**)&key, &keylen);
		if(r != 0) {
			LogError(errno, RS_RET_ERR, "error reading keyfile %s",
				keyfile);
			ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
		}
	}

	/* if we reach this point, we have a valid key */
	r = rsosslSetKey(pThis->ctx, key, keylen);
	if(r > 0) {
		LogError(0, RS_RET_INVALID_PARAMS, "Key length %d expected, but "
			"key of length %d given", r, keylen);
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	}

finalize_it:
	free(key);
	free(keyfile);
	free(algomode);
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, pblk);
	RETiRet;
}

static void
SetDeleteOnClose(void *pF, int val)
{
	osslfileSetDeleteOnClose(pF, val);
}

static rsRetVal
GetBytesLeftInBlock(void *pF, ssize_t *left)
{
	return osslfileGetBytesLeftInBlock((osslfile) pF, left);
}

static rsRetVal
DeleteStateFiles(uchar *logfn)
{
	return osslfileDeleteState(logfn);
}

static rsRetVal
OnFileOpen(void *pT, uchar *fn, void *pGF, char openMode)
{
	lmcry_ossl_t* pThis = (lmcry_ossl_t*)pT;
	osslfile* pgf = (osslfile*)pGF;
	DEFiRet;
	DBGPRINTF("lmcry_ossl: open file '%s', mode '%c'\n", fn, openMode);

	iRet = rsosslInitCrypt(pThis->ctx, pgf, fn, openMode);
	if (iRet != RS_RET_OK) {
		LogError(0, iRet, "Encryption Provider"
			"Error: cannot open .encinfo file - disabling log file");
	}
	RETiRet;
}

static rsRetVal
Decrypt(void *pF, uchar *rec, size_t *lenRec)
{
	DEFiRet;
	iRet = rsosslDecrypt(pF, rec, lenRec);
	RETiRet;
}


static rsRetVal
Encrypt(void *pF, uchar *rec, size_t *lenRec)
{
	DEFiRet;
	iRet = rsosslEncrypt(pF, rec, lenRec);
	RETiRet;
}

static rsRetVal
OnFileClose(void *pF, off64_t offsLogfile)
{
	DEFiRet;
	osslfileDestruct(pF, offsLogfile);
	RETiRet;
}

BEGINobjQueryInterface(lmcry_ossl)
CODESTARTobjQueryInterface(lmcry_ossl)
	 if(pIf->ifVersion != cryprovCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct = (rsRetVal(*)(void*)) lmcry_osslConstruct;
	pIf->SetCnfParam = SetCnfParam;
	pIf->SetDeleteOnClose = SetDeleteOnClose;
	pIf->Destruct = (rsRetVal(*)(void*)) lmcry_osslDestruct;
	pIf->OnFileOpen = OnFileOpen;
	pIf->Encrypt = Encrypt;
	pIf->Decrypt = Decrypt;
	pIf->OnFileClose = OnFileClose;
	pIf->DeleteStateFiles = DeleteStateFiles;
	pIf->GetBytesLeftInBlock = GetBytesLeftInBlock;
finalize_it:
ENDobjQueryInterface(lmcry_ossl)


BEGINObjClassExit(lmcry_ossl, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(lmcry_ossl)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	rsosslExit();
ENDObjClassExit(lmcry_ossl)


BEGINObjClassInit(lmcry_ossl, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	if (rsosslInit() != 0) {
		LogError(0, RS_RET_CRYPROV_ERR, "error initializing "
			"ossl crypto provider - cannot encrypt");
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}
ENDObjClassInit(lmcry_ossl)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	lmcry_osslClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(lmcry_osslClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit

