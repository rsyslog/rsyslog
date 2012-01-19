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
#include "cfsysline.h"
#include "template.h"
#include "msg.h"
#include "module-template.h"
#include "unicode-helper.h"
#include "errmsg.h"

MODULE_TYPE_STRGEN
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("sm_cust_bindcdr")
STRGEN_NAME("Custom_BindCDR,sql")

/* internal structures
 */
DEF_SMOD_STATIC_DATA
DEFobjCurrIf(errmsg)

/* list of "allowed" IPs */
typedef struct allowedip_s {
	uchar *pszIP;
	struct allowedip_s *next;
} allowedip_t;

static allowedip_t *root;


/* config data */

/* check if the provided IP is (already) in the allowed list
 */
static int
isAllowed(uchar *pszIP)
{
	allowedip_t *pallow;
	int ret = 0;

	for(pallow = root ; pallow != NULL ; pallow = pallow->next) {
		if(!ustrcmp(pallow->pszIP, pszIP)) {
			ret = 1;
			goto finalize_it;
		}
	}
finalize_it: return ret;
}

/* This function is called to add an additional allowed IP. It adds
 * the IP to the linked list of them. An error is emitted if the IP
 * already exists.
 */
static rsRetVal addAllowedIP(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	allowedip_t *pNew;
	DEFiRet;

	if(isAllowed(pNewVal)) {
		errmsg.LogError(0, NO_ERRCODE, "error: allowed IP '%s' already configured "
				"duplicate ignored", pNewVal);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	CHKmalloc(pNew = malloc(sizeof(allowedip_t)));
	pNew->pszIP = pNewVal;
	pNew->next = root;
	root = pNew;
	DBGPRINTF("sm_cust_bindcdr: allowed IP '%s' added.\n", pNewVal);

finalize_it:
	if(iRet != RS_RET_OK) {
		free(pNewVal);
	}

	RETiRet;
}

/* This strgen tries to minimize the amount of reallocs be first obtaining pointers to all strings
 * needed (including their length) and then calculating the actual space required. So when we 
 * finally copy, we know exactly what we need. So we do at most one alloc.
 * An actual message sample for what we intend to parse is (one line):
  <30>Mar 24 13:01:51 named[6085]: 24-Mar-2011 13:01:51.865 queries: info: client 10.0.0.96#39762: view trusted: query: 8.6.0.9.9.4.1.4.6.1.8.3.mobilecrawler.com IN TXT + (10.0.0.96)
 */
//previos dev: #define SQL_STMT "INSERT INTO CDR(`Date`,`Time`, timeMS, client, view, query, ip) VALUES ('"
#define SQL_STMT "INSERT INTO CDR(`date`,ip,user,dest) VALUES ('"
#define ADD_SQL_DELIM \
	memcpy(*ppBuf + iBuf, "', '", sizeof("', '") - 1); \
	iBuf += sizeof("', '") - 1;
#define SQL_STMT_END "');\n"
BEGINstrgen
	int iBuf;
	uchar *psz;
	uchar szDate[64];
	unsigned lenDate;
	uchar szTime[64];
	unsigned lenTime;
	uchar szMSec[64];
	unsigned lenMSec;
	uchar szClient[64];
	unsigned lenClient;
	uchar szView[64];
	unsigned lenView;
	uchar szQuery[64];
	unsigned lenQuery;
	uchar szIP[64];
	unsigned lenIP;
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
	/* Note that there are two date fields present, one in the header
	 * and one more in the actual message. We use the one from the message
	 * and parse that our. We check validity based on some fixe fields. In-
	 * depth verification is probably not worth the effort (CPU time), because
	 * we do various other checks on the message format below).
	 */
	psz = getMSG(pMsg);
	if(psz[0] == ' ' && psz[3] == '-' && psz[7] == '-') {
		memcpy(szDate, psz+8,  4);
		szDate[4] = '-';
		if(!strncmp((char*)psz+4, "Jan", 3)) {
			szDate[5] = '0';
			szDate[6] = '1';
		} else if(!strncmp((char*)psz+4, "Feb", 3)) {
			szDate[5] = '0';
			szDate[6] = '2';
		} else if(!strncmp((char*)psz+4, "Mar", 3)) {
			szDate[5] = '0';
			szDate[6] = '3';
		} else if(!strncmp((char*)psz+4, "Apr", 3)) {
			szDate[5] = '0';
			szDate[6] = '4';
		} else if(!strncmp((char*)psz+4, "May", 3)) {
			szDate[5] = '0';
			szDate[6] = '5';
		} else if(!strncmp((char*)psz+4, "Jun", 3)) {
			szDate[5] = '0';
			szDate[6] = '6';
		} else if(!strncmp((char*)psz+4, "Jul", 3)) {
			szDate[5] = '0';
			szDate[6] = '7';
		} else if(!strncmp((char*)psz+4, "Aug", 3)) {
			szDate[5] = '0';
			szDate[6] = '8';
		} else if(!strncmp((char*)psz+4, "Sep", 3)) {
			szDate[5] = '0';
			szDate[6] = '9';
		} else if(!strncmp((char*)psz+4, "Oct", 3)) {
			szDate[5] = '1';
			szDate[6] = '0';
		} else if(!strncmp((char*)psz+4, "Nov", 3)) {
			szDate[5] = '1';
			szDate[6] = '1';
		} else if(!strncmp((char*)psz+4, "Dec", 3)) {
			szDate[5] = '1';
			szDate[6] = '2';
		}
		szDate[7] = '-';
		szDate[8] = psz[1];
		szDate[9] = psz[2];
		szDate[10] = '\0';
		lenDate = 10;
	} else {
		dbgprintf("Custom_BindCDR: date part in msg missing\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* now time (pull both regular time and ms) */
	if(psz[12] == ' ' && psz[15] == ':' && psz[18] == ':' && psz[21] == '.' && psz[25] == ' ') {
		memcpy(szTime, (char*)psz+13, 8);
		szTime[9] = '\0';
		lenTime = 8;
		memcpy(szMSec, (char*)psz+22, 3);
		szMSec[4] = '\0';
		lenMSec = 3;
	} else {
		dbgprintf("Custom_BindCDR: date part in msg missing\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* "client" */
	psz = (uchar*) strstr((char*) getMSG(pMsg), "client ");
	if(psz == NULL) {
		dbgprintf("Custom_BindCDR: client part in msg missing\n");
		ABORT_FINALIZE(RS_RET_ERR);
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
		ABORT_FINALIZE(RS_RET_ERR);
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
		ABORT_FINALIZE(RS_RET_ERR);
	} else {
		psz += sizeof("query: ") - 1; /* skip "label" */
		/* first find end-of-strihttp://www.rsyslog.com/doc/omruleset.htmlng to process */
		while(*psz && (isdigit(*psz) || *psz == '.')) {
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
		ABORT_FINALIZE(RS_RET_ERR);
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

	/* now check if the IP is "allowed", in which case we should not
	 * insert into the database.
	 */
	if(isAllowed(szIP)) {
		DBGPRINTF("sm_cust_bindcdr: message from allowed IP, ignoring\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	/* calculate len, constants for spaces and similar fixed strings */
	lenTotal = lenDate + lenTime + lenMSec + lenClient + lenView + lenQuery
		   + lenIP + 7 * 5 + sizeof(SQL_STMT) + sizeof(SQL_STMT_END) + 2;

	/* now make sure buffer is large enough */
	if(lenTotal  >= *pLenBuf)
		CHKiRet(ExtendBuf(ppBuf, pLenBuf, lenTotal));

	/* and concatenate the resulting string */
	memcpy(*ppBuf, SQL_STMT, sizeof(SQL_STMT) - 1);
	iBuf = sizeof(SQL_STMT) - 1;

	memcpy(*ppBuf + iBuf, szDate, lenDate);
	iBuf += lenDate;
	/* prviously: ADD_SQL_DELIM */
	*(*ppBuf + iBuf) = ' ';
	++iBuf;

	memcpy(*ppBuf + iBuf, szTime, lenTime);
	iBuf += lenTime;
	ADD_SQL_DELIM

	/* we shall now discard this part
	memcpy(*ppBuf + iBuf, szMSec, lenMSec);
	iBuf += lenMSec;
	ADD_SQL_DELIM
	*/

	/* Note that this seem to be the IP to use */
	memcpy(*ppBuf + iBuf, szClient, lenClient);
	iBuf += lenClient;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szView, lenView);
	iBuf += lenView;
	ADD_SQL_DELIM

	memcpy(*ppBuf + iBuf, szQuery, lenQuery);
	iBuf += lenQuery;
	/* this is now the last field, so we dont need: ADD_SQL_DELIM */

	/* no longer to be included
	memcpy(*ppBuf + iBuf, szIP, lenIP);
	iBuf += lenIP;
	*/

	/* end of SQL statement/trailer (NUL is contained in string!) */
	memcpy(*ppBuf + iBuf, SQL_STMT_END, sizeof(SQL_STMT_END));
	iBuf += sizeof(SQL_STMT_END);

finalize_it:
ENDstrgen


BEGINmodExit
	allowedip_t *pallow, *pdel;
CODESTARTmodExit
	for(pallow = root ; pallow != NULL ; ) {
		pdel = pallow;
		pallow = pallow->next;
		free(pdel->pszIP);
		free(pdel);
	}

	objRelease(errmsg, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_SMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

	root = NULL;
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"sgcustombindcdrallowedip", 0, eCmdHdlrGetWord,
		addAllowedIP, NULL, STD_LOADABLE_MODULE_ID));
	dbgprintf("rsyslog sm_cust_bindcdr called, compiled with version %s\n", VERSION);
ENDmodInit
