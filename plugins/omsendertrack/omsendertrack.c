/* omsendertrack
 *
 * Copyright 2025 Adiscon GmbH.
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
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "hashtable.h"
#include "hashtable_itr.h"
//#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omsendertrack")

//static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA

#define DEFAULT_INTERVAL 60 /* seconds */

/* module data structures */


typedef struct {
	const uchar *sender;
	uint64_t nMsgs;
	time_t lastSeen;
	time_t firstSeen;
} sender_stats_t;


/* config variables */

typedef struct _instanceData {
	int interval;
	uchar *statefile;
	uchar *cmdfile;
	uchar *templateName; // TODO: keep this as "template" or "sender-id"?
	pthread_mutex_t mutSenders;
	struct hashtable *stats_senders;
	int bShutdownBackgroundWriter;
	pthread_t bgw_tid;	/* thread ID of background writer */
} instanceData;

typedef struct wrkrInstanceData { // TODO: we probably want multiple workers, but then we need atomic updates!
	instanceData *pData;
} wrkrInstanceData_t;

#if 0
typedef struct configSettings_s { // TODO: remove
} configSettings_t;
static configSettings_t cs;
#endif

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "interval", eCmdHdlrPositiveInt, 0 },
	{ "statefile", eCmdHdlrString, 0 },
	{ "cmdfile", eCmdHdlrString, 0 },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

