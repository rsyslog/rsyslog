/* sm_cust_bindcdr.c
 * This is a custom developed plugin to process bind information into
 * a specific SQL statement. While the actual processing may be too specific
 * to be of general use, this module serves as a template on how this type
 * of processing can be done.
 *
 * Format generated:
 * "%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"
 * Note that this is the same as smtradfile.c, except that we do have a RFC3339 timestamp. However,
 * we have copied over the code from there, it is too simple to go through all the hassle
 * of having a single code base.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2011-03-17 by RGerhards
 *
 * Copyright 2011 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "conf.h"
#include "syslogd-types.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "unicode-helper.h"

MODULE_TYPE_STRGEN
MODULE_TYPE_NOKEEP
STRGEN_NAME("Custom_BindCDR,sql")

/* internal structures
 */
DEF_SMOD_STATIC_DATA


/* config data */


/* This strgen tries to minimize the amount of reallocs be first obtaining pointers to all strings
 * needed (including their length) and then calculating the actual space required. So when we 
 * finally copy, we know exactly what we need. So we do at most one alloc.
 */
//#define SQL_STMT "INSERT INTO CDR(date,time,client,view,query,ip) VALUES ('"
#define SQL_STMT "INSERT INTO bind_test(date,time,client,view,query,ip) VALUES ('"
BEGINstrgen
	register int iBuf;
	uchar *pTimeStamp;
	size_t lenTimeStamp;
	size_t lenTotal;
CODESTARTstrgen
	/* first obtain all strings and their length (if not fixed) */
	pTimeStamp = (uchar*) getTimeReported(pMsg, tplFmtRFC3339Date);
	lenTimeStamp = ustrlen(pTimeStamp);

	/* calculate len, constants for spaces and similar fixed strings */
	lenTotal = lenTimeStamp + 1 + 200 /* test! */ + 2;

	/* now make sure buffer is large enough */
	if(lenTotal  >= *pLenBuf)
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, lenTotal));

	/* and concatenate the resulting string */
	memcpy(*ppBuf, SQL_STMT, sizeof(SQL_STMT) - 1);
	iBuf = sizeof(SQL_STMT) - 1;

	// SQL content:DATE,TIME,CLIENT,VIEW,QUERY,IP); 

	memcpy(*ppBuf + iBuf, pTimeStamp, lenTimeStamp);
	iBuf += lenTimeStamp;
	memcpy(*ppBuf + iBuf, "' , '", sizeof("', '") - 1);
	iBuf += sizeof("', '") - 1;

	/* end of SQL statement/trailer (NUL is contained in string!) */
	memcpy(*ppBuf + iBuf, "');", sizeof("');"));
	iBuf += sizeof("');");

finalize_it:
ENDstrgen


BEGINmodExit
CODESTARTmodExit
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_SMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr

	dbgprintf("rsyslog sm_cust_bindcdr called, compiled with version %s\n", VERSION);
ENDmodInit
