/* lmcry_gcry.c
 *
 * An implementation of the cryprov interface for libgcrypt.
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
#include "cryprov.h"
#include "libgcry.h"
#include "lmcry_gcry.h"

MODULE_TYPE_LIB
MODULE_TYPE_NOKEEP

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

/* tables for interfacing with the v6 config system */
static struct cnfparamdescr cnfpdescr[] = {
	{ "cry.key", eCmdHdlrGetWord, 0 },
	{ "cry.mode", eCmdHdlrGetWord, 0 }, /* CBC, ECB, etc */
	{ "cry.algo", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk pblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(cnfpdescr)/sizeof(struct cnfparamdescr),
	  cnfpdescr
	};


#if 0
static void
errfunc(__attribute__((unused)) void *usrptr, uchar *emsg)
{
	errmsg.LogError(0, RS_RET_CRYPROV_ERR, "Crypto Provider"
		"Error: %s - disabling encryption", emsg);
}
#endif

/* Standard-Constructor
 */
BEGINobjConstruct(lmcry_gcry)
	dbgprintf("DDDD: lmcry_gcry: called construct\n");
	pThis->ctx = gcryCtxNew();
ENDobjConstruct(lmcry_gcry)


/* destructor for the lmcry_gcry object */
BEGINobjDestruct(lmcry_gcry) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(lmcry_gcry)
	dbgprintf("DDDD: lmcry_gcry: called destruct\n");
	rsgcryCtxDel(pThis->ctx);
ENDobjDestruct(lmcry_gcry)


/* apply all params from param block to us. This must be called
 * after construction, but before the OnFileOpen() entry point.
 * Defaults are expected to have been set during construction.
 */
static rsRetVal
SetCnfParam(void *pT, struct nvlst *lst)
{
	lmcry_gcry_t *pThis = (lmcry_gcry_t*) pT;
	int i, r;
	uchar *cstr;
	uchar *key = NULL;
	struct cnfparamvals *pvals;
	DEFiRet;

	pvals = nvlstGetParams(lst, &pblk, NULL);
	if(Debug) {
		dbgprintf("param blk in lmcry_gcry:\n");
		cnfparamsPrint(&pblk, pvals);
	}

	for(i = 0 ; i < pblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(pblk.descr[i].name, "cry.key")) {
			key = (uchar*) es_str2cstr(pvals[i].val.d.estr, NULL);
#if 0
		} else if(!strcmp(pblk.descr[i].name, "sig.timestampservice")) {
			cstr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			gcrySetTimestamper(pThis->ctx, (char*) cstr);
			free(cstr);
		} else if(!strcmp(pblk.descr[i].name, "sig.block.sizelimit")) {
			gcrySetBlockSizeLimit(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeprecordhashes")) {
			gcrySetKeepRecordHashes(pThis->ctx, pvals[i].val.d.n);
		} else if(!strcmp(pblk.descr[i].name, "sig.keeptreehashes")) {
			gcrySetKeepTreeHashes(pThis->ctx, pvals[i].val.d.n);
		} else {
			DBGPRINTF("lmcry_gcry: program error, non-handled "
			  "param '%s'\n", pblk.descr[i].name);
#endif
		}
	}
	if(key != NULL) {
		errmsg.LogError(0, RS_RET_ERR, "Note: specifying an actual key directly from the "
			"config file is highly insecure - DO NOT USE FOR PRODUCTION");
		r = rsgcrySetKey(pThis->ctx, key, strlen((char*)key));
		if(r > 0) {
			errmsg.LogError(0, RS_RET_INVALID_PARAMS, "Key length %d expected, but "
				"key of length %d given", r, strlen((char*)key));
			ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
		}
	}

	cnfparamvalsDestruct(pvals, &pblk);
	if(key != NULL) {
		memset(key, 0, strlen((char*)key));
		free(key);
	}
finalize_it:
	RETiRet;
}


static rsRetVal
OnFileOpen(void *pT, uchar *fn, void *pGF)
{
	lmcry_gcry_t *pThis = (lmcry_gcry_t*) pT;
	gcryfile *pgf = (gcryfile*) pGF;
	DEFiRet;
dbgprintf("DDDD: cry: onFileOpen: %s\n", fn);
	/* note: if *pgf is set to NULL, this auto-disables GT functions */
	//*pgf = gcryCtxOpenFile(pThis->ctx, fn);

	CHKiRet(rsgcryInitCrypt(pThis->ctx, pgf, GCRY_CIPHER_MODE_CBC, "TODO: init value"));
finalize_it:
	RETiRet;
}

static rsRetVal
Encrypt(void *pF, uchar *rec, size_t *lenRec)
{
	DEFiRet;
dbgprintf("DDDD: Encrypt (%u): %s\n", *lenRec-1, rec);
	iRet = rsgcryEncrypt(pF, rec, lenRec);

	RETiRet;
}

static rsRetVal
OnFileClose(void *pF)
{
	DEFiRet;
dbgprintf("DDDD: onFileClose\n");
	gcryfileDestruct(pF);

	RETiRet;
}

BEGINobjQueryInterface(lmcry_gcry)
CODESTARTobjQueryInterface(lmcry_gcry)
	 if(pIf->ifVersion != cryprovCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}
	pIf->Construct = (rsRetVal(*)(void*)) lmcry_gcryConstruct;
	pIf->SetCnfParam = SetCnfParam;
	pIf->Destruct = (rsRetVal(*)(void*)) lmcry_gcryDestruct;
	pIf->OnFileOpen = OnFileOpen;
	pIf->Encrypt = Encrypt;
	pIf->OnFileClose = OnFileClose;
finalize_it:
ENDobjQueryInterface(lmcry_gcry)


BEGINObjClassExit(lmcry_gcry, OBJ_IS_LOADABLE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(lmcry_gcry)
	/* release objects we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);

	rsgcryExit();
ENDObjClassExit(lmcry_gcry)


BEGINObjClassInit(lmcry_gcry, 1, OBJ_IS_LOADABLE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	if(rsgcryInit() != 0) {
		errmsg.LogError(0, RS_RET_CRYPROV_ERR, "error initializing "
			"crypto provider - cannot encrypt");
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}
ENDObjClassInit(lmcry_gcry)


/* --------------- here now comes the plumbing that makes as a library module --------------- */


BEGINmodExit
CODESTARTmodExit
	lmcry_gcryClassExit();
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_LIB_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
	/* Initialize all classes that are in our module - this includes ourselfs */
	CHKiRet(lmcry_gcryClassInit(pModInfo)); /* must be done after tcps_sess, as we use it */
ENDmodInit
