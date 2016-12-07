/* ommongodb.c
 * Output module for mongodb.
 * Note: this module uses the libmongo-client library. The original 10gen
 * mongodb C interface is crap. Obtain the library here:
 * https://github.com/algernon/libmongo-client
 *
 * Copyright 2007-2016 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-attributes"
#include <mongo.h>
#pragma GCC diagnostic pop
#include <json.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "datetime.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "unicode-helper.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("ommongodb")
/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(datetime)

typedef struct _instanceData {
	mongo_sync_connection *conn;
	struct json_tokener *json_tokener; /* only if (tplName != NULL) */
	uchar *server;
	int port;
        uchar *db;
	uchar *collection;
	uchar *uid;
	uchar *pwd;
	uchar *dbNcoll;
	uchar *tplName;
	int bErrMsgPermitted;	/* only one errmsg permitted per connection */
} instanceData;

typedef struct wrkrInstanceData {
	instanceData *pData;
} wrkrInstanceData_t;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 0 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "db", eCmdHdlrGetWord, 0 },
	{ "collection", eCmdHdlrGetWord, 0 },
	{ "uid", eCmdHdlrGetWord, 0 },
	{ "pwd", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};

static pthread_mutex_t mutDoAct = PTHREAD_MUTEX_INITIALIZER;

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* use this to specify if select features are supported by this
	 * plugin. If not, the framework will handle that. Currently, only
	 * RepeatedMsgReduction ("last message repeated n times") is optional.
	 */
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

static void closeMongoDB(instanceData *pData)
{
	if(pData->conn != NULL) {
                mongo_sync_disconnect(pData->conn);
		pData->conn = NULL;
	}
}


BEGINfreeInstance
CODESTARTfreeInstance
	closeMongoDB(pData);
	if (pData->json_tokener != NULL)
		json_tokener_free(pData->json_tokener);
	free(pData->server);
	free(pData->db);
	free(pData->collection);
	free(pData->uid);
	free(pData->pwd);
	free(pData->dbNcoll);
	free(pData->tplName);
ENDfreeInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
	(void)pData;
ENDdbgPrintInstInfo


/* report error that occured during *last* operation
 */
static void
reportMongoError(instanceData *pData)
{
	char errStr[1024];
	gchar *err;
	int eno;

	if(pData->bErrMsgPermitted) {
		eno = errno;
		if(mongo_sync_cmd_get_last_error(pData->conn, (gchar*)pData->db, &err) == TRUE) {
			errmsg.LogError(0, RS_RET_ERR, "ommongodb: error: %s", err);
		} else {
			DBGPRINTF("ommongodb: we had an error, but can not obtain specifics, "
				  "using plain old errno error message generator\n");
			errmsg.LogError(0, RS_RET_ERR, "ommongodb: error: %s",
				rs_strerror_r(eno, errStr, sizeof(errStr)));
		}
		pData->bErrMsgPermitted = 0;
	}
}


/* The following function is responsible for initializing a
 * MongoDB connection.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal initMongoDB(instanceData *pData, int bSilent)
{
	const char *server;
	DEFiRet;

	server = (pData->server == NULL) ? "127.0.0.1" : (const char*) pData->server;
	DBGPRINTF("ommongodb: trying connect to '%s' at port %d\n", server, pData->port);

	pData->conn = mongo_sync_connect(server, pData->port, TRUE);
	if(pData->conn == NULL) {
		if(!bSilent) {
			reportMongoError(pData);
			dbgprintf("ommongodb: can not initialize MongoDB handle");
		}
                ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

	/* perform authentication */
	if(pData->uid && pData->pwd) {

	  /* require both uid and pwd before attempting authentication */
	  if(!pData->uid || !pData->pwd) {
	    dbgprintf("ommongodb: authentication requires uid and pwd attributes set; skipping");
	  }
	  else if(!mongo_sync_cmd_authenticate(pData->conn, (const gchar*)pData->db,
	  	  			(const gchar*)pData->uid, (const gchar*)pData->pwd)) {
	    if(!bSilent) {
	      reportMongoError(pData);
	      dbgprintf("ommongodb: could not authenticate %s against '%s'", pData->uid, pData->db);
	    }

	    /* no point in continuing with an unauthenticated connection */
	    closeMongoDB(pData);
	    ABORT_FINALIZE(RS_RET_SUSPENDED);
	  }
	  else {
	    dbgprintf("ommongodb: authenticated with %s against '%s'", pData->uid, pData->db);
	  }
	}

finalize_it:
	RETiRet;
}


/* map syslog severity to lumberjack level
 * TODO: consider moving this to msg.c - make some dirty "friend" references...
 * rgerhards, 2012-03-19
 */
