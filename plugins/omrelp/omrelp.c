/* omrelp.c
 *
 * This is the implementation of the RELP output module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-03-13 by RGerhards
 *
 * Copyright 2008-2012 Adiscon GmbH.
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
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <librelp.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "cfsysline.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "debug.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omrelp")

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)

static relpEngine_t *pRelpEngine;	/* our relp engine */

typedef struct _instanceData {
	char *f_hname;
	int compressionLevel; /* 0 - no compression, else level for zlib */
	char *port;
	int bInitialConnect; /* is this the initial connection request of our module? (0-no, 1-yes) */
	int bIsConnected; /* currently connected to server? 0 - no, 1 - yes */
	relpClt_t *pRelpClt;		/* relp client for this instance */
} instanceData;

typedef struct configSettings_s {
	EMPTY_STRUCT
} configSettings_t;
static configSettings_t __attribute__((unused)) cs;

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
ENDinitConfVars

/* get the syslog forward port from selector_t. The passed in
 * struct must be one that is setup for forwarding.
 * rgerhards, 2007-06-28
 * We may change the implementation to try to lookup the port
 * if it is unspecified. So far, we use the IANA default auf 514.
 */
static char *getRelpPt(instanceData *pData)
{
	assert(pData != NULL);
	if(pData->port == NULL)
		return("514");
	else
		return(pData->port);
}

BEGINcreateInstance
CODESTARTcreateInstance
	pData->bInitialConnect = 1;
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	if(pData->port != NULL)
		free(pData->port);

	/* final cleanup */
	if(pData->pRelpClt != NULL)
		relpEngineCltDestruct(pRelpEngine, &pData->pRelpClt);

	if(pData->f_hname != NULL)
		free(pData->f_hname);

ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	printf("RELP/%s", pData->f_hname);
ENDdbgPrintInstInfo


/* try to connect to server
 * rgerhards, 2008-03-21
 */
