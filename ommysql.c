/* omusrmsg.c
 * This is the implementation of the build-in output module for MySQL.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2007-07-20 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "rsyslog.h"
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
typedef struct _instanceData {
	MYSQL	*f_hmysql;		/* handle to MySQL */
	char	f_dbsrv[MAXHOSTNAMELEN+1];	/* IP or hostname of DB server*/ 
	char	f_dbname[_DB_MAXDBLEN+1];	/* DB name */
	char	f_dbuid[_DB_MAXUNAMELEN+1];	/* DB user */
	char	f_dbpwd[_DB_MAXPWDLEN+1];	/* DB user's password */
	time_t	f_timeResumeOnError;		/* 0 if no error is present,	
						   otherwise is it set to the
						   time a retrail should be attampt */
	int	f_iLastDBErrNo;			/* Last db error number. 0 = no error */
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
	dprintf("in closeMySQL\n");
	if(pData->f_hmysql != NULL)	/* just to be on the safe side... */
		mysql_close(pData->f_hmysql);	
}

BEGINfreeInstance
CODESTARTfreeInstance
#	ifdef	WITH_DB
	closeMySQL(pData);
#	endif
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


static rsRetVal reInitMySQL(instanceData *);


/**
 * DBErrorHandler
 *
 * Call this function if an db error apears. It will initiate 
 * the "delay" handling which stopped the db logging for some 
 * time.  
 *
 * We now check if we have a valid MySQL handle. If not, we simply
 * report an error, but can not be specific. RGerhards, 2007-01-30
 */
static void DBErrorHandler(instanceData *pData)
{
	char errMsg[512];

	assert(pData != NULL);

	/* TODO:
	 * NO DB connection -> Can not log to DB
	 * -------------------- 
	 * Case 1: db server unavailable
	 * We can check after a specified time interval if the server is up.
	 * Also a reason can be a down DNS service.
	 * Case 2: uid, pwd or dbname are incorrect
	 * If this is a fault in the syslog.conf we have no chance to recover. But
	 * if it is a problem of the DB we can make a retry after some time. Possible
	 * are that the admin has not already set up the database table. Or he has not
	 * created the database user yet. 
	 * Case 3: unkown error
	 * If we get an unkowon error here, we should in any case try to recover after
	 * a specified time interval.
	 *
	 * Insert failed -> Can not log to DB
	 * -------------------- 
	 * If the insert fails it is never a good idea to give up. Only an
	 * invalid sql sturcture (wrong template) force us to disable db
	 * logging. 
	 *
	 * Think about different "delay" for different errors!
	 */
	if(pData->f_hmysql == NULL) {
		logerror("unknown DB error occured - called error handler with NULL MySQL handle.");
	} else { /* we can ask mysql for the error description... */
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			"db error (%d): %s\n", mysql_errno(pData->f_hmysql),
			mysql_error(pData->f_hmysql));
		pData->f_iLastDBErrNo = mysql_errno(pData->f_hmysql);
		logerror(errMsg);
	}
		
	/* Enable "delay" */
	pData->f_timeResumeOnError = time(&pData->f_timeResumeOnError) + _DB_DELAYTIMEONERROR ;
}


/**
 * checkDBErrorState
 *
 * Check if we can go on with database logging or if we should wait
 * a little bit longer. It also check if the DB hanlde is still valid. 
 * If it is necessary, it takes action to reinitiate the db connection.
 *
 * \ret int		Returns 0 if successful (no error)
 */ 
rsRetVal checkDBErrorState(instanceData *pData)
{
	time_t now;
	assert(pData != NULL);
	/* dprintf("in checkDBErrorState, timeResumeOnError: %d\n", pData->f_timeResumeOnError); */

	/* If timeResumeOnError == 0 no error occured, 
	   we can return with 0 (no error) */
	if (pData->f_timeResumeOnError == 0)
		return RS_RET_OK;
	
	(void) time(&now);
	/* Now we know an error occured. We check timeResumeOnError
	   if we can process. If we have not reach the resume time
	   yet, we return an error status. */  
	if (pData->f_timeResumeOnError > now)
	{
		/* dprintf("Wait time is not over yet.\n"); */
		return RS_RET_ERR;
	}
	 	
	/* Ok, we can try to resume the database logging. First
	   we have to reset the status (timeResumeOnError) and
	   the last error no. */
	/* To improve this code it would be better to check 
	   if we really need to reInit the db connection. If 
	   only the insert failed and the db conncetcion is
	   still valid, we need no reInit. 
	   Of course, if an unkown error appeared, we should
	   reInit. */
	 /* rgerhards 2004-12-08: I think it is pretty unlikely
	  * that we can re-use a connection after the error. So I guess
	  * the connection must be closed and re-opened in all cases
	  * (as it is done currently). When we come back to optimize
	  * this code, we should anyhow see if there are cases where
	  * we could keep it open. I just doubt this won't be the case.
	  * I added this comment (and did not remove Michaels) just so
	  * that we all know what we are looking for.
	  */
	pData->f_timeResumeOnError = 0;
	pData->f_iLastDBErrNo = 0; 
	return reInitMySQL(pData);
}

