/* omusrmsg.c
 * This is the implementation of the build-in output module for MySQL.
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
#include "ommysql.h"
#include "mysql/mysql.h" 
#include "mysql/errmsg.h"


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
static void DBErrorHandler(register selector_t *f)
{
	char errMsg[512];

	assert(f != NULL);

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
	if(f->f_un.f_mysql.f_hmysql == NULL) {
		logerror("unknown DB error occured - called error handler with NULL MySQL handle.");
	} else { /* we can ask mysql for the error description... */
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			"db error (%d): %s\n", mysql_errno(f->f_un.f_mysql.f_hmysql),
			mysql_error(f->f_un.f_mysql.f_hmysql));
		f->f_un.f_mysql.f_iLastDBErrNo = mysql_errno(f->f_un.f_mysql.f_hmysql);
		logerror(errMsg);
	}
		
	/* Enable "delay" */
	f->f_un.f_mysql.f_timeResumeOnError = time(&f->f_un.f_mysql.f_timeResumeOnError) + _DB_DELAYTIMEONERROR ;
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
int checkDBErrorState(register selector_t *f)
{
	time_t now;
	assert(f != NULL);
	/* dprintf("in checkDBErrorState, timeResumeOnError: %d\n", f->f_timeResumeOnError); */

	/* If timeResumeOnError == 0 no error occured, 
	   we can return with 0 (no error) */
	if (f->f_un.f_mysql.f_timeResumeOnError == 0)
		return 0;
	
	(void) time(&now);
	/* Now we know an error occured. We check timeResumeOnError
	   if we can process. If we have not reach the resume time
	   yet, we return an error status. */  
	if (f->f_un.f_mysql.f_timeResumeOnError > now)
	{
		/* dprintf("Wait time is not over yet.\n"); */
		return 1;
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
	f->f_un.f_mysql.f_timeResumeOnError = 0;
	f->f_un.f_mysql.f_iLastDBErrNo = 0; 
	reInitMySQL(f);
	return 0;

}

/*
 * The following function is responsible for initializing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
void initMySQL(register selector_t *f)
{
	int iCounter = 0;
	assert(f != NULL);

	if (checkDBErrorState(f))
		return;
	
	f->f_un.f_mysql.f_hmysql = mysql_init(NULL);
	if(f->f_un.f_mysql.f_hmysql == NULL) {
		logerror("can not initialize MySQL handle - ignoring this action");
		/* The next statement  causes a redundant message, but it is the
		 * best thing we can do in this situation. -- rgerhards, 2007-01-30
	     	 */
		 f->f_type = F_UNUSED;
	} else { /* we could get the handle, now on with work... */
		do {
			/* Connect to database */
			if (!mysql_real_connect(f->f_un.f_mysql.f_hmysql, f->f_un.f_mysql.f_dbsrv, f->f_un.f_mysql.f_dbuid,
			                        f->f_un.f_mysql.f_dbpwd, f->f_un.f_mysql.f_dbname, 0, NULL, 0)) {
				/* if also the second attempt failed
				   we call the error handler */
				if(iCounter)
					DBErrorHandler(f);
			} else {
				f->f_un.f_mysql.f_timeResumeOnError = 0; /* We have a working db connection */
				dprintf("connected successfully to db\n");
			}
			iCounter++;
		} while (mysql_errno(f->f_un.f_mysql.f_hmysql) && iCounter<2);
	}
}

/*
 * The following function is responsible for closing a
 * MySQL connection.
 * Initially added 2004-10-28
 */
void closeMySQL(register selector_t *f)
{
	assert(f != NULL);
	dprintf("in closeMySQL\n");
	if(f->f_un.f_mysql.f_hmysql != NULL)	/* just to be on the safe side... */
		mysql_close(f->f_un.f_mysql.f_hmysql);	
}

/*
 * Reconnect a MySQL connection.
 * Initially added 2004-12-02
 */
void reInitMySQL(register selector_t *f)
{
	assert(f != NULL);

	dprintf("reInitMySQL\n");
	closeMySQL(f); /* close the current handle */
	initMySQL(f); /* new connection */   
}



/* The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28 mmeckelein
 */
void writeMySQL(register selector_t *f)
{
	char *psz;
	int iCounter=0;
	assert(f != NULL);

	iovCreate(f);
	psz = iovAsString(f);
	
	if (checkDBErrorState(f))
		return;

	/* Now we are trying to insert the data. 
	 *
	 * If the first attampt fails we simply try a second one. If that
	 * fails too, we discard the message and enable "delay" error handling.
	 */
	do {
		/* query */
		if(mysql_query(f->f_un.f_mysql.f_hmysql, psz)) {
			/* if the second attempt fails
			   we call the error handler */
			if(iCounter)
				DBErrorHandler(f);
		}
		else {
			/* dprintf("db insert sucessfully\n"); */
		}
		iCounter++;
	} while (mysql_errno(f->f_un.f_mysql.f_hmysql) && iCounter<2);
}

/* call the shell action
 * returns 0 if it succeeds, something else otherwise
 */
int doActionMySQL(selector_t *f)
{
	assert(f != NULL);

	dprintf("\n");
	writeMySQL(f);
	return 0;
}

#endif /* #ifdef WITH_DB */
/*
 * vi:set ai:
 */
