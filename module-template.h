/* module-template.h
 * This header contains macros that can be used to implement the 
 * plumbing of modules. 
 *
 * File begun on 2007-07-25 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#ifndef	MODULE_TEMPLATE_H_INCLUDED
#define	MODULE_TEMPLATE_H_INCLUDED 1

#include "objomsr.h"

/* macro to define standard output-module static data members
 */
#define DEF_OMOD_STATIC_DATA \
	static rsRetVal (*omsdRegCFSLineHdlr)();

/* to following macros are used to generate function headers and standard
 * functionality. It works as follows (described on the sample case of
 * createInstance()):
 *
 * BEGINcreateInstance
 * ... custom variable definitions (on stack) ... (if any)
 * CODESTARTcreateInstance
 * ... custom code ... (if any)
 * ENDcreateInstance
 */

/* createInstance()
 */
#define BEGINcreateInstance \
static rsRetVal createInstance(instanceData **ppData)\
	{\
	DEFiRet; /* store error code here */\
	instanceData *pData; /* use this to point to data elements */

#define CODESTARTcreateInstance \
	if((pData = calloc(1, sizeof(instanceData))) == NULL) {\
		*ppData = NULL;\
		return RS_RET_OUT_OF_MEMORY;\
	}

#define ENDcreateInstance \
	*ppData = pData;\
	return iRet;\
}

/* freeInstance()
 */
#define BEGINfreeInstance \
static rsRetVal freeInstance(void* pModData)\
{\
	DEFiRet;\
	instanceData *pData;

#define CODESTARTfreeInstance \
	pData = (instanceData*) pModData;

#define ENDfreeInstance \
	if(pData != NULL)\
		free(pData); /* we need to free this in any case */\
	return iRet;\
}

/* isCompatibleWithFeature()
 */
#define BEGINisCompatibleWithFeature \
static rsRetVal isCompatibleWithFeature(syslogFeature __attribute__((unused)) eFeat)\
{\
	rsRetVal iRet = RS_RET_INCOMPATIBLE;

#define CODESTARTisCompatibleWithFeature

#define ENDisCompatibleWithFeature \
	return iRet;\
}

/* doAction()
 */
#define BEGINdoAction \
static rsRetVal doAction(uchar __attribute__((unused)) **ppString, unsigned __attribute__((unused)) iMsgOpts, instanceData __attribute__((unused)) *pData)\
{\
	DEFiRet;

#define CODESTARTdoAction \
	/* ppString may be NULL if the output module requested no strings */

#define ENDdoAction \
	return iRet;\
}


/* dbgPrintInstInfo()
 * Extra comments:
 * Print debug information about this instance.
 */
#define BEGINdbgPrintInstInfo \
static rsRetVal dbgPrintInstInfo(void *pModData)\
{\
	DEFiRet;\
	instanceData *pData = NULL;

#define CODESTARTdbgPrintInstInfo \
	pData = (instanceData*) pModData;

#define ENDdbgPrintInstInfo \
	return iRet;\
}


/* needUDPSocket()
 * Talks back to syslogd if the global UDP syslog socket is needed for
 * sending. Returns 0 if not, 1 if needed. This interface hopefully goes
 * away at some time, because it is kind of a hack. However, currently
 * there is no way around it, so we need to support it.
 * rgerhards, 2007-07-26
 */
#define BEGINneedUDPSocket \
static rsRetVal needUDPSocket(void *pModData)\
{\
	rsRetVal iRet = RS_RET_FALSE;\
	instanceData *pData = NULL;

#define CODESTARTneedUDPSocket \
	pData = (instanceData*) pModData;

#define ENDneedUDPSocket \
	return iRet;\
}


/* onSelectReadyWrite()
 * Extra comments:
 * This is called when select() returned with a writable file descriptor
 * for this module. The fd was most probably obtained by getWriteFDForSelect()
 * before.
 */
#define BEGINonSelectReadyWrite \
static rsRetVal onSelectReadyWrite(void *pModData)\
{\
	rsRetVal iRet = RS_RET_NONE;\
	instanceData *pData = NULL;

#define CODESTARTonSelectReadyWrite \
	pData = (instanceData*) pModData;

#define ENDonSelectReadyWrite \
	return iRet;\
}


/* getWriteFDForSelect()
 * Extra comments:
 * Gets writefd for select call. Must only be returned when the selector must
 * be written to. If the module has no such fds, it must return RS_RET_NONE.
 * In this case, the default implementation is sufficient.
 * This interface will probably go away over time, but we need it now to
 * continue modularization.
 */
#define BEGINgetWriteFDForSelect \
static rsRetVal getWriteFDForSelect(void *pModData, short  __attribute__((unused)) *fd)\
{\
	rsRetVal iRet = RS_RET_NONE;\
	instanceData *pData = NULL;

#define CODESTARTgetWriteFDForSelect \
	assert(fd != NULL);\
	pData = (instanceData*) pModData;

#define ENDgetWriteFDForSelect \
	return iRet;\
}


/* parseSelectorAct()
 * Extra comments:
 * try to process a selector action line. Checks if the action
 * applies to this module and, if so, processed it. If not, it
 * is left untouched. The driver will then call another module.
 * On exit, ppModData must point to instance data. Also, a string
 * request object must be created and filled. A macro is defined
 * for that.
 * For the most usual case, we have defined a macro below.
 * If more than one string is requested, the macro can be used together
 * with own code that overwrites the entry count. In this case, the
 * macro must come before the own code. It is recommended to be
 * placed right after CODESTARTparseSelectorAct.
 */
