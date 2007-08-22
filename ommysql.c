/* ommysql.c
 * This is the implementation of the build-in output module for MySQL.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
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
#include "config.h"
#ifdef	WITH_DB
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "ommysql.h"
#include "mysql/mysql.h" 
#include "mysql/errmsg.h"
#include "module-template.h"

/* internal structures
 */
DEF_OMOD_STATIC_DATA

typedef struct _instanceData {
	MYSQL	*f_hmysql;		/* handle to MySQL */
	char	f_dbsrv[MAXHOSTNAMELEN+1];	/* IP or hostname of DB server*/ 
	char	f_dbname[_DB_MAXDBLEN+1];	/* DB name */
	char	f_dbuid[_DB_MAXUNAMELEN+1];	/* DB user */
	char	f_dbpwd[_DB_MAXPWDLEN+1];	/* DB user's password */
	unsigned uLastMySQLErrno;	/* last errno returned by MySQL or 0 if all is well */
} instanceData;


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
	assert(pData != NULL);

	if(pData->f_hmysql != NULL) {	/* just to be on the safe side... */
		mysql_server_end();
		mysql_close(pData->f_hmysql);	
		pData->f_hmysql = NULL;
	}
}

BEGINfreeInstance
CODESTARTfreeInstance
	closeMySQL(pData);
ENDfreeInstance


BEGINneedUDPSocket
CODESTARTneedUDPSocket
ENDneedUDPSocket


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* nothing special here */
ENDdbgPrintInstInfo


BEGINonSelectReadyWrite
CODESTARTonSelectReadyWrite
ENDonSelectReadyWrite


BEGINgetWriteFDForSelect
CODESTARTgetWriteFDForSelect
ENDgetWriteFDForSelect


/* log a database error with descriptive message.
 * We check if we have a valid MySQL handle. If not, we simply
 * report an error, but can not be specific. RGerhards, 2007-01-30
 */
static void reportDBError(instanceData *pData, int bSilent)
{
	char errMsg[512];
	unsigned uMySQLErrno;

	assert(pData != NULL);

	/* output log message */
	errno = 0;
	if(pData->f_hmysql == NULL) {
		logerror("unknown DB error occured - could not obtain MySQL handle");
	} else { /* we can ask mysql for the error description... */
		uMySQLErrno = mysql_errno(pData->f_hmysql);
		snprintf(errMsg, sizeof(errMsg)/sizeof(char), "db error (%d): %s\n", uMySQLErrno,
			mysql_error(pData->f_hmysql));
		if(bSilent || uMySQLErrno == pData->uLastMySQLErrno)
			dbgprintf("mysql, DBError(silent): %s\n", errMsg);
		else {
			pData->uLastMySQLErrno = uMySQLErrno;
			logerror(errMsg);
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

	assert(pData != NULL);
	assert(pData->f_hmysql == NULL);

	pData->f_hmysql = mysql_init(NULL);
	if(pData->f_hmysql == NULL) {
		logerror("can not initialize MySQL handle");
		iRet = RS_RET_SUSPENDED;
	} else { /* we could get the handle, now on with work... */
		/* Connect to database */
		if(mysql_real_connect(pData->f_hmysql, pData->f_dbsrv, pData->f_dbuid,
				      pData->f_dbpwd, pData->f_dbname, 0, NULL, 0) == NULL) {
			reportDBError(pData, bSilent);
			closeMySQL(pData); /* ignore any error we may get */
			iRet = RS_RET_SUSPENDED;
		}
	}

	return iRet;
}


/* The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28 mmeckelein
 */
rsRetVal writeMySQL(uchar *psz, instanceData *pData)
{
	DEFiRet;

	assert(psz != NULL);
	assert(pData != NULL);

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

	return iRet;
}


BEGINtryResume
CODESTARTtryResume
	if(pData->f_hmysql == NULL) {
		iRet = initMySQL(pData, 1);
	}
ENDtryResume

BEGINdoAction
CODESTARTdoAction
	dbgprintf("\n");
	iRet = writeMySQL(ppString[0], pData);
ENDdoAction


BEGINparseSelectorAct
	int iMySQLPropErr = 0;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* first check if this config line is actually for us */
	if(*p != '>') {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;

	p++; /* eat '>' '*/

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
		logerror("Trouble with MySQL connection properties. -MySQL logging disabled");
		ABORT_FINALIZE(RS_RET_INVALID_PARAMS);
	} else {
		CHKiRet(initMySQL(pData, 0));
	}

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit

#endif /* #ifdef WITH_DB */
/*
 * vi:set ai:
 */
