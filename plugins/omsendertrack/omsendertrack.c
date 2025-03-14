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

// TODO CI: a) more tests, multiple hosts, b) make current tests really check the counted stats, not just abort ;-)

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
	pthread_mutex_t mut;
} sender_stats_t;


/* config variables */

typedef struct _instanceData {
	int interval;
	uchar *statefile;
	uchar *cmdfile;
	uchar *templateName; // TODO: keep this as "template" or "sender-id"?
	pthread_rwlock_t mutSenders;
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

/* forward references */
static rsRetVal initHashtable(instanceData *const pData);
static void * bgWriter(void *arg);


static rsRetVal
addSender(instanceData *const pData,
	const char *const sender, const uint64_t nMsgs,
	const time_t firstSeen, const time_t lastSeen)
{
	sender_stats_t* stat;
	DEFiRet;

	DBGPRINTF("addSender: Sender: %s, Messages: %" PRIu64 ", FirstSeen: %ld, LastSeen: %ld\n",
		sender, nMsgs, firstSeen, lastSeen);

	CHKmalloc(stat = calloc(1, sizeof(sender_stats_t)));
	stat->sender = (const uchar*)strdup((const char*)sender);
	stat->nMsgs = nMsgs;
	stat->firstSeen = firstSeen;
	stat->lastSeen = lastSeen;
	pthread_mutex_init(&stat->mut, NULL);
	if(hashtable_insert(pData->stats_senders, (void*)stat->sender, (void*)stat) == 0) {
		LogError(errno, RS_RET_INTERNAL_ERROR,
			"omsendertrack error inserting sender '%s' into sender "
			"hash table - entry lost", sender);
		free(stat);
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
	}

finalize_it:
	RETiRet;
}


#define CHUNK_SIZE 16*1024  // Read file in 16KiB chunks
static rsRetVal ATTR_NONNULL()
readSenderStats(instanceData *const pData, json_object **jsontree)
{
	DEFiRet;

	*jsontree = NULL;
	FILE *fp = fopen((const char*) pData->statefile, "r");
	// TODO: in case of read errors, shall we set the sender info file to a different, configurable, name?
	// TODO: check error codes returned!
	if(!fp) {
		LogMsg(errno, RS_RET_OK_WARN, LOG_ERR, "error opening sender info file '%s' - "
			"starting without any previous data", pData->statefile);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	json_tokener *tok = json_tokener_new();
	if (!tok) {
		LogMsg(errno, RS_RET_OK_WARN, LOG_ERR, "error creating json_tokener - "
			"starting without any previous data");
		fclose(fp);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	char buffer[CHUNK_SIZE];
	json_object *parsed_json = NULL;
	enum json_tokener_error jerr;

	while (fgets(buffer, sizeof(buffer), fp)) {
		parsed_json = json_tokener_parse_ex(tok, buffer, strlen(buffer));
		jerr = fjson_tokener_get_error(tok);

		if (jerr == json_tokener_continue) {
			continue;  // Need more data, keep reading
		} else if (jerr != fjson_tokener_success) {
			LogMsg(errno, RS_RET_OK_WARN, LOG_ERR, "error processing input json - "
				"starting without any previous data, error: %s",
				json_tokener_error_desc(jerr));
			json_object_put(parsed_json);
			parsed_json = NULL;
			break;
		}
	}

	json_tokener_free(tok);
	fclose(fp);

	*jsontree = parsed_json;
finalize_it:
	RETiRet;
}

static rsRetVal
jsonToHashtable(instanceData *const pData, json_object *const jsonTree)
{
	DEFiRet;

	if (!json_object_is_type(jsonTree, json_type_array)) {
		fprintf(stderr, "Error: Expected a JSON array.\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	size_t array_len = json_object_array_length(jsonTree);
	for (size_t i = 0; i < array_len; i++) {
		json_object *entry = json_object_array_get_idx(jsonTree, i);

		// Extract fields from each object
		json_object *j_sender, *j_messages, *j_firstseen, *j_lastseen;
		if (json_object_object_get_ex(entry, "sender", &j_sender) &&
		json_object_object_get_ex(entry, "messages", &j_messages) &&
		json_object_object_get_ex(entry, "firstseen", &j_firstseen) &&
		json_object_object_get_ex(entry, "lastseen", &j_lastseen)) {
			// Convert to C types
			const char *sender = json_object_get_string(j_sender);
			int messages = json_object_get_int(j_messages);
			long firstseen = json_object_get_int64(j_firstseen);
			long lastseen = json_object_get_int64(j_lastseen);
			CHKiRet(addSender(pData, sender, messages, firstseen, lastseen));

		}
	}

finalize_it:
	RETiRet;
}

static rsRetVal
initHashtable(instanceData *const pData)
{
	DEFiRet;

	if((pData->stats_senders = create_hashtable(100, hash_from_string, key_equals_string, NULL)) == NULL) {
		LogError(0, RS_RET_INTERNAL_ERROR, "error trying to initialize hash-table "
			"for sender table. Sender statistics and warnings are disabled.");
		ABORT_FINALIZE(RS_RET_INTERNAL_ERROR); // TODO: check status!
	}

	/* read existing data */
	json_object *jsonTree;
	CHKiRet(readSenderStats(pData, &jsonTree));
	CHKiRet(jsonToHashtable(pData, jsonTree));

	/* start background writer */
	pthread_rwlock_init(&pData->mutSenders, NULL);
	if(pthread_create(&pData->bgw_tid, NULL, bgWriter, pData) != 0) {
		LogError(0, RS_RET_ERR, "omsendertrack: cannot create background writing thread. "
				"No interim files will be written!");
		ABORT_FINALIZE(RS_RET_ERR);
	}

finalize_it:
	RETiRet;
}


#if 0
static void ATTR_NONNULL()
doFunc_parse_json(struct cnffunc *__restrict__ const func,
	struct svar *__restrict__ const ret,
	void *const usrptr,
	wti_t *const pWti)
{
	int bMustFree;
	int bMustFree2;
	smsg_t *const pMsg = (smsg_t*)usrptr;
	struct json_object *json;

	int retVal;
	assert(jsontext != NULL);
	assert(container != NULL);
	assert(pMsg != NULL);

	struct json_tokener *const tokener = json_tokener_new();
	if(tokener == NULL) {
		retVal = 1;
		goto finalize_it;
	}
	json = json_tokener_parse_ex(tokener, jsontext, strlen(jsontext));
	if(json == NULL) {
		retVal = RS_SCRIPT_EINVAL;
	} else {
		size_t off = (*container == '$') ? 1 : 0;
		msgAddJSON(pMsg, (uchar*)container+off, json, 0, 0);
		retVal = RS_SCRIPT_EOK;
	}
	wtiSetScriptErrno(pWti, retVal);
	json_tokener_free(tokener);


finalize_it:

	if(bMustFree) {
		free(jsontext);
	}
	if(bMustFree2) {
		free(container);
	}
}
#endif


/* this function writes the actual sender stats. */
static rsRetVal
writeSenderStats(instanceData *const pData, FILE *const fp)
{
	struct hashtable_itr *itr = NULL;
	sender_stats_t *stat;
	int bNeedEOL = 0;
	DEFiRet;

	dbgprintf("writeSenderStats() called, hashtable_count %d\n", hashtable_count(pData->stats_senders));
	fprintf(fp, "[\n"); /* begin JSON array */

	pthread_rwlock_rdlock(&pData->mutSenders);

	/* Iterator constructor only returns a valid iterator if
	 * the hashtable is not empty
	 */
	if(hashtable_count(pData->stats_senders) > 0) {
		itr = hashtable_iterator(pData->stats_senders);
		do {
			stat = (sender_stats_t*)hashtable_iterator_value(itr);
			fprintf(fp, "%s{"
				"\"sender\":\"%s\""
				",\"messages\":%" PRIu64
				",\"firstseen\":%" PRIdMAX
				",\"lastseen\":%" PRIdMAX
				"}",
				(bNeedEOL ? ",\n" : ""),
				stat->sender, stat->nMsgs,
				(intmax_t) stat->firstSeen, (intmax_t) stat->lastSeen);
			bNeedEOL = 1;
		} while (hashtable_iterator_advance(itr));
	}

	free(itr);
	pthread_rwlock_unlock(&pData->mutSenders);

	fprintf(fp, "%s]\n", bNeedEOL ? "\n" : ""); /* end JSON array */
	RETiRet;
}

static rsRetVal
writeSenderInfo(instanceData *const pData)
{

	DEFiRet;
	FILE *fp = NULL;
	char tmpname[1024]; // TODO: size!

	dbgprintf("writeSenderInfo, file %s\n", pData->statefile);
	snprintf(tmpname, sizeof(tmpname), "%s.tmp", pData->statefile);

	if((fp = fopen(tmpname, "w")) == NULL) {
		LogError(errno, RS_RET_IO_ERROR,
			"omsendertrack could not create new temp sender file");
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

	CHKiRet(writeSenderStats(pData, fp));

	/* Atomically replace the old file with the new one */
	if(rename(tmpname, (const char*) pData->statefile) != 0) {
		LogError(errno, RS_RET_IO_ERROR,
			"omsendertrack rename '%s' to configured file name '%s' failed - "
			"left temp file intact", tmpname, pData->statefile);
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	}

finalize_it:
	if(fp != NULL) {
		fclose(fp);
	}
	RETiRet;
}


/* background writing thread for sender stats */
static void *
bgWriter(void *arg)
{
	instanceData *pData = (instanceData *) arg;

	/* block all signals except SIGTTIN and SIGSEGV */
	sigset_t sigSet;
	sigfillset(&sigSet);
	sigdelset(&sigSet, SIGTTIN);
	sigdelset(&sigSet, SIGSEGV);
	pthread_sigmask(SIG_BLOCK, &sigSet, NULL);

	uchar thrdName[32];
	snprintf((char*)thrdName, sizeof(thrdName), "omsendertrack/bgw"); // TODO: instance-identifier?
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
		srSleep(60, 0);
		if(pData->bShutdownBackgroundWriter == 1) {
			break;
		}
		dbgprintf("bgwriter writing report file\n");
		writeSenderInfo(pData);
	}

	dbgprintf("bgWriter finished\n");
	return NULL;
}


static rsRetVal
recordSender(instanceData *const pData, const uchar *const sender, const time_t lastSeen)
{
	sender_stats_t* stat;
	DEFiRet;
	int needUpdate = 1;

	assert(pData->stats_senders != NULL);

	pthread_rwlock_rdlock(&pData->mutSenders);
	stat = hashtable_search(pData->stats_senders, (void*)sender);
	if(stat == NULL) {
		/* we now need to write to the hash table */
		pthread_rwlock_unlock(&pData->mutSenders);
		pthread_rwlock_wrlock(&pData->mutSenders);

		// Re-check in case another writer added it
		stat = hashtable_search(pData->stats_senders, (void*)sender);
		if(stat == NULL) {
			DBGPRINTF("recordSender: sender '%s' not found, adding\n", sender);
			CHKiRet(addSender(pData, (const char *) sender, 1, lastSeen, lastSeen));
			needUpdate = 0;
		}
	}

	if(needUpdate) {
		/* this mutex is for the atomic update of a single sender record. We do NOT
		 * expect much contention on it.
		 */
		pthread_mutex_lock(&stat->mut);
		stat->nMsgs++;
		stat->lastSeen = lastSeen;
		pthread_mutex_unlock(&stat->mut);
	}

	DBGPRINTF("omsendertrack: recordSender: '%s', lastSeen %llu\n",
		sender, (long long unsigned) lastSeen);

finalize_it:
	pthread_rwlock_unlock(&pData->mutSenders);
	RETiRet;
}

BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars
ENDinitConfVars

BEGINcreateInstance
CODESTARTcreateInstance
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
	pthread_kill(pData->bgw_tid, SIGTTIN);

	/* wait until stopped */
	dbgprintf("waiting for bgWriter to finish\n");
	pthread_join(pData->bgw_tid, NULL);

	/* do final write */
	writeSenderInfo(pData);

	/* destroy data structs */
	pthread_rwlock_destroy(&pData->mutSenders);
	hashtable_destroy(pData->stats_senders, 1); /* 1 => free all values automatically */
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
		} else if(!strcmp(actpblk.descr[i].name, "statefile")) {
			pData->statefile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
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

	initHashtable(pData);
CODE_STD_FINALIZERnewActInst
	if(bDestructPValsOnExit)
		cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst



BEGINparseSelectorAct
	int iTplOpts;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
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
