/* ommysql.c
 * This is the implementation of the build-in output module for MySQL.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007-2012 Adiscon GmbH.
 *
 * This file is part of rsyslog.
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
#include <errno.h>
#include <time.h>
#include <mysql.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "ommysql.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("ommysql")

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal);

/* internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

typedef struct _instanceData {
	MYSQL	*f_hmysql;			/* handle to MySQL */
	char	f_dbsrv[MAXHOSTNAMELEN+1];	/* IP or hostname of DB server*/ 
	unsigned int f_dbsrvPort;		/* port of MySQL server */
	char	f_dbname[_DB_MAXDBLEN+1];	/* DB name */
	char	f_dbuid[_DB_MAXUNAMELEN+1];	/* DB user */
	char	f_dbpwd[_DB_MAXPWDLEN+1];	/* DB user's password */
	unsigned uLastMySQLErrno;		/* last errno returned by MySQL or 0 if all is well */
	uchar * f_configfile;			/* MySQL Client Configuration File */
	uchar * f_configsection;		/* MySQL Client Configuration Section */
	uchar	*tplName;       /* format template to use */
} instanceData;

typedef struct configSettings_s {
	int iSrvPort;				/* database server port */
	uchar *pszMySQLConfigFile;	/* MySQL Client Configuration File */
	uchar *pszMySQLConfigSection;	/* MySQL Client Configuration Section */ 
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "server", eCmdHdlrGetWord, 1 },
	{ "db", eCmdHdlrGetWord, 1 },
	{ "uid", eCmdHdlrGetWord, 1 },
	{ "pwd", eCmdHdlrGetWord, 1 },
	{ "serverport", eCmdHdlrInt, 0 },
	{ "mysqlconfig.file", eCmdHdlrGetWord, 0 },
	{ "mysqlconfig.section", eCmdHdlrGetWord, 0 },
	{ "template", eCmdHdlrGetWord, 0 }
};
static struct cnfparamblk actpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(actpdescr)/sizeof(struct cnfparamdescr),
	  actpdescr
	};


BEGINinitConfVars		/* (re)set config variables to default values */
CODESTARTinitConfVars 
	resetConfigVariables(NULL, NULL);
ENDinitConfVars


BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


/* The following function is responsible for closing a
 * MySQL connection.
 * Initially added 2004-10-28
 */
static void closeMySQL(instanceData *pData)
{
	ASSERT(pData != NULL);

	if(pData->f_hmysql != NULL) {	/* just to be on the safe side... */
		mysql_close(pData->f_hmysql);	
		pData->f_hmysql = NULL;
	}
	if(pData->f_configfile!=NULL){
		free(pData->f_configfile);
		pData->f_configfile=NULL;
	}
	if(pData->f_configsection!=NULL){
		free(pData->f_configsection);
		pData->f_configsection=NULL;
	}
}

BEGINfreeInstance
CODESTARTfreeInstance
	free(pData->f_configfile);
	free(pData->f_configsection);
	free(pData->tplName);
	closeMySQL(pData);
ENDfreeInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
ENDdbgPrintInstInfo


/* log a database error with descriptive message.
 * We check if we have a valid MySQL handle. If not, we simply
 * report an error, but can not be specific. RGerhards, 2007-01-30
 */
static void reportDBError(instanceData *pData, int bSilent)
{
	char errMsg[512];
	unsigned uMySQLErrno;

	ASSERT(pData != NULL);

	/* output log message */
	errno = 0;
	if(pData->f_hmysql == NULL) {
		errmsg.LogError(0, NO_ERRCODE, "unknown DB error occured - could not obtain MySQL handle");
	} else { /* we can ask mysql for the error description... */
		uMySQLErrno = mysql_errno(pData->f_hmysql);
		snprintf(errMsg, sizeof(errMsg)/sizeof(char), "db error (%d): %s\n", uMySQLErrno,
			mysql_error(pData->f_hmysql));
		if(bSilent || uMySQLErrno == pData->uLastMySQLErrno)
			dbgprintf("mysql, DBError(silent): %s\n", errMsg);
		else {
			pData->uLastMySQLErrno = uMySQLErrno;
			errmsg.LogError(0, NO_ERRCODE, "%s", errMsg);
		}
	}
		
	return;
}


