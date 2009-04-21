/* module-template.h
 * This header contains macros that can be used to implement the 
 * plumbing of modules. 
 *
 * File begun on 2007-07-25 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef	MODULE_TEMPLATE_H_INCLUDED
#define	MODULE_TEMPLATE_H_INCLUDED 1

#include "modules.h"
#include "obj.h"
#include "objomsr.h"
#include "threads.h"

/* macro to define standard output-module static data members
 */
#define DEF_MOD_STATIC_DATA \
	static __attribute__((unused)) rsRetVal (*omsdRegCFSLineHdlr)();

#define DEF_OMOD_STATIC_DATA \
	DEF_MOD_STATIC_DATA \
	DEFobjCurrIf(obj)
#define DEF_IMOD_STATIC_DATA \
	DEF_MOD_STATIC_DATA \
	DEFobjCurrIf(obj)
#define DEF_LMOD_STATIC_DATA \
	DEF_MOD_STATIC_DATA


/* Macro to define the module type. Each module can only have a single type. If
 * a module provides multiple types, several separate modules must be created which
 * then should share a single library containing the majority of code. This macro
 * must be present in each module. -- rgerhards, 2007-12-14
 * Note that MODULE_TYPE_TESTBENCH is reserved for testbenches, but
 * declared in their own header files (because the rest does not need these
 * defines). -- rgerhards, 2008-06-13
 */
#define MODULE_TYPE(x)\
static rsRetVal modGetType(eModType_t *modType) \
	{ \
		*modType = x; \
		return RS_RET_OK;\
	}

#define MODULE_TYPE_INPUT MODULE_TYPE(eMOD_IN)
#define MODULE_TYPE_OUTPUT MODULE_TYPE(eMOD_OUT)
#define MODULE_TYPE_LIB \
	DEF_LMOD_STATIC_DATA \
	MODULE_TYPE(eMOD_LIB)


/* macro to define a unique module id. This must be able to fit in a void*. The
 * module id must be unique inside a running rsyslogd application. It is used to
 * track ownership of several objects. Most importantly, when the module is 
 * unloaded the module id value is used to find what needs to be destroyed.
 * We currently use a pointer to modExit() as the module id. This sounds to be 
 * reasonable save, as each module must have this entry point AND there is no valid
 * reason for twice this entry point being in memory.
 * rgerhards, 2007-11-21
 */
#define STD_LOADABLE_MODULE_ID ((void*) modExit)


/* macro to implement the "modGetID()" interface function
 * rgerhards 2007-11-21
 */
#define DEFmodGetID \
static rsRetVal modGetID(void **pID) \
	{ \
		*pID = STD_LOADABLE_MODULE_ID;\
		return RS_RET_OK;\
	}

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
		ENDfunc \
		return RS_RET_OUT_OF_MEMORY;\
	}

#define ENDcreateInstance \
	*ppData = pData;\
	RETiRet;\
}

/* freeInstance()
 * This is the cleanup function for the module instance. It is called immediately before
 * the module instance is destroyed (unloaded). The module should do any cleanup
 * here, e.g. close file, free instantance heap memory and the like. Control will
 * not be passed back to the module once this function is finished. Keep in mind,
 * however, that other instances may still be loaded and used. So do not destroy
 * anything that may be used by another instance. If you have such a ressource, you
 * currently need to do the instance counting yourself.
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
	RETiRet;\
}

/* isCompatibleWithFeature()
 */
#define BEGINisCompatibleWithFeature \
static rsRetVal isCompatibleWithFeature(syslogFeature __attribute__((unused)) eFeat)\
{\
	rsRetVal iRet = RS_RET_INCOMPATIBLE; \
	BEGINfunc

#define CODESTARTisCompatibleWithFeature

#define ENDisCompatibleWithFeature \
	RETiRet;\
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
	RETiRet;\
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
	RETiRet;\
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
		if(pData != NULL) {\
			freeInstance(pData);\
		} \
	}

#define ENDparseSelectorAct \
	RETiRet;\
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
	RETiRet;\
}



/* queryEtryPt()
 */
#define BEGINqueryEtryPt \
DEFmodGetID \
static rsRetVal queryEtryPt(uchar *name, rsRetVal (**pEtryPoint)())\
{\
	DEFiRet;

#define CODESTARTqueryEtryPt \
	if((name == NULL) || (pEtryPoint == NULL)) {\
		ENDfunc \
		return RS_RET_PARAM_ERROR;\
	} \
	*pEtryPoint = NULL;

#define ENDqueryEtryPt \
	if(iRet == RS_RET_OK)\
		if(*pEtryPoint == NULL) { \
			dbgprintf("entry point '%s' not present in module\n", name); \
			iRet = RS_RET_MODULE_ENTRY_POINT_NOT_FOUND;\
		} \
	RETiRet;\
}