/*
 * The following function is responsible for initializing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
static rsRetVal initMySQL(instanceData *pData)
{
	int iCounter = 0;
	rsRetVal iRet = RS_RET_OK;
	assert(pData != NULL);

	if((iRet = checkDBErrorState(pData)) != RS_RET_OK)
		return iRet;
	
	pData->f_hmysql = mysql_init(NULL);
	if(pData->f_hmysql == NULL) {
		logerror("can not initialize MySQL handle - ignoring this action");
		/* The next statement  causes a redundant message, but it is the
		 * best thing we can do in this situation. -- rgerhards, 2007-01-30
	     	 */
		 iRet = RS_RET_DISABLE_ACTION;
	} else { /* we could get the handle, now on with work... */
		do {
			/* Connect to database */
			if (!mysql_real_connect(pData->f_hmysql, pData->f_dbsrv, pData->f_dbuid,
			                        pData->f_dbpwd, pData->f_dbname, 0, NULL, 0)) {
				/* if also the second attempt failed
				   we call the error handler */
				if(iCounter)
					DBErrorHandler(pData);
			} else {
				pData->f_timeResumeOnError = 0; /* We have a working db connection */
				dprintf("connected successfully to db\n");
			}
			iCounter++;
		} while (mysql_errno(pData->f_hmysql) && iCounter<2);
	}
	return iRet;
}

/*
 * Reconnect a MySQL connection.
 * Initially added 2004-12-02
 */
static rsRetVal reInitMySQL(instanceData *pData)
{
	assert(pData != NULL);

	dprintf("reInitMySQL\n");
	closeMySQL(pData); /* close the current handle */
	return initMySQL(pData); /* new connection */   
}



/* The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28 mmeckelein
 */
rsRetVal writeMySQL(uchar *psz, instanceData *pData)
{
	int iCounter=0;
	rsRetVal iRet = RS_RET_OK;
	assert(pData != NULL);

	if((iRet = checkDBErrorState(pData)) != RS_RET_OK)
		return iRet;

	/* Now we are trying to insert the data. 
	 *
	 * If the first attampt fails we simply try a second one. If that
	 * fails too, we discard the message and enable "delay" error handling.
	 */
	do {
		/* query */
		if(mysql_query(pData->f_hmysql, (char*)psz)) {
			/* if the second attempt fails
			   we call the error handler */
			if(iCounter)
				DBErrorHandler(pData);
		}
		else {
			/* dprintf("db insert sucessfull\n"); */
		}
		iCounter++;
	} while (mysql_errno(pData->f_hmysql) && iCounter<2);
	return iRet;
}


BEGINdoAction
CODESTARTdoAction
	dprintf("\n");
	iRet = writeMySQL(ppString[0], pData);
ENDdoAction


BEGINparseSelectorAct
	int iMySQLPropErr = 0;
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* yes, the if below is redundant, but I need it now. Will go away as
	 * the code further changes.  -- rgerhards, 2007-07-25
	 */
	if(*p == '>') {
		if((iRet = createInstance(&pData)) != RS_RET_OK)
			return iRet;
	}

	switch (*p)
	{
	case '>':       /* rger 2004-10-28: added support for MySQL
			 * >server,dbname,userid,password
			 * rgerhards 2005-08-12: changed rsyslogd so that
			 * if no DB is selected and > is used, an error
			 * message is logged.
			 */
		if(bModMySQLLoaded == 0)
			logerror("To enable MySQL logging, a \"$ModLoad MySQL\" must be done - accepted for "
		        	 "the time being, but will fail in future releases.");
#ifndef	WITH_DB
		iRet = RS_RET_ERROR; /* this goes away anyhow, so it's not worth putting much effort in the return code */
		errno = 0;
		logerror("write to database action in config file, but rsyslogd compiled without "
		         "database functionality - ignored");
#else /* WITH_DB defined! */
		p++;
		
		/* Now we read the MySQL connection properties 
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
		if(*p != ';')
			--p;	/* TODO: the whole parsing of the MySQL module needs to be re-thought - but this here
				 *       is clean enough for the time being -- rgerhards, 2007-07-30
			         */
		if((iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_RQD_TPL_OPT_SQL, (uchar*) " StdSQLFmt"))
		   != RS_RET_OK)
			return iRet;
		
		/* If we detect invalid properties, we disable logging, 
		 * because right properties are vital at this place.  
		 * Retries make no sense. 
		 */
		if (iMySQLPropErr) { 
			iRet = RS_RET_ERR; /* re-vist error code when working on this module */
			dprintf("Trouble with MySQL conncetion properties.\n"
				"MySQL logging disabled.\n");
			break;
		} else {
			initMySQL(pData);
		}
#endif	/* #ifdef WITH_DB */
		break;
	default:
		iRet = RS_RET_CONFLINE_UNPROCESSED;
		break;
	}
ENDparseSelectorAct


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit(MySQL)
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
ENDmodInit

#endif /* #ifdef WITH_DB */
/*
 * vi:set ai:
 */