/* The following function is responsible for initializing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal initMySQL(instanceData *pData, int bSilent)
{
	DEFiRet;

	ASSERT(pData != NULL);
	ASSERT(pData->f_hmysql == NULL);
	pData->f_hmysql = mysql_init(NULL);
	if(pData->f_hmysql == NULL) {
		errmsg.LogError(0, RS_RET_SUSPENDED, "can not initialize MySQL handle");
		iRet = RS_RET_SUSPENDED;
	} else { /* we could get the handle, now on with work... */
		mysql_options(pData->f_hmysql,MYSQL_READ_DEFAULT_GROUP,((pData->f_configsection!=NULL)?(char*)pData->f_configsection:"client"));
		if(pData->f_configfile!=NULL){
			FILE * fp;
			fp=fopen((char*)pData->f_configfile,"r");
			int err=errno;
			if(fp==NULL){
				char msg[512];
				snprintf(msg,sizeof(msg)/sizeof(char),"Could not open '%s' for reading",pData->f_configfile);
				if(bSilent) {
					char errStr[512];
					rs_strerror_r(err, errStr, sizeof(errStr));
					dbgprintf("mysql configuration error(%d): %s - %s\n",err,msg,errStr);
				} else
					errmsg.LogError(err,NO_ERRCODE,"mysql configuration error: %s\n",msg);
			} else {
				fclose(fp);
				mysql_options(pData->f_hmysql,MYSQL_READ_DEFAULT_FILE,pData->f_configfile);
			}
		}
		/* Connect to database */
		if(mysql_real_connect(pData->f_hmysql, pData->f_dbsrv, pData->f_dbuid,
				      pData->f_dbpwd, pData->f_dbname, pData->f_dbsrvPort, NULL, 0) == NULL) {
			reportDBError(pData, bSilent);
			closeMySQL(pData); /* ignore any error we may get */
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
		mysql_autocommit(pData->f_hmysql, 0);
	}

finalize_it:
	RETiRet;
}


/* The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28 mmeckelein
 */
rsRetVal writeMySQL(uchar *psz, instanceData *pData)
{
	DEFiRet;

	ASSERT(psz != NULL);
	ASSERT(pData != NULL);

	/* see if we are ready to proceed */
	if(pData->f_hmysql == NULL) {
		CHKiRet(initMySQL(pData, 0));
		
	}

	/* try insert */
	if(mysql_query(pData->f_hmysql, (char*)psz)) {
		/* error occured, try to re-init connection and retry */
		closeMySQL(pData); /* close the current handle */
		CHKiRet(initMySQL(pData, 0)); /* try to re-open */
		if(mysql_query(pData->f_hmysql, (char*)psz)) { /* re-try insert */
			/* we failed, giving up for now */
			reportDBError(pData, 0);
			closeMySQL(pData); /* free ressources */
			ABORT_FINALIZE(RS_RET_SUSPENDED);
		}
	}

finalize_it:
	if(iRet == RS_RET_OK) {
		pData->uLastMySQLErrno = 0; /* reset error for error supression */
	}

	RETiRet;
}


BEGINtryResume
CODESTARTtryResume
	if(pData->f_hmysql == NULL) {
		iRet = initMySQL(pData, 1);
	}
ENDtryResume

BEGINbeginTransaction
CODESTARTbeginTransaction
	CHKiRet(writeMySQL((uchar*)"START TRANSACTION", pData));
finalize_it:
ENDbeginTransaction

BEGINdoAction
CODESTARTdoAction
	dbgprintf("\n");
	CHKiRet(writeMySQL(ppString[0], pData));
	iRet = RS_RET_DEFER_COMMIT;
finalize_it:
ENDdoAction

BEGINendTransaction
CODESTARTendTransaction
	if (mysql_commit(pData->f_hmysql) != 0)	{	
		dbgprintf("mysql server error: transaction not committed\n");		
		iRet = RS_RET_SUSPENDED;
	}
ENDendTransaction


static inline void
setInstParamDefaults(instanceData *pData)
{
	pData->f_dbsrvPort = 0;
	pData->f_configfile = NULL;
	pData->f_configsection = NULL;
	pData->tplName = NULL;
	pData->f_hmysql = NULL; /* initialize, but connect only on first message (important for queued mode!) */
}


/* note: we use the fixed-size buffers inside the config object to avoid
 * changing too much of the previous plumbing. rgerhards, 2012-02-02
 */
BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	char *cstr;
CODESTARTnewActInst
	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));
	setInstParamDefaults(pData);

	CODE_STD_STRING_REQUESTparseSelectorAct(1)
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(actpblk.descr[i].name, "server")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			strncpy(pData->f_dbsrv, cstr, sizeof(pData->f_dbsrv));
			free(cstr);
		} else if(!strcmp(actpblk.descr[i].name, "serverport")) {
			pData->f_dbsrvPort = (int) pvals[i].val.d.n, NULL;
		} else if(!strcmp(actpblk.descr[i].name, "db")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			strncpy(pData->f_dbname, cstr, sizeof(pData->f_dbname));
			free(cstr);
		} else if(!strcmp(actpblk.descr[i].name, "uid")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			strncpy(pData->f_dbuid, cstr, sizeof(pData->f_dbuid));
			free(cstr);
		} else if(!strcmp(actpblk.descr[i].name, "pwd")) {
			cstr = es_str2cstr(pvals[i].val.d.estr, NULL);
			strncpy(pData->f_dbpwd, cstr, sizeof(pData->f_dbpwd));
			free(cstr);
		} else if(!strcmp(actpblk.descr[i].name, "mysqlconfig.file")) {
			pData->f_configfile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "mysqlconfig.section")) {
			pData->f_configsection = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(actpblk.descr[i].name, "template")) {
			pData->tplName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("ommysql: program error, non-handled "
			  "param '%s'\n", actpblk.descr[i].name);
		}
	}

	if(pData->tplName == NULL) {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar*) strdup(" StdDBFmt"),
			OMSR_RQD_TPL_OPT_SQL));
	} else {
		CHKiRet(OMSRsetEntry(*ppOMSR, 0,
			(uchar*) strdup((char*) pData->tplName),
			OMSR_RQD_TPL_OPT_SQL));
	}