/* the following definition is the standard block for queryEtryPt for all types
 * of modules. It should be included in any module, and typically is so by calling
 * the module-type specific macros.
 */
#define CODEqueryEtryPt_STD_MOD_QUERIES \
	if(!strcmp((char*) name, "modExit")) {\
		*pEtryPoint = modExit;\
	} else if(!strcmp((char*) name, "modGetID")) {\
		*pEtryPoint = modGetID;\
	} else if(!strcmp((char*) name, "getType")) {\
		*pEtryPoint = modGetType;\
	}

/* the following definition is the standard block for queryEtryPt for output
 * modules. This can be used if no specific handling (e.g. to cover version
 * differences) is needed.
 */
#define CODEqueryEtryPt_STD_OMOD_QUERIES \
	CODEqueryEtryPt_STD_MOD_QUERIES \
	else if(!strcmp((char*) name, "doAction")) {\
		*pEtryPoint = doAction;\
	} else if(!strcmp((char*) name, "dbgPrintInstInfo")) {\
		*pEtryPoint = dbgPrintInstInfo;\
	} else if(!strcmp((char*) name, "freeInstance")) {\
		*pEtryPoint = freeInstance;\
	} else if(!strcmp((char*) name, "parseSelectorAct")) {\
		*pEtryPoint = parseSelectorAct;\
	} else if(!strcmp((char*) name, "isCompatibleWithFeature")) {\
		*pEtryPoint = isCompatibleWithFeature;\
	} else if(!strcmp((char*) name, "tryResume")) {\
		*pEtryPoint = tryResume;\
	}

/* the following definition is the standard block for queryEtryPt for INPUT
 * modules. This can be used if no specific handling (e.g. to cover version
 * differences) is needed.
 */
#define CODEqueryEtryPt_STD_IMOD_QUERIES \
	CODEqueryEtryPt_STD_MOD_QUERIES \
	else if(!strcmp((char*) name, "runInput")) {\
		*pEtryPoint = runInput;\
	} else if(!strcmp((char*) name, "willRun")) {\
		*pEtryPoint = willRun;\
	} else if(!strcmp((char*) name, "afterRun")) {\
		*pEtryPoint = afterRun;\
	}

/* the following definition is the standard block for queryEtryPt for LIBRARY
 * modules. This can be used if no specific handling (e.g. to cover version
 * differences) is needed.
 */
#define CODEqueryEtryPt_STD_LIB_QUERIES \
	CODEqueryEtryPt_STD_MOD_QUERIES

/* modInit()
 * This has an extra parameter, which is the specific name of the modInit
 * function. That is needed for built-in modules, which must have unique
 * names in order to link statically. Please note that this is always only
 * the case with modInit() and NO other entry point. The reason is that only
 * modInit() is visible form a linker/loader point of view. All other entry
 * points are passed via rsyslog-internal query functions and are defined
 * static inside the modules source. This is an important concept, as it allows
 * us to support different interface versions within a single module. (Granted,
 * we do not currently have different interface versions, so we can not put
 * it to a test - but our firm believe is that we can do all abstraction needed...)
 *
 * Extra Comments:
 * initialize the module
 *
 * Later, much more must be done. So far, we only return a pointer
 * to the queryEtryPt() function
 * TODO: do interface version checking & handshaking
 * iIfVersRequetsed is the version of the interface specification that the
 * caller would like to see being used. ipIFVersProvided is what we
 * decide to provide.
 * rgerhards, 2007-11-21: see modExit() comment below for important information
 * on the need to initialize static data with code. modInit() may be called on a
 * cached, left-in-memory copy of a previous incarnation.
 */
#define BEGINmodInit(uniqName) \
rsRetVal modInit##uniqName(int iIFVersRequested __attribute__((unused)), int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(), rsRetVal (*pHostQueryEtryPt)(uchar*, rsRetVal (**)()), modInfo_t __attribute__((unused)) *pModInfo)\
{\
	DEFiRet; \
	rsRetVal (*pObjGetObjInterface)(obj_if_t *pIf);

#define CODESTARTmodInit \
	assert(pHostQueryEtryPt != NULL);\
	iRet = pHostQueryEtryPt((uchar*)"objGetObjInterface", &pObjGetObjInterface); \
	if((iRet != RS_RET_OK) || (pQueryEtryPt == NULL) || (ipIFVersProvided == NULL) || (pObjGetObjInterface == NULL)) { \
		ENDfunc \
		return (iRet == RS_RET_OK) ? RS_RET_PARAM_ERROR : iRet; \
	} \
	/* now get the obj interface so that we can access other objects */ \
	CHKiRet(pObjGetObjInterface(&obj));

#define ENDmodInit \
finalize_it:\
	*pQueryEtryPt = queryEtryPt;\
	RETiRet;\
}