static const char *
getLumberjackLevel(short severity)
{
	switch(severity) {
		case 0: return "FATAL";
		case 1:
		case 2:
		case 3: return "ERROR";
		case 4: return "WARN";
		case 5:
		case 6: return "INFO";
		case 7: return "DEBUG";
		default:DBGPRINTF("ommongodb: invalid syslog severity %u\n", severity);
			return "INVLD";
	}
}


/* small helper: get integer power of 10 */
static int
i10pow(int exp)
{
	int r = 1;
	while(exp > 0) {
		r *= 10;
		exp--;
	}
	return r;
}
/* Return a BSON document when an user hasn't specified a template.
 * In this mode, we use the standard document format, which is somewhat
 * aligned to cee (as described in project lumberjack). Note that this is
 * a moving target, so we may run out of sync (and stay so to retain
 * backward compatibility, which we consider pretty important).
 */
static bson *
getDefaultBSON(smsg_t *pMsg)
{
	bson *doc = NULL;
	uchar *procid; short unsigned procid_free; rs_size_t procid_len;
	uchar *tag; short unsigned tag_free; rs_size_t tag_len;
	uchar *pid; short unsigned pid_free; rs_size_t pid_len;
	uchar *sys; short unsigned sys_free; rs_size_t sys_len;
	uchar *msg; short unsigned msg_free; rs_size_t msg_len;
	int severity, facil;
	gint64 ts_gen, ts_rcv; /* timestamps: generated, received */
	int secfrac;
	msgPropDescr_t cProp; /* we use internal implementation knowledge... */

	cProp.id = PROP_PROGRAMNAME;
	procid = MsgGetProp(pMsg, NULL, &cProp, &procid_len, &procid_free, NULL);
	cProp.id = PROP_SYSLOGTAG;
	tag = MsgGetProp(pMsg, NULL, &cProp, &tag_len, &tag_free, NULL);
	cProp.id = PROP_PROCID;
	pid = MsgGetProp(pMsg, NULL, &cProp, &pid_len, &pid_free, NULL);
	cProp.id = PROP_HOSTNAME;
	sys = MsgGetProp(pMsg, NULL, &cProp, &sys_len, &sys_free, NULL);
	cProp.id = PROP_MSG;
	msg = MsgGetProp(pMsg, NULL, &cProp, &msg_len, &msg_free, NULL);

	/* TODO: move to datetime? Refactor in any case! rgerhards, 2012-03-30 */
	ts_gen = (gint64) datetime.syslogTime2time_t(&pMsg->tTIMESTAMP) * 1000; /* ms! */
	DBGPRINTF("ommongodb: ts_gen is %lld\n", (long long) ts_gen);
	DBGPRINTF("ommongodb: secfrac is %d, precision %d\n",  pMsg->tTIMESTAMP.secfrac, pMsg->tTIMESTAMP.secfracPrecision);
	if(pMsg->tTIMESTAMP.secfracPrecision > 3) {
		secfrac = pMsg->tTIMESTAMP.secfrac / i10pow(pMsg->tTIMESTAMP.secfracPrecision - 3);
	} else if(pMsg->tTIMESTAMP.secfracPrecision < 3) {
		secfrac = pMsg->tTIMESTAMP.secfrac * i10pow(3 - pMsg->tTIMESTAMP.secfracPrecision);
	} else {
		secfrac = pMsg->tTIMESTAMP.secfrac;
	}
	ts_gen += secfrac;
	ts_rcv = (gint64) datetime.syslogTime2time_t(&pMsg->tRcvdAt) * 1000; /* ms! */
	if(pMsg->tRcvdAt.secfracPrecision > 3) {
		secfrac = pMsg->tRcvdAt.secfrac / i10pow(pMsg->tRcvdAt.secfracPrecision - 3);
	} else if(pMsg->tRcvdAt.secfracPrecision < 3) {
		secfrac = pMsg->tRcvdAt.secfrac * i10pow(3 - pMsg->tRcvdAt.secfracPrecision);
	} else {
		secfrac = pMsg->tRcvdAt.secfrac;
	}
	ts_rcv += secfrac;

	/* the following need to be int, but are short, so we need to xlat */
	severity = pMsg->iSeverity;
	facil = pMsg->iFacility;

	doc = bson_build(BSON_TYPE_STRING, "sys", sys, sys_len,
			 BSON_TYPE_UTC_DATETIME, "time", ts_gen,
			 BSON_TYPE_UTC_DATETIME, "time_rcvd", ts_rcv,
			 BSON_TYPE_STRING, "msg", msg, msg_len,
			 BSON_TYPE_INT32, "syslog_fac", facil,
			 BSON_TYPE_INT32, "syslog_sever", severity,
			 BSON_TYPE_STRING, "syslog_tag", tag, tag_len,
			 BSON_TYPE_STRING, "procid", procid, procid_len,
			 BSON_TYPE_STRING, "pid", pid, pid_len,
			 BSON_TYPE_STRING, "level", getLumberjackLevel(pMsg->iSeverity), -1,
			 BSON_TYPE_NONE);

	if(procid_free) free(procid);
	if(tag_free) free(tag);
	if(pid_free) free(pid);
	if(sys_free) free(sys);
	if(msg_free) free(msg);

	if(doc == NULL)
		return doc;
	bson_finish(doc);
	return doc;
}

