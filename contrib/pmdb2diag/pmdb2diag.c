/* pmdb2diag.c
 *
 * This is a parser module specifically for DB2diag log file parsing.
 * It extracted programs, pids and error level from the log.
 *
 * Copyright 2015 Philippe Duveau @ Pari Mutuel Urbain.
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
#define _XOPEN_SOURCE
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "glbl.h"
#include "errmsg.h"
#include "parser.h"
#include "datetime.h"
#include "unicode-helper.h"

MODULE_TYPE_PARSER
MODULE_TYPE_NOKEEP
PARSER_NAME("db2.diag")
MODULE_CNFNAME("pmdb2diag")

/* internal structures
 */
DEF_PMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(parser)
DEFobjCurrIf(datetime)

/* input instance parameters */
static struct cnfparamdescr parserpdescr[] = {
	{ "levelpos", eCmdHdlrInt, 0 },
	{ "timepos", eCmdHdlrInt, 0 },
	{ "timeformat", eCmdHdlrString, 0 },
	{ "parsetag", eCmdHdlrString, 0 },
};
static struct cnfparamblk parserpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(parserpdescr)/sizeof(struct cnfparamdescr),
	  parserpdescr
	};

struct instanceConf_s {
	int levelpos;
	char *timeformat;
	int timepos;
	char sepSec;
};

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATUREAutomaticPRIParsing)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static  char *levels = "CritAlerSeveErroWarnEvenInfoDebu";
static const int levels_len[] = { 8, 5, 6, 5, 7, 5, 4, 5 };

BEGINparse2
	int i, microsec = 0, tz = 0;
	int32_t lv, *p;
	struct tm tm;
	char *ms, *t, *e, *f, *prog, *pid, *val, *end;
  int lprog, lpid;
	int32_t* nlevels = (int32_t*)levels;
  msgPropDescr_t pProp;
  rs_size_t valLen;
  unsigned short mustBeFreed;
  char procid[128];
CODESTARTparse2
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);

	dbgprintf("Message will now be parsed by \"db2diag\" parser.\n");
	if(pMsg->iLenRawMsg - pMsg->offAfterPRI < pInst->levelpos+4)
		ABORT_FINALIZE(RS_RET_COULD_NOT_PARSE);

	/* this takes the 4 first chars of level read it as int32 to reduce CPU cycles in comparing */
	p = (int*)(pMsg->pszRawMsg + pMsg->offAfterPRI + pInst->levelpos);
	lv = *p;

	end = (char*)pMsg->pszRawMsg + pMsg->iLenRawMsg ;

	for (i=0; i<8 && *nlevels!=lv; nlevels++, i++)
		;

	if (i < 8)
		pMsg->iSeverity = i;
	else
    ABORT_FINALIZE(0);

	t = (char*)pMsg->pszRawMsg + pMsg->offAfterPRI + pInst->timepos;

	dbgprintf("db2parse Time %.20s\n", t);
	ms = strptime(t, pInst->timeformat, &tm);

	if (ms > t &&  *(ms-1) == pInst->sepSec)
	{
		t = strchr(ms, '+');
		if (!t) t = strchr(ms, '-');
		if (!t) t = "+";

		sscanf(ms, (*t == '+') ? "%d+%d " : "%d-%d ", &microsec, &tz);

		pMsg->tTIMESTAMP.year = tm.tm_year+1900;
		pMsg->tTIMESTAMP.month = tm.tm_mon + 1;
		pMsg->tTIMESTAMP.day = tm.tm_mday;
		pMsg->tTIMESTAMP.hour = tm.tm_hour;
		pMsg->tTIMESTAMP.minute = tm.tm_min;
		pMsg->tTIMESTAMP.second = tm.tm_sec;
		pMsg->tTIMESTAMP.secfrac = microsec;
		pMsg->tTIMESTAMP.secfracPrecision = t-ms;
		pMsg->tTIMESTAMP.OffsetMode = *t;
		pMsg->tTIMESTAMP.OffsetHour = tz / 60;
		pMsg->tTIMESTAMP.OffsetMinute = tz % 60;

	pid = (char*)pMsg->pszRawMsg + pInst->levelpos + levels_len[i] + 11;
	if (pid>=end) ABORT_FINALIZE(0);
	lpid = strchr(pid, ' ') - pid;

	prog = pid + 49;
	if (prog>=end) ABORT_FINALIZE(0);

	e = strchr(prog, ' ');
	if (e && e>=end) ABORT_FINALIZE(0);

	f = strchr(prog, '\\');
	if (!f || f>=end) ABORT_FINALIZE(0);

	lprog = (e && e<f) ? e-prog : f-prog;
	lprog = (CONF_PROGNAME_BUFSIZE-1 < lprog) ? CONF_PROGNAME_BUFSIZE-1 : lprog;

	strncpy((char*)pMsg->PROGNAME.szBuf, prog, lprog);
	pMsg->PROGNAME.szBuf[lprog] = '\0';

	snprintf(procid, 128, "%.*s.%.*s", lprog, prog, lpid, pid);
	MsgSetPROCID(pMsg, procid);

	pProp.id = PROP_SYSLOGTAG;
	val = (char*)MsgGetProp(pMsg, NULL, &pProp, &valLen, &mustBeFreed, NULL);
	MsgSetAPPNAME(pMsg, val);
	if (mustBeFreed)
		free(val);
	}