/* definitions for host API queries */
#define CODEmodInit_QueryRegCFSLineHdlr \
	CHKiRet(pHostQueryEtryPt((uchar*)"regCfSysLineHdlr", &omsdRegCFSLineHdlr));

#endif /* #ifndef MODULE_TEMPLATE_H_INCLUDED */

/* modExit()
 * This is the counterpart to modInit(). It destroys a module and makes it ready for
 * unloading. It is similiar to freeInstance() for the instance data. Please note that
 * this entry point needs to free any module-global data structures and registrations.
 * For example, the CfSysLineHandlers a module has registered need to be unregistered
 * here. This entry point is only called immediately before unloading of the module. So
 * it is likely to be destroyed. HOWEVER, the caller may decide to keep the module cached.
 * So a module must never assume that it is actually destroyed. A call to modInit() may
 * happen immediately after modExit(). So a module can NOT assume that static data elements
 * are being re-initialized by the loader - this must always be done by module code itself.
 * It is suggested to do this in modInit(). - rgerhards, 2007-11-21
 */
#define BEGINmodExit \
static rsRetVal modExit(void)\
{\
	DEFiRet;

#define CODESTARTmodExit 

#define ENDmodExit \
	RETiRet;\
}


/* runInput()
 * This is the main function for input modules. It is used to gather data from the
 * input source and submit it to the message queue. Each runInput() instance has its own
 * thread. This is handled by the rsyslog engine. It needs to spawn off new threads only
 * if there is a module-internal need to do so.
 */
#define BEGINrunInput \
static rsRetVal runInput(thrdInfo_t __attribute__((unused)) *pThrd)\
{\
	DEFiRet;

#define CODESTARTrunInput \
	dbgSetThrdName((uchar*)__FILE__); /* we need to provide something better later */

#define ENDrunInput \
	RETiRet;\
}


/* willRun()
 * This is a function that will be replaced in the longer term. It is used so
 * that a module can tell the caller if it will run or not. This is to be replaced
 * when we introduce input module instances. However, these require config syntax
 * changes and I may (or may not... ;)) hold that until another config file 
 * format is available. -- rgerhards, 2007-12-17
 * returns RS_RET_NO_RUN if it will not run (RS_RET_OK or error otherwise)
 */
#define BEGINwillRun \
static rsRetVal willRun(void)\
{\
	DEFiRet;

#define CODESTARTwillRun 

#define ENDwillRun \
	RETiRet;\
}


/* afterRun()
 * This function is called after an input module has been run and its thread has
 * been terminated. It shall do any necessary cleanup.
 * This is expected to evolve into a freeInstance type of call once the input module
 * interface evolves to support multiple instances.
 * rgerhards, 2007-12-17
 */
#define BEGINafterRun \
static rsRetVal afterRun(void)\
{\
	DEFiRet;

#define CODESTARTafterRun 

#define ENDafterRun \
	RETiRet;\
}


/* doHUP()
 * This function is optional. Currently, it is available to output plugins
 * only, but may be made available to other types of plugins in the future.
 * A plugin does not need to define this entry point. If if does, it gets
 * called when a non-restart type of HUP is done. A plugin should register
 * this function so that it can close files, connection or other ressources
 * on HUP - if it can be assume the user wanted to do this as a part of HUP
 * processing. Note that the name "HUP" has historical reasons, it stems back
 * to the infamous SIGHUP which was sent to restart a syslogd. We still retain
 * that legacy, but may move this to a different signal.
 * rgerhards, 2008-10-22
 */
#define CODEqueryEtryPt_doHUP \
	else if(!strcmp((char*) name, "doHUP")) {\
		*pEtryPoint = doHUP;\
	}
#define BEGINdoHUP \
static rsRetVal doHUP(instanceData __attribute__((unused)) *pData)\
{\
	DEFiRet;

#define CODESTARTdoHUP 

#define ENDdoHUP \
	RETiRet;\
}


/* vim:set ai:
 */
