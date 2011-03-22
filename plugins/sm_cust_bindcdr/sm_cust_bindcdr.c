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
#include <ctype.h>
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
//#define SQL_STMT "INSERT INTO bind_test(`Date`,`time`,client,view,query,ip) VALUES ('"
#define SQL_STMT "INSERT INTO bind_test(`Date`,ip) VALUES ('"
#define ADD_SQL_DELIM \
	memcpy(*ppBuf + iBuf, "', '", sizeof("', '") - 1); \
	iBuf += sizeof("', '") - 1;
#define SQL_STMT_END "');\n"
BEGINstrgen
	register int iBuf;
	uchar *psz;
	uchar *pTimeStamp;
	uchar szClient[64];
	unsigned lenClient;
	uchar szView[64];
	unsigned lenView;
	uchar szQuery[64];
	unsigned lenQuery;
	uchar szIP[64];
	unsigned lenIP;
	size_t lenTimeStamp;
	size_t lenTotal;
CODESTARTstrgen
	/* first create an empty statement. This is to be replaced if
	 * we have better data to fill in.
	 */
	/* now make sure buffer is large enough */
	if(*pLenBuf < 2)
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, 2));
	memcpy(*ppBuf, ";", sizeof(";"));

	/* first obtain all strings and their length (if not fixed) */
	pTimeStamp = (uchar*) getTimeReported(pMsg, tplFmtRFC3339Date);
	lenTimeStamp = ustrlen(pTimeStamp);
	
	/* "client" */
	psz = (uchar*) strstr((char*) getMSG(pMsg), "client ");
	if(psz == NULL) {
		dbgprintf("Custom_BindCDR: client part in msg missing\n");
		FINALIZE;
	} else {
		psz += sizeof("client ") - 1; /* skip "label" */
		for(  lenClient = 0
		    ; *psz && *psz != '#' && lenClient < sizeof(szClient) - 1
		    ; ++lenClient) {
			szClient[lenClient] = *psz++;
		}
		szClient[lenClient] = '\0';
	}

	/* "view" */
	psz = (uchar*) strstr((char*) getMSG(pMsg), "view ");
	if(psz == NULL) {
		dbgprintf("Custom_BindCDR: view part in msg missing\n");
		FINALIZE;
	} else {
		psz += sizeof("view ") - 1; /* skip "label" */
		for(  lenView = 0
		    ; *psz && *psz != ':' && lenView < sizeof(szView) - 1
		    ; ++lenView) {
			szView[lenView] = *psz++;
		}
		szView[lenView] = '\0';
	}

	/* "query" - we must extract just the number, and in reverse! */
	psz = (uchar*) strstr((char*) getMSG(pMsg), "query: ");
	if(psz == NULL) {
		dbgprintf("Custom_BindCDR: query part in msg missing\n");
		FINALIZE;
	} else {
		psz += sizeof("query: ") - 1; /* skip "label" */
		/* first find end-of-string to process */
		while(*psz && (isdigit(*psz) || *psz == '.')) {
dbgprintf("XXXX: step 1: %c\n", *psz);
			psz++;
		}
		/* now shuffle data */
		for(  lenQuery = 0
		    ; *psz && *psz != ' ' && lenQuery < sizeof(szQuery) - 1
		    ; --psz) {
			if(isdigit(*psz))
				szQuery[lenQuery++] = *psz;
		}
		szQuery[lenQuery] = '\0';
	}

	/* "ip" */
	psz = (uchar*) strstr((char*) getMSG(pMsg), "IN TXT + (");
	if(psz == NULL) {
		dbgprintf("Custom_BindCDR: ip part in msg missing\n");
		FINALIZE;
	} else {
		psz += sizeof("IN TXT + (") - 1; /* skip "label" */
		for(  lenIP = 0
		    ; *psz && *psz != ')' && lenIP < sizeof(szIP) - 1
		    ; ++lenIP) {
			szIP[lenIP] = *psz++;
		}
		szIP[lenIP] = '\0';
	}


	/* --- strings extracted ---- */

	/* calculate len, constants for spaces and similar fixed strings */
	lenTotal = lenTimeStamp + lenClient + lenView + lenQuery + lenIP + 5 * 5
		   + sizeof(SQL_STMT) + sizeof(SQL_STMT_END) + 2;

	/* now make sure buffer is large enough */
	if(lenTotal  >= *pLenBuf)
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, lenTotal));

	/* and concatenate the resulting string */
	memcpy(*ppBuf, SQL_STMT, sizeof(SQL_STMT) - 1);
	iBuf = sizeof(SQL_STMT) - 1;

	// SQL content:DATE,TIME,CLIENT,VIEW,QUERY,IP); 

	memcpy(*ppBuf + iBuf, pTimeStamp, lenTimeStamp);
	iBuf += lenTimeStamp;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szClient, lenClient);
	iBuf += lenClient;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szView, lenView);
	iBuf += lenView;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szQuery, lenQuery);
	iBuf += lenQuery;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szIP, lenIP);
	iBuf += lenIP;
	ADD_SQL_DELIM

	/* end of SQL statement/trailer (NUL is contained in string!) */
	memcpy(*ppBuf + iBuf, SQL_STMT_END, sizeof(SQL_STMT_END));
	iBuf += sizeof(SQL_STMT_END);

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