static bson *BSONFromJSONArray(struct json_object *json);
static bson *BSONFromJSONObject(struct json_object *json);
static gboolean BSONAppendExtendedJSON(bson *doc, const gchar *name, struct json_object *json);

/* Append a BSON variant of json to doc using name.  Return TRUE on success */
static gboolean
BSONAppendJSONObject(bson *doc, const gchar *name, struct json_object *json)
{
	switch(json != NULL ? json_object_get_type(json) : json_type_null) {
	case json_type_null:
		return bson_append_null(doc, name);
	case json_type_boolean:
		return bson_append_boolean(doc, name,
					   json_object_get_boolean(json));
	case json_type_double:
		return bson_append_double(doc, name,
					  json_object_get_double(json));
	case json_type_int: {
		int64_t i;

		i = json_object_get_int64(json);
		if (i >= INT32_MIN && i <= INT32_MAX)
			return bson_append_int32(doc, name, i);
		else
			return bson_append_int64(doc, name, i);
	}
	case json_type_object: {

		if (BSONAppendExtendedJSON(doc, name, json) == TRUE)
		    return TRUE;

		bson *sub;
		gboolean ok;

		sub = BSONFromJSONObject(json);
		if (sub == NULL)
			return FALSE;
		ok = bson_append_document(doc, name, sub);
		bson_free(sub);
		return ok;
	}
	case json_type_array: {
		bson *sub;
		gboolean ok;

		sub = BSONFromJSONArray(json);
		if (sub == NULL)
			return FALSE;
		ok = bson_append_document(doc, name, sub);
		bson_free(sub);
		return ok;
	}
	case json_type_string:
		return bson_append_string(doc, name,
					  json_object_get_string(json), -1);

	default:
		return FALSE;
	}
}

/* Note: this function assumes that at max a single sub-object exists. This
 * may need to be extended to cover cases where multiple objects are contained.
 * However, I am not sure about the original intent of this contribution and
 * just came across it when refactoring the json calls. As everything seems
 * to work since quite a while, I do not make any changes now.
 * rgerhards, 2016-04-09
 */
static gboolean
BSONAppendExtendedJSON(bson *doc, const gchar *name, struct json_object *json)
{
	struct json_object_iterator itEnd = json_object_iter_end(json);
	struct json_object_iterator it = json_object_iter_begin(json);

	if (!json_object_iter_equal(&it, &itEnd)) {
		const char *const key = json_object_iter_peek_name(&it);
		if (strcmp(key, "$date") == 0) {
			struct tm tm;
			gint64 ts;
			struct json_object *val;

			val = json_object_iter_peek_value(&it);
			DBGPRINTF("ommongodb: extended json date detected %s", json_object_get_string(val));
			tm.tm_isdst = -1;
			strptime(json_object_get_string(val), "%Y-%m-%dT%H:%M:%S%z", &tm);
			ts = 1000 * (gint64) mktime(&tm);
			return bson_append_utc_datetime(doc, name, ts);
		}
	}
	return FALSE;
}

/* Return a BSON variant of json, which must be a json_type_array */
static bson *
BSONFromJSONArray(struct json_object *json)
{
	/* Way more than necessary */
	bson *doc = NULL;
	size_t i, array_len;

	doc = bson_new();
	if(doc == NULL)
		goto error;

	array_len = json_object_array_length(json);
	for (i = 0; i < array_len; i++) {
		char buf[sizeof(size_t) * CHAR_BIT + 1];

		if ((size_t)snprintf(buf, sizeof(buf), "%zu", i) >= sizeof(buf))
			goto error;
		if (BSONAppendJSONObject(doc, buf,
					 json_object_array_get_idx(json, i))
		    == FALSE)
			goto error;
	}

	if(bson_finish(doc) == FALSE)
		goto error;

	return doc;

error:
	if(doc != NULL)
		bson_free(doc);
	return NULL;
}

/* Return a BSON variant of json, which must be a json_type_object */
static bson *
BSONFromJSONObject(struct json_object *json)
{
	bson *doc = NULL;

	doc = bson_new();
	if(doc == NULL)
		goto error;

	struct json_object_iterator it = json_object_iter_begin(json);
	struct json_object_iterator itEnd = json_object_iter_end(json);
	while (!json_object_iter_equal(&it, &itEnd)) {
		if (BSONAppendJSONObject(doc, json_object_iter_peek_name(&it),
			json_object_iter_peek_value(&it)) == FALSE)
			goto error;
		json_object_iter_next(&it);
	}

	if(bson_finish(doc) == FALSE)
		goto error;

	return doc;

error:
	if(doc != NULL)
		bson_free(doc);
	return NULL;
}