static rsRetVal doConnect(instanceData *pData)
{
	DEFiRet;

	if(pData->bInitialConnect) {
		iRet = relpCltConnect(pData->pRelpClt, glbl.GetDefPFFamily(), (uchar*) pData->port, (uchar*) pData->f_hname);
		if(iRet == RELP_RET_OK)
			pData->bInitialConnect = 0;
	} else {
		iRet = relpCltReconnect(pData->pRelpClt);
	}

	if(iRet == RELP_RET_OK) {
		pData->bIsConnected = 1;
	} else {
		pData->bIsConnected = 0;
		iRet = RS_RET_SUSPENDED;
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	iRet = doConnect(pData);
ENDtryResume


BEGINdoAction
	uchar *pMsg; /* temporary buffering */
	size_t lenMsg;
	relpRetVal ret;
CODESTARTdoAction
	dbgprintf(" %s:%s/RELP\n", pData->f_hname, getRelpPt(pData));

	if(!pData->bIsConnected) {
		CHKiRet(doConnect(pData));
	}

	pMsg = ppString[0];
	lenMsg = strlen((char*) pMsg); /* TODO: don't we get this? */

	/* TODO: think about handling oversize messages! */
	if((int) lenMsg > glbl.GetMaxLine())
		lenMsg = glbl.GetMaxLine();

	/* forward */
	ret = relpCltSendSyslog(pData->pRelpClt, (uchar*) pMsg, lenMsg);
	if(ret != RELP_RET_OK) {
		/* error! */
		dbgprintf("error forwarding via relp, suspending\n");
		iRet = RS_RET_SUSPENDED;
	}

finalize_it:
ENDdoAction


BEGINparseSelectorAct
	uchar *q;
	int i;
	int bErr;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":omrelp:", sizeof(":omrelp:") - 1)) {
		p += sizeof(":omrelp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		FINALIZE;

	/* we are now after the protocol indicator. Now check if we should
	 * use compression. We begin to use a new option format for this:
	 * @(option,option)host:port
	 * The first option defined is "z[0..9]" where the digit indicates
	 * the compression level. If it is not given, 9 (best compression) is
	 * assumed. An example action statement might be:
	 * :omrelp:(z5,o)127.0.0.1:1400  
	 * Which means send via TCP with medium (5) compresion (z) to the local
	 * host on port 1400. The '0' option means that octet-couting (as in
	 * IETF I-D syslog-transport-tls) is to be used for framing (this option
	 * applies to TCP-based syslog only and is ignored when specified with UDP).
	 * That is not yet implemented.
	 * rgerhards, 2006-12-07
	 * TODO: think of all this in spite of RELP -- rgerhards, 2008-03-13
	 */
	if(*p == '(') {
		/* at this position, it *must* be an option indicator */
		do {
			++p; /* eat '(' or ',' (depending on when called) */
			/* check options */
			if(*p == 'z') { /* compression */
#					ifdef USE_NETZIP
				++p; /* eat */
				if(isdigit((int) *p)) {
					int iLevel;
					iLevel = *p - '0';
					++p; /* eat */
					pData->compressionLevel = iLevel;
				} else {
					errmsg.LogError(0, NO_ERRCODE, "Invalid compression level '%c' specified in "
						 "forwardig action - NOT turning on compression.",
						 *p);
				}
#					else
				errmsg.LogError(0, NO_ERRCODE, "Compression requested, but rsyslogd is not compiled "
					 "with compression support - request ignored.");
#					endif /* #ifdef USE_NETZIP */
			} else { /* invalid option! Just skip it... */
				errmsg.LogError(0, NO_ERRCODE, "Invalid option %c in forwarding action - ignoring.", *p);
				++p; /* eat invalid option */
			}
			/* the option processing is done. We now do a generic skip
			 * to either the next option or the end of the option
			 * block.
			 */
			while(*p && *p != ')' && *p != ',')
				++p;	/* just skip it */
		} while(*p && *p == ','); /* Attention: do.. while() */
		if(*p == ')')
			++p; /* eat terminator, on to next */
		else
			/* we probably have end of string - leave it for the rest
			 * of the code to handle it (but warn the user)
			 */
			errmsg.LogError(0, NO_ERRCODE, "Option block not terminated in forwarding action.");
	}
	/* extract the host first (we do a trick - we replace the ';' or ':' with a '\0')
	 * now skip to port and then template name. rgerhards 2005-07-06
	 */
	if(*p == '[') { /* everything is hostname upto ']' */
		++p; /* skip '[' */
		for(q = p ; *p && *p != ']' ; ++p)
			/* JUST SKIP */;
		if(*p == ']') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p; /* eat it */
		}
	} else { /* traditional view of hostname */
		for(q = p ; *p && *p != ';' && *p != ':' && *p != '#' ; ++p)
			/* JUST SKIP */;
	}

	pData->port = NULL;
	if(*p == ':') { /* process port */
		uchar * tmp;

		*p = '\0'; /* trick to obtain hostname (later)! */
		tmp = ++p;
		for(i=0 ; *p && isdigit((int) *p) ; ++p, ++i)
			/* SKIP AND COUNT */;
		pData->port = MALLOC(i + 1);
		if(pData->port == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "Could not get memory to store relp port, "
				 "using default port, results may not be what you intend\n");
			/* we leave f_forw.port set to NULL, this is then handled by getRelpPt() */
		} else {
			memcpy(pData->port, tmp, i);
			*(pData->port + i) = '\0';
		}
	}
	
	/* now skip to template */
	bErr = 0;
	while(*p && *p != ';') {
		if(*p && *p != ';' && !isspace((int) *p)) {
			if(bErr == 0) { /* only 1 error msg! */
				bErr = 1;
				errno = 0;
				errmsg.LogError(0, NO_ERRCODE, "invalid selector line (port), probably not doing "
					 "what was intended");
			}
		}
		++p;
	}

	/* TODO: make this if go away! */
	if(*p == ';') {
		*p = '\0'; /* trick to obtain hostname (later)! */
		CHKmalloc(pData->f_hname = strdup((char*) q));
		*p = ';';
	} else {
		CHKmalloc(pData->f_hname = strdup((char*) q));
	}

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) "RSYSLOG_ForwardFormat"));

	/* create our relp client  */
	CHKiRet(relpEngineCltConstruct(pRelpEngine, &pData->pRelpClt)); /* we use CHKiRet as librelp has a similar return value range */

	/* TODO: do we need to call freeInstance if we failed - this is a general question for
	 * all output modules. I'll address it later as the interface evolves. rgerhards, 2007-07-25
	 */
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	relpEngineDestruct(&pRelpEngine);

	/* release what we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES 
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* create our relp engine */
	CHKiRet(relpEngineConstruct(&pRelpEngine));
	CHKiRet(relpEngineSetDbgprint(pRelpEngine, dbgprintf));
	CHKiRet(relpEngineSetEnableCmd(pRelpEngine, (uchar*) "syslog", eRelpCmdState_Required));

	/* tell which objects we need */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDmodInit

/* vim:set ai:
 */