finalize_it:
ENDparse2


BEGINfreeParserInst
CODESTARTfreeParserInst
	free(pInst->timeformat);
ENDfreeParserInst

static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;
	CHKmalloc(inst = (instanceConf_t *)malloc(sizeof(instanceConf_t)));
	inst->timeformat = NULL;
	inst->levelpos = 59;
	inst->timepos = 0;
	*pinst = inst;
finalize_it:
	RETiRet;
}

BEGINnewParserInst
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTnewParserInst
  inst = NULL;

	DBGPRINTF("newParserInst (pmdb2diag)\n");
	CHKiRet(createInstance(&inst));

	if (lst)
	{
		if((pvals = nvlstGetParams(lst, &parserpblk, NULL)) == NULL) {
			ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
		}

		if(Debug) {
			dbgprintf("parser param blk in pmdb2diag:\n");
			cnfparamsPrint(&parserpblk, pvals);
		}

		for(i = 0 ; i < parserpblk.nParams ; ++i) {
			if(!pvals[i].bUsed)
				continue;
			if(!strcmp(parserpblk.descr[i].name, "timeformat")) {
				inst->timeformat = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
			} else if(!strcmp(parserpblk.descr[i].name, "timepos")) {
				inst->timepos = (int)pvals[i].val.d.n;
			} else if(!strcmp(parserpblk.descr[i].name, "levelpos")) {
				inst->levelpos = (int) pvals[i].val.d.n;
			} else {
				dbgprintf("pmdb2diag: program error, non-handled "
				  "param '%s'\n", parserpblk.descr[i].name);
			}
		}
	}

	if (inst->timeformat == NULL)
	{
		inst->timeformat = strdup("%Y-%m-%d-%H.%M.%S.");
		inst->sepSec = '.';
	}else
		inst->sepSec = inst->timeformat[strlen(inst->timeformat)-1];

	dbgprintf("pmdb2diag: parsing date/time with '%s' at position %d and level at position %d.\n",
			inst->timeformat, inst->timepos, inst->levelpos);

finalize_it:
CODE_STD_FINALIZERnewParserInst
	if(lst != NULL)
		cnfparamvalsDestruct(pvals, &parserpblk);
ENDnewParserInst

BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(parser, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_PMOD2_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(parser, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));

	dbgprintf("pmdb2diag parser init called, compiled with version %s\n", VERSION);
ENDmodInit

/* vim:set ai:
 */