BEGINtryResume
CODESTARTtryResume
	if(pWrkrData->pData->conn == NULL) {
		iRet = initMongoDB(pWrkrData->pData, 1);
	}
ENDtryResume

BEGINdoAction_NoStrings
	bson *doc = NULL;
	instanceData *pData;
CODESTARTdoAction
	pthread_mutex_lock(&mutDoAct);
	pData = pWrkrData->pData;
	/* see if we are ready to proceed */
	if(pData->conn == NULL) {
		CHKiRet(initMongoDB(pData, 0));
	}

	if(pData->tplName == NULL) {
		doc = getDefaultBSON(*(smsg_t**)pMsgData);
	} else {
		doc = BSONFromJSONObject(*(struct json_object **)pMsgData);
	}
	if(doc == NULL) {
		dbgprintf("ommongodb: error creating BSON doc\n");
		/* FIXME: is this a correct return code? */
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if(mongo_sync_cmd_insert(pData->conn, (char*)pData->dbNcoll, doc, NULL)) {
		pData->bErrMsgPermitted = 1;
	} else {
		dbgprintf("ommongodb: insert error\n");
		reportMongoError(pData);
		/* close on insert error to permit resume */
		closeMongoDB(pData);
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}

finalize_it:
	pthread_mutex_unlock(&mutDoAct);
	if(doc != NULL)
		bson_free(doc);
ENDdoAction


static void
setInstParamDefaults(instanceData *pData)
{
	pData->server = NULL;
	pData->port = 27017;
	pData->db = NULL;
	pData->collection= NULL;
	pData->uid = NULL;
	pData->pwd = NULL;
	pData->tplName = NULL;
}

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	unsigned lendb, lencoll;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTnewActInst(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "server")) {
			pData->server = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->port = (int) pvals[i].val.d.n;
		} else if(!strcmp(actpblk.descr[i].name, "db")) {
			pData->db = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "collection")) {
			pData->collection = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "uid")) {
			pData->uid = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "pwd")) {
			pData->pwd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("ommongodb: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));
	} else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, ustrdup(pData->tplName),
				     OMSR_TPL_AS_JSON));
		CHKmalloc(pData->json_tokener = json_tokener_new());
	}

	if(pData->db == NULL)
		CHKmalloc(pData->db = (uchar*)strdup("syslog"));
	if(pData->collection == NULL)
		 CHKmalloc(pData->collection = (uchar*)strdup("log"));

	/* we now create a db+collection string as we need to pass this
	 * into the API and we do not want to generate it each time ;)
	 * +2 ==> dot as delimiter and \0
	 */
	lendb = strlen((char*)pData->db);
	lencoll = strlen((char*)pData->collection);
	CHKmalloc(pData->dbNcoll = malloc(lendb+lencoll+2));
	memcpy(pData->dbNcoll, pData->db, lendb);
	pData->dbNcoll[lendb] = '.';
	/* lencoll+1 => copy \0! */
	memcpy(pData->dbNcoll+lendb+1, pData->collection, lencoll+1);

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	if(!strncmp((char*) p, ":ommongodb:", sizeof(":ommongodb:") - 1)) {
		errmsg.LogError(0, RS_RET_LEGA_ACT_NOT_SUPPORTED,
			"ommongodb supports only v6 config format, use: "
			"action(type=\"ommongodb\" server=...)");
	}
	ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
	rsRetVal localRet;
	rsRetVal (*pomsrGetSupportedTplOpts)(unsigned long *pOpts);
	unsigned long opts;
	int bJSONPassingSupported;
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	DBGPRINTF("ommongodb: module compiled with rsyslog version %s.\n", VERSION);

	/* check if the rsyslog core supports parameter passing code */
	bJSONPassingSupported = 0;
	localRet = pHostQueryEtryPt((uchar*)"OMSRgetSupportedTplOpts",
				    &pomsrGetSupportedTplOpts);
	if(localRet == RS_RET_OK) {
		/* found entry point, so let's see if core supports msg passing */
		CHKiRet((*pomsrGetSupportedTplOpts)(&opts));
		if(opts & OMSR_TPL_AS_JSON)
			bJSONPassingSupported = 1;
	} else if(localRet != RS_RET_ENTRY_POINT_NOT_FOUND) {
		ABORT_FINALIZE(localRet); /* Something else went wrong, not acceptable */
	}
	if(!bJSONPassingSupported) {
		DBGPRINTF("ommongodb: JSON-passing is not supported by rsyslog core, "
			  "can not continue.\n");
		ABORT_FINALIZE(RS_RET_NO_JSON_PASSING);
	}
ENDmodInit