#define BEGINparseSelectorAct \
static rsRetVal parseSelectorAct(uchar **pp, void **ppModData, omodStringRequest_t **ppOMSR)\
{\
	DEFiRet;\
	uchar *p;\
	instanceData *pData = NULL;

#define CODESTARTparseSelectorAct \
	assert(pp != NULL);\
	assert(ppModData != NULL);\
	assert(ppOMSR != NULL);\
	p = *pp;

#define CODE_STD_STRING_REQUESTparseSelectorAct(NumStrReqEntries) \
	CHKiRet(OMSRconstruct(ppOMSR, NumStrReqEntries));

#define CODE_STD_FINALIZERparseSelectorAct \
finalize_it:\
	if(iRet == RS_RET_OK || iRet == RS_RET_SUSPENDED) {\
		*ppModData = pData;\
		*pp = p;\
	} else {\
		/* cleanup, we failed */\
		if(*ppOMSR != NULL) {\
			OMSRdestruct(*ppOMSR);\
			*ppOMSR = NULL;\
		}\
		if(pData != NULL)\
			freeInstance(&pData);\
	}

#define ENDparseSelectorAct \
	return iRet;\
}


/* tryResume()
 * This entry point is called to check if a module can resume operations. This
 * happens when a module requested that it be suspended. In suspended state,
 * the engine periodically tries to resume the module. If that succeeds, normal
 * processing continues. If not, the module will not be called unless a
 * tryResume() call succeeds.
 * Returns RS_RET_OK, if resumption succeeded, RS_RET_SUSPENDED otherwise
 * rgerhard, 2007-08-02
 */
#define BEGINtryResume \
static rsRetVal tryResume(instanceData __attribute__((unused)) *pData)\
{\
	DEFiRet;

#define CODESTARTtryResume \
	assert(pData != NULL);

#define ENDtryResume \
	return iRet;\
}



/* queryEtryPt()
 */
#define BEGINqueryEtryPt \
static rsRetVal queryEtryPt(uchar *name, rsRetVal (**pEtryPoint)())\
{\
	DEFiRet;

#define CODESTARTqueryEtryPt \
	if((name == NULL) || (pEtryPoint == NULL))\
		return RS_RET_PARAM_ERROR;\
	*pEtryPoint = NULL;

#define ENDqueryEtryPt \
	if(iRet == RS_RET_OK)\
		iRet = (*pEtryPoint == NULL) ? RS_RET_NOT_FOUND : RS_RET_OK;\
	return iRet;\
}

/* the following definition is the standard block for queryEtryPt for output
 * modules. This can be used if no specific handling (e.g. to cover version
 * differences) is needed.
 */
#define CODEqueryEtryPt_STD_OMOD_QUERIES\
	if(!strcmp((char*) name, "doAction")) {\
		*pEtryPoint = doAction;\
	} else if(!strcmp((char*) name, "parseSelectorAct")) {\
		*pEtryPoint = parseSelectorAct;\
	} else if(!strcmp((char*) name, "isCompatibleWithFeature")) {\
		*pEtryPoint = isCompatibleWithFeature;\
	} else if(!strcmp((char*) name, "dbgPrintInstInfo")) {\
		*pEtryPoint = dbgPrintInstInfo;\
	} else if(!strcmp((char*) name, "freeInstance")) {\
		*pEtryPoint = freeInstance;\
	} else if(!strcmp((char*) name, "getWriteFDForSelect")) {\
		*pEtryPoint = getWriteFDForSelect;\
	} else if(!strcmp((char*) name, "onSelectReadyWrite")) {\
		*pEtryPoint = onSelectReadyWrite;\
	} else if(!strcmp((char*) name, "needUDPSocket")) {\
		*pEtryPoint = needUDPSocket;\
	} else if(!strcmp((char*) name, "tryResume")) {\
		*pEtryPoint = tryResume;\
	}

/* modInit()
 * This has an extra parameter, which is the specific name of the modInit
 * function. That is needed for built-in modules, which must have unique
 * names in order to link statically.
 *
 * Extra Comments:
 * initialize the module
 *
 * Later, much more must be done. So far, we only return a pointer
 * to the queryEtryPt() function
 * TODO: do interface version checking & handshaking
 * iIfVersRequeted is the version of the interface specification that the
 * caller would like to see being used. ipIFVersProvided is what we
 * decide to provide.
 */
#define BEGINmodInit(uniqName) \
rsRetVal modInit##uniqName(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()))\
{\
	DEFiRet;

#define CODESTARTmodInit \
	assert(pHostQueryEtryPt != NULL);\
	if((pQueryEtryPt == NULL) || (ipIFVersProvided == NULL))\
		return RS_RET_PARAM_ERROR;

#define ENDmodInit \
finalize_it:\
	*pQueryEtryPt = queryEtryPt;\
	return iRet;\
}


/* definitions for host API queries */
#define CODEmodInit_QueryRegCFSLineHdlr \
	CHKiRet(pHostQueryEtryPt((uchar*)"regCfSysLineHdlr", &omsdRegCFSLineHdlr));

#endif /* #ifndef MODULE_TEMPLATE_H_INCLUDED */
/*
 * vi:set ai:
 */