CODE_STD_FINALIZERnewActInst
dbgprintf("XXXX: added param, iRet %d\n", iRet);
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
	int iMySQLPropErr = 0;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us
	 * The first test [*p == '>'] can be skipped if a module shall only
	 * support the newer slection syntax [:modname:]. This is in fact
	 * recommended for new modules. Please note that over time this part
	 * will be handled by rsyslogd itself, but for the time being it is
	 * a good compromise to do it at the module level.
	 * rgerhards, 2007-10-15
	 */
	if(*p == '>') {
		p++; /* eat '>' '*/
	} else if(!strncmp((char*) p, ":ommysql:", sizeof(":ommysql:") - 1)) {
		p += sizeof(":ommysql:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	CHKiRet(createInstance(&pData));

	/* rger 2004-10-28: added support for MySQL
	 * >server,dbname,userid,password
	 * Now we read the MySQL connection properties 
	 * and verify that the properties are valid.
	 */
	if(getSubString(&p, pData->f_dbsrv, MAXHOSTNAMELEN+1, ','))
		iMySQLPropErr++;
	if(*pData->f_dbsrv == '\0')
		iMySQLPropErr++;
	if(getSubString(&p, pData->f_dbname, _DB_MAXDBLEN+1, ','))
		iMySQLPropErr++;
	if(*pData->f_dbname == '\0')
		iMySQLPropErr++;
	if(getSubString(&p, pData->f_dbuid, _DB_MAXUNAMELEN+1, ','))
		iMySQLPropErr++;
	if(*pData->f_dbuid == '\0')
		iMySQLPropErr++;
	if(getSubString(&p, pData->f_dbpwd, _DB_MAXPWDLEN+1, ';'))
		iMySQLPropErr++;
	/* now check for template
	 * We specify that the SQL option must be present in the template.
	 * This is for your own protection (prevent sql injection).
	 */
	if(*(p-1) == ';')
		--p;	/* TODO: the whole parsing of the MySQL module needs to be re-thought - but this here
			 *       is clean enough for the time being -- rgerhards, 2007-07-30
			 */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_RQD_TPL_OPT_SQL, (uchar*) " StdDBFmt"));
	
	/* If we detect invalid properties, we disable logging, 
	 * because right properties are vital at this place.  
	 * Retries make no sense. 
	 */
	if (iMySQLPropErr) { 
		errmsg.LogError(0, RS_RET_INVALID_PARAMS, "Trouble with MySQL connection properties. -MySQL logging disabled");
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	} else {
		pData->f_dbsrvPort = (unsigned) cs.iSrvPort;	/* set configured port */
		pData->f_configfile = cs.pszMySQLConfigFile;
		pData->f_configsection = cs.pszMySQLConfigSection;
		pData->f_hmysql = NULL; /* initialize, but connect only on first message (important for queued mode!) */
	}

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
CODESTARTmodExit
#	ifdef HAVE_MYSQL_LIBRARY_INIT
	mysql_library_end();
#	else
	mysql_server_end();
#	endif
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_TXIF_OMOD_QUERIES /* we support the transactional interface! */
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	DEFiRet;
	cs.iSrvPort = 0; /* zero is the default port */
	free(cs.pszMySQLConfigFile);
	cs.pszMySQLConfigFile = NULL;
	free(cs.pszMySQLConfigSection);
	cs.pszMySQLConfigSection = NULL;
	RETiRet;
}

BEGINmodInit()
CODESTARTmodInit
INITLegCnfVars
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	INITChkCoreFeature(bCoreSupportsBatching, CORE_FEATURE_BATCHING);
	if(!bCoreSupportsBatching) {	
		errmsg.LogError(0, NO_ERRCODE, "ommysql: rsyslog core too old");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* we need to init the MySQL library. If that fails, we cannot run */
	if(
#	ifdef HAVE_MYSQL_LIBRARY_INIT
	   mysql_library_init(0, NULL, NULL)
#	else
	   mysql_server_init(0, NULL, NULL)
#	endif
	                                   ) {
		errmsg.LogError(0, NO_ERRCODE, "ommysql: mysql_server_init() failed, plugin "
		                "can not run");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* register our config handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionommysqlserverport", 0, eCmdHdlrInt, NULL, &cs.iSrvPort, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"ommysqlconfigfile",0,eCmdHdlrGetWord,NULL,&cs.pszMySQLConfigFile,STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"ommysqlconfigsection",0,eCmdHdlrGetWord,NULL,&cs.pszMySQLConfigSection,STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit

/* vi:set ai:
 */