struct modConfData_s {
	rsconf_t *pConf;	/* our overall config object */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current exec process */



#if 0
/* this function obtains all sender stats. 
 */
static void
getSenderStats(instanceData *const pData,
	rsRetVal(*cb)(void*, const char*),
	void *usrptr)
{
	struct hashtable_itr *itr = NULL;
	sender_stats_t *stat;
	char fmtbuf[2048];

	pthread_mutex_lock(&pData->mutSenders);

	/* Iterator constructor only returns a valid iterator if
	 * the hashtable is not empty
	 */
	if(hashtable_count(pData->stats_senders) > 0) {
		itr = hashtable_iterator(pData->stats_senders);
		do {
			stat = (sender_stats_t*)hashtable_iterator_value(itr);
			// TODO: add proper content
			snprintf(fmtbuf, sizeof(fmtbuf),
				"{ \"name\":\"_sender_stat\", "
				"\"origin\":\"impstats\", "
				"\"sender\":\"%s\", \"messages\":%"
				PRIu64 "}",
				stat->sender, stat->nMsgs);
			fmtbuf[sizeof(fmtbuf)-1] = '\0';
			cb(usrptr, fmtbuf);
		} while (hashtable_iterator_advance(itr));
	}

	free(itr);
	pthread_mutex_unlock(&pData->mutSenders);
}
#endif

static rsRetVal
writeSenderInfo(const char *const fname, const char *data)
{

	DEFiRet;
	char tmpname[1024]; // TODO: size!
	snprintf(tmpname, sizeof(tmpname), "%s.tmp", fname);

	int fd = open(tmpname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd < 0) {
		LogError(errno, RS_RET_IO_ERROR,
			"omsendertrack could not create new temp sender file");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	size_t len = strlen(data);
	ssize_t written = write(fd, data, len);
	if (written != (ssize_t)len) {
		unlink(tmpname); // Remove partial file
		LogError(errno, RS_RET_IO_ERROR,
			"omsendertrack write to temp sender file failed");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	close(fd);
	fd = -1;

	// Atomically replace the old file with the new one
	if (rename(tmpname, fname) != 0) {
		unlink(tmpname);
		LogError(errno, RS_RET_IO_ERROR,
			"omsendertrack rename to configure file name failed");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

finalize_it:
	if(iRet != RS_RET_OK) {
		close(fd);
	}
	RETiRet;
}

/* background writing thread for sender stats */
static void *
bgWriter(void *arg)
{
	instanceData *pData = (instanceData *) arg;


	uchar thrdName[32];
	snprintf((char*)thrdName, sizeof(thrdName), "omsendertrack/bgw"); // TODO: instanceientifier?
	dbgSetThrdName(thrdName);
	dbgprintf("bgWriter started\n");

	/* set thread name - we ignore if it fails, has no harsh consequences... */
#	if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
	if(prctl(PR_SET_NAME, thrdName, 0, 0, 0) != 0) {
		DBGPRINTF("prctl failed, not setting thread name for '%s'\n", thrdName);
	}
#	elif defined(HAVE_PTHREAD_SETNAME_NP)
	int r = pthread_setname_np(pthread_self(), (char*) thrdName);
	if(r != 0) {
		DBGPRINTF("pthread_setname_np failed, not setting thread name for '%s'\n", thrdName);
	}
#	endif

	while(pData->bShutdownBackgroundWriter == 0) {
		srSleep(1, 0);
		if(pData->bShutdownBackgroundWriter == 1) {
			break;
		}
		// TODO: add call to do actual write
dbgprintf("bgwriter writing report file\n");
	}

	dbgprintf("bgWriter finished\n");
	return NULL;
}

static rsRetVal
recordSender(instanceData *const pData, const uchar *const sender, const time_t lastSeen)
{
	sender_stats_t* stat;
	int mustUnlock = 0;
	DEFiRet;

	assert(pData->stats_senders != NULL);

	pthread_mutex_lock(&pData->mutSenders);
	mustUnlock = 1;
	stat = hashtable_search(pData->stats_senders, (void*)sender);
	if(stat == NULL) {
		DBGPRINTF("recordSender: sender '%s' not found, adding\n", sender);
		CHKmalloc(stat = calloc(1, sizeof(sender_stats_t)));
		stat->sender = (const uchar*)strdup((const char*)sender);
		stat->nMsgs = 0;
		stat->firstSeen = lastSeen;
		if(hashtable_insert(pData->stats_senders, (void*)stat->sender, (void*)stat) == 0) {
			LogError(errno, RS_RET_INTERNAL_ERROR,
				"omsendertrack error inserting sender '%s' into sender "
				"hash table", sender);
			ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
		}
	}

	stat->nMsgs++; // TODO: later atomic?
	stat->lastSeen = lastSeen;
	DBGPRINTF("omsendertrack: recordSender: '%s', nmsgs now %llu, lastSeen %llu\n", sender,
		(long long unsigned) stat->nMsgs, (long long unsigned) lastSeen);

finalize_it:
	if(mustUnlock)
		pthread_mutex_unlock(&pData->mutSenders);
	RETiRet;
}

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
ENDinitConfVars

BEGINcreateInstance
CODESTARTcreateInstance
	pthread_mutex_init(&pData->mutSenders, NULL);

	if((pData->stats_senders = create_hashtable(100, hash_from_string, key_equals_string, NULL)) == NULL) {
		LogError(0, RS_RET_INTERNAL_ERROR, "error trying to initialize hash-table "
			"for sender table. Sender statistics and warnings are disabled.");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR); // TODO: check status!
	}

	/* read existing data */
	// TODO: implement

	/* background writer */
	pthread_mutex_init(&pData->mutSenders, NULL);
	if(pthread_create(&pData->bgw_tid, NULL, bgWriter, pData) != 0) {
		LogError(0, RS_RET_ERR, "omsendertrack: cannot create background writing thread. "
				"No interim files will be written!");
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
ENDcreateInstance


BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINendCnfLoad
CODESTARTendCnfLoad
	loadModConf = NULL; /* done loading */
ENDendCnfLoad

BEGINcheckCnf
CODESTARTcheckCnf
ENDcheckCnf

BEGINactivateCnf
CODESTARTactivateCnf
	runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
CODESTARTfreeCnf
ENDfreeCnf


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
CODESTARTfreeInstance
	/* stop bgWriter */
	pData->bShutdownBackgroundWriter = 1;
	// TODO: implement this and the following. We need a wakeup, e.g. via SIGTTIN

	/* wait until stopped */
	dbgprintf("waiting for bgWriter to finish\n");
	pthread_join(pData->bgw_tid, NULL);

	/* do final write */

	/* destroy data structs */
	pthread_mutex_destroy(&pData->mutSenders);
	hashtable_destroy(pData->stats_senders, 1);
ENDfreeInstance


BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("omsendertrack\n");
	dbgprintf("\ttemplate='%s'\n", pData->templateName);
ENDdbgPrintInstInfo


BEGINtryResume
CODESTARTtryResume
ENDtryResume

BEGINbeginTransaction
CODESTARTbeginTransaction
ENDbeginTransaction

BEGINcommitTransaction
CODESTARTcommitTransaction
	const time_t lastSeen = time(NULL); /* do only query once per TX - it's sufficiently precise */
	for(unsigned i = 0 ; i < nParams ; ++i) {
		recordSender(pWrkrData->pData, actParam(pParams, 1, i, 0).param, lastSeen);
	}
ENDcommitTransaction


static void
setInstParamDefaults(instanceData *pData)
{
	pData->interval = DEFAULT_INTERVAL;
	pData->templateName = (uchar*) "TODO_WHICH_ONE"; // TODO
}


BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	int bDestructPValsOnExit;
	uchar *tplToUse;
CODESTARTnewActInst
	DBGPRINTF("newActInst (omsendertrack)\n");

	bDestructPValsOnExit = 0;
	pvals = nvlstGetParams(lst, &actpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "omsendertrack: error reading "
				"config parameters");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}
	bDestructPValsOnExit = 1;

	if(Debug) {
		dbgprintf("action param blk in omsendertrack:\n");
		cnfparamsPrint(&actpblk, pvals);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed) {
			continue;
		} else if(!strcmp(actpblk.descr[i].name, "interval")) {
			pData->interval = (int) pvals[i].val.d.n;
		// TODO: add statefile, cmdfile
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->templateName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			DBGPRINTF("omsendertrack: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}


	CODE_STD_STRING_REQUESTnewActInst(1)
	//TODO: make the template a parameter
	tplToUse = (uchar*) strdup((pData->templateName == NULL) ? "RSYSLOG_FileFormat" : (char *)pData->templateName);
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));
CODE_STD_FINALIZERnewActInst
	if(bDestructPValsOnExit)
		cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst



BEGINparseSelectorAct
	int iTplOpts;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	// TODO: check if we can opt out of legacy support at all
	/* first check if this config line is actually for us */
	if(strncmp((char*) p, ":omsendertrack:", sizeof(":omsendertrack:") - 1)) {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	p += sizeof(":omsendertrack:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	CHKiRet(createInstance(&pData));

	/* check if a non-standard template is to be applied */
	if(*(p-1) == ';')
		--p;
	iTplOpts = 0;
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, iTplOpts, (uchar*) "RSYSLOG_FileFormat"));
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMODTX_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	/* old-style system not supported */
ENDmodInit
