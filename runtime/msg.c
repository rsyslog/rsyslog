/* msg.c
 * The msg object. Implementation of all msg-related functions
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007-2013 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#define SYSLOG_NAMES
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/socket.h>
#if HAVE_SYSINFO_UPTIME
#include <sys/sysinfo.h>
#endif
#include <netdb.h>
#include <libestr.h>
#include <json/json.h>
/* For struct json_object_iter, should not be necessary in future versions */
#include <json/json_object_private.h>
#if HAVE_MALLOC_H
#  include <malloc.h>
#endif
#ifdef USE_LIBUUID
  #include <uuid/uuid.h>
#endif
#include "rsyslog.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "template.h"
#include "msg.h"
#include "datetime.h"
#include "glbl.h"
#include "regexp.h"
#include "atomic.h"
#include "unicode-helper.h"
#include "ruleset.h"
#include "prop.h"
#include "net.h"
#include "var.h"
#include "rsconf.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(datetime)
DEFobjCurrIf(glbl)
DEFobjCurrIf(regexp)
DEFobjCurrIf(prop)
DEFobjCurrIf(net)
DEFobjCurrIf(var)

static char *two_digits[100] = {
	"00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99"};

static struct {
	uchar *pszName;
	short lenName;
} syslog_pri_names[192] = {
	{ UCHAR_CONSTANT("0"), 3},
	{ UCHAR_CONSTANT("1"), 3},
	{ UCHAR_CONSTANT("2"), 3},
	{ UCHAR_CONSTANT("3"), 3},
	{ UCHAR_CONSTANT("4"), 3},
	{ UCHAR_CONSTANT("5"), 3},
	{ UCHAR_CONSTANT("6"), 3},
	{ UCHAR_CONSTANT("7"), 3},
	{ UCHAR_CONSTANT("8"), 3},
	{ UCHAR_CONSTANT("9"), 3},
	{ UCHAR_CONSTANT("10"), 4},
	{ UCHAR_CONSTANT("11"), 4},
	{ UCHAR_CONSTANT("12"), 4},
	{ UCHAR_CONSTANT("13"), 4},
	{ UCHAR_CONSTANT("14"), 4},
	{ UCHAR_CONSTANT("15"), 4},
	{ UCHAR_CONSTANT("16"), 4},
	{ UCHAR_CONSTANT("17"), 4},
	{ UCHAR_CONSTANT("18"), 4},
	{ UCHAR_CONSTANT("19"), 4},
	{ UCHAR_CONSTANT("20"), 4},
	{ UCHAR_CONSTANT("21"), 4},
	{ UCHAR_CONSTANT("22"), 4},
	{ UCHAR_CONSTANT("23"), 4},
	{ UCHAR_CONSTANT("24"), 4},
	{ UCHAR_CONSTANT("25"), 4},
	{ UCHAR_CONSTANT("26"), 4},
	{ UCHAR_CONSTANT("27"), 4},
	{ UCHAR_CONSTANT("28"), 4},
	{ UCHAR_CONSTANT("29"), 4},
	{ UCHAR_CONSTANT("30"), 4},
	{ UCHAR_CONSTANT("31"), 4},
	{ UCHAR_CONSTANT("32"), 4},
	{ UCHAR_CONSTANT("33"), 4},
	{ UCHAR_CONSTANT("34"), 4},
	{ UCHAR_CONSTANT("35"), 4},
	{ UCHAR_CONSTANT("36"), 4},
	{ UCHAR_CONSTANT("37"), 4},
	{ UCHAR_CONSTANT("38"), 4},
	{ UCHAR_CONSTANT("39"), 4},
	{ UCHAR_CONSTANT("40"), 4},
	{ UCHAR_CONSTANT("41"), 4},
	{ UCHAR_CONSTANT("42"), 4},
	{ UCHAR_CONSTANT("43"), 4},
	{ UCHAR_CONSTANT("44"), 4},
	{ UCHAR_CONSTANT("45"), 4},
	{ UCHAR_CONSTANT("46"), 4},
	{ UCHAR_CONSTANT("47"), 4},
	{ UCHAR_CONSTANT("48"), 4},
	{ UCHAR_CONSTANT("49"), 4},
	{ UCHAR_CONSTANT("50"), 4},
	{ UCHAR_CONSTANT("51"), 4},
	{ UCHAR_CONSTANT("52"), 4},
	{ UCHAR_CONSTANT("53"), 4},
	{ UCHAR_CONSTANT("54"), 4},
	{ UCHAR_CONSTANT("55"), 4},
	{ UCHAR_CONSTANT("56"), 4},
	{ UCHAR_CONSTANT("57"), 4},
	{ UCHAR_CONSTANT("58"), 4},
	{ UCHAR_CONSTANT("59"), 4},
	{ UCHAR_CONSTANT("60"), 4},
	{ UCHAR_CONSTANT("61"), 4},
	{ UCHAR_CONSTANT("62"), 4},
	{ UCHAR_CONSTANT("63"), 4},
	{ UCHAR_CONSTANT("64"), 4},
	{ UCHAR_CONSTANT("65"), 4},
	{ UCHAR_CONSTANT("66"), 4},
	{ UCHAR_CONSTANT("67"), 4},
	{ UCHAR_CONSTANT("68"), 4},
	{ UCHAR_CONSTANT("69"), 4},
	{ UCHAR_CONSTANT("70"), 4},
	{ UCHAR_CONSTANT("71"), 4},
	{ UCHAR_CONSTANT("72"), 4},
	{ UCHAR_CONSTANT("73"), 4},
	{ UCHAR_CONSTANT("74"), 4},
	{ UCHAR_CONSTANT("75"), 4},
	{ UCHAR_CONSTANT("76"), 4},
	{ UCHAR_CONSTANT("77"), 4},
	{ UCHAR_CONSTANT("78"), 4},
	{ UCHAR_CONSTANT("79"), 4},
	{ UCHAR_CONSTANT("80"), 4},
	{ UCHAR_CONSTANT("81"), 4},
	{ UCHAR_CONSTANT("82"), 4},
	{ UCHAR_CONSTANT("83"), 4},
	{ UCHAR_CONSTANT("84"), 4},
	{ UCHAR_CONSTANT("85"), 4},
	{ UCHAR_CONSTANT("86"), 4},
	{ UCHAR_CONSTANT("87"), 4},
	{ UCHAR_CONSTANT("88"), 4},
	{ UCHAR_CONSTANT("89"), 4},
	{ UCHAR_CONSTANT("90"), 4},
	{ UCHAR_CONSTANT("91"), 4},
	{ UCHAR_CONSTANT("92"), 4},
	{ UCHAR_CONSTANT("93"), 4},
	{ UCHAR_CONSTANT("94"), 4},
	{ UCHAR_CONSTANT("95"), 4},
	{ UCHAR_CONSTANT("96"), 4},
	{ UCHAR_CONSTANT("97"), 4},
	{ UCHAR_CONSTANT("98"), 4},
	{ UCHAR_CONSTANT("99"), 4},
	{ UCHAR_CONSTANT("100"), 5},
	{ UCHAR_CONSTANT("101"), 5},
	{ UCHAR_CONSTANT("102"), 5},
	{ UCHAR_CONSTANT("103"), 5},
	{ UCHAR_CONSTANT("104"), 5},
	{ UCHAR_CONSTANT("105"), 5},
	{ UCHAR_CONSTANT("106"), 5},
	{ UCHAR_CONSTANT("107"), 5},
	{ UCHAR_CONSTANT("108"), 5},
	{ UCHAR_CONSTANT("109"), 5},
	{ UCHAR_CONSTANT("110"), 5},
	{ UCHAR_CONSTANT("111"), 5},
	{ UCHAR_CONSTANT("112"), 5},
	{ UCHAR_CONSTANT("113"), 5},
	{ UCHAR_CONSTANT("114"), 5},
	{ UCHAR_CONSTANT("115"), 5},
	{ UCHAR_CONSTANT("116"), 5},
	{ UCHAR_CONSTANT("117"), 5},
	{ UCHAR_CONSTANT("118"), 5},
	{ UCHAR_CONSTANT("119"), 5},
	{ UCHAR_CONSTANT("120"), 5},
	{ UCHAR_CONSTANT("121"), 5},
	{ UCHAR_CONSTANT("122"), 5},
	{ UCHAR_CONSTANT("123"), 5},
	{ UCHAR_CONSTANT("124"), 5},
	{ UCHAR_CONSTANT("125"), 5},
	{ UCHAR_CONSTANT("126"), 5},
	{ UCHAR_CONSTANT("127"), 5},
	{ UCHAR_CONSTANT("128"), 5},
	{ UCHAR_CONSTANT("129"), 5},
	{ UCHAR_CONSTANT("130"), 5},
	{ UCHAR_CONSTANT("131"), 5},
	{ UCHAR_CONSTANT("132"), 5},
	{ UCHAR_CONSTANT("133"), 5},
	{ UCHAR_CONSTANT("134"), 5},
	{ UCHAR_CONSTANT("135"), 5},
	{ UCHAR_CONSTANT("136"), 5},
	{ UCHAR_CONSTANT("137"), 5},
	{ UCHAR_CONSTANT("138"), 5},
	{ UCHAR_CONSTANT("139"), 5},
	{ UCHAR_CONSTANT("140"), 5},
	{ UCHAR_CONSTANT("141"), 5},
	{ UCHAR_CONSTANT("142"), 5},
	{ UCHAR_CONSTANT("143"), 5},
	{ UCHAR_CONSTANT("144"), 5},
	{ UCHAR_CONSTANT("145"), 5},
	{ UCHAR_CONSTANT("146"), 5},
	{ UCHAR_CONSTANT("147"), 5},
	{ UCHAR_CONSTANT("148"), 5},
	{ UCHAR_CONSTANT("149"), 5},
	{ UCHAR_CONSTANT("150"), 5},
	{ UCHAR_CONSTANT("151"), 5},
	{ UCHAR_CONSTANT("152"), 5},
	{ UCHAR_CONSTANT("153"), 5},
	{ UCHAR_CONSTANT("154"), 5},
	{ UCHAR_CONSTANT("155"), 5},
	{ UCHAR_CONSTANT("156"), 5},
	{ UCHAR_CONSTANT("157"), 5},
	{ UCHAR_CONSTANT("158"), 5},
	{ UCHAR_CONSTANT("159"), 5},
	{ UCHAR_CONSTANT("160"), 5},
	{ UCHAR_CONSTANT("161"), 5},
	{ UCHAR_CONSTANT("162"), 5},
	{ UCHAR_CONSTANT("163"), 5},
	{ UCHAR_CONSTANT("164"), 5},
	{ UCHAR_CONSTANT("165"), 5},
	{ UCHAR_CONSTANT("166"), 5},
	{ UCHAR_CONSTANT("167"), 5},
	{ UCHAR_CONSTANT("168"), 5},
	{ UCHAR_CONSTANT("169"), 5},
	{ UCHAR_CONSTANT("170"), 5},
	{ UCHAR_CONSTANT("171"), 5},
	{ UCHAR_CONSTANT("172"), 5},
	{ UCHAR_CONSTANT("173"), 5},
	{ UCHAR_CONSTANT("174"), 5},
	{ UCHAR_CONSTANT("175"), 5},
	{ UCHAR_CONSTANT("176"), 5},
	{ UCHAR_CONSTANT("177"), 5},
	{ UCHAR_CONSTANT("178"), 5},
	{ UCHAR_CONSTANT("179"), 5},
	{ UCHAR_CONSTANT("180"), 5},
	{ UCHAR_CONSTANT("181"), 5},
	{ UCHAR_CONSTANT("182"), 5},
	{ UCHAR_CONSTANT("183"), 5},
	{ UCHAR_CONSTANT("184"), 5},
	{ UCHAR_CONSTANT("185"), 5},
	{ UCHAR_CONSTANT("186"), 5},
	{ UCHAR_CONSTANT("187"), 5},
	{ UCHAR_CONSTANT("188"), 5},
	{ UCHAR_CONSTANT("189"), 5},
	{ UCHAR_CONSTANT("190"), 5},
	{ UCHAR_CONSTANT("191"), 5}
	};
static char hexdigit[16] =
	{'0', '1', '2', '3', '4', '5', '6', '7', '8',
	 '9', 'A', 'B', 'C', 'D', 'E', 'F' };

/*syslog facility names (as of RFC5424) */
static char *syslog_fac_names[24] = { "kern", "user", "mail", "daemon", "auth", "syslog", "lpr",
			    	      "news", "uucp", "cron", "authpriv", "ftp", "ntp", "audit",
			    	      "alert", "clock", "local0", "local1", "local2", "local3",
			    	      "local4", "local5", "local6", "local7" };
/* length of the facility names string (for optimizatiions) */
static short len_syslog_fac_names[24] = { 4, 4, 4, 6, 4, 6, 3,
			    	          4, 4, 4, 8, 3, 3, 5,
			    	          5, 5, 6, 6, 6, 6,
			    	          6, 6, 6, 6 };

/* table of severity names (in numerical order)*/
static char *syslog_severity_names[8] = { "emerg", "alert", "crit", "err", "warning", "notice", "info", "debug" };
static short len_syslog_severity_names[8] = { 5, 5, 4, 3, 7, 6, 4, 5 };

/* numerical values as string - this is the most efficient approach to convert severity
 * and facility values to a numerical string... -- rgerhars, 2009-06-17
 */

static char *syslog_number_names[24] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
					 "15", "16", "17", "18", "19", "20", "21", "22", "23" };

/* global variables */
#if defined(HAVE_MALLOC_TRIM) && !defined(HAVE_ATOMIC_BUILTINS)
static pthread_mutex_t mutTrimCtr;	 /* mutex to handle malloc trim */
#endif

/* some forward declarations */
static int getAPPNAMELen(msg_t *pM, sbool bLockMutex);
static rsRetVal jsonPathFindParent(msg_t *pM, uchar *name, uchar *leaf, struct json_object **parent, int bCreate);
static uchar * jsonPathGetLeaf(uchar *name, int lenName);
static struct json_object *jsonDeepCopy(struct json_object *src);


/* the locking and unlocking implementations: */
static inline void
MsgLock(msg_t *pThis)
{
	/* DEV debug only! dbgprintf("MsgLock(0x%lx)\n", (unsigned long) pThis); */
	pthread_mutex_lock(&pThis->mut);
}
static inline void
MsgUnlock(msg_t *pThis)
{
	/* DEV debug only! dbgprintf("MsgUnlock(0x%lx)\n", (unsigned long) pThis); */
	pthread_mutex_unlock(&pThis->mut);
}


/* set RcvFromIP name in msg object WITHOUT calling AddRef.
 * rgerhards, 2013-01-22
 */
static inline void
MsgSetRcvFromIPWithoutAddRef(msg_t *pThis, prop_t *new)
{
	if(pThis->pRcvFromIP != NULL)
		prop.Destruct(&pThis->pRcvFromIP);
	pThis->pRcvFromIP = new;
}


/* set RcvFrom name in msg object WITHOUT calling AddRef.
 * rgerhards, 2013-01-22
 */
void MsgSetRcvFromWithoutAddRef(msg_t *pThis, prop_t *new)
{
	assert(pThis != NULL);

	if(pThis->msgFlags & NEEDS_DNSRESOL) {
		if(pThis->rcvFrom.pfrominet != NULL)
			free(pThis->rcvFrom.pfrominet);
		pThis->msgFlags &= ~NEEDS_DNSRESOL;
	} else {
		if(pThis->rcvFrom.pRcvFrom != NULL)
			prop.Destruct(&pThis->rcvFrom.pRcvFrom);
	}
	pThis->rcvFrom.pRcvFrom = new;
}


/* rgerhards 2012-04-18: set associated ruleset (by ruleset name)
 * If ruleset cannot be found, no update is done.
 */
static void
MsgSetRulesetByName(msg_t *pMsg, cstr_t *rulesetName)
{
	rulesetGetRuleset(runConf, &(pMsg->pRuleset), rsCStrGetSzStrNoNULL(rulesetName));
}


static inline int getProtocolVersion(msg_t *pM)
{
	return(pM->iProtocolVersion);
}


/* do a DNS reverse resolution, if not already done, reflect status
 * rgerhards, 2009-11-16
 */
static inline rsRetVal
resolveDNS(msg_t *pMsg) {
	rsRetVal localRet;
	prop_t *propFromHost = NULL;
	prop_t *ip;
	prop_t *localName;
	DEFiRet;

	MsgLock(pMsg);
	CHKiRet(objUse(net, CORE_COMPONENT));
	if(pMsg->msgFlags & NEEDS_DNSRESOL) {
		localRet = net.cvthname(pMsg->rcvFrom.pfrominet, &localName, NULL, &ip);
		if(localRet == RS_RET_OK) {
			/* we pass down the props, so no need for AddRef */
			MsgSetRcvFromWithoutAddRef(pMsg, localName);
			MsgSetRcvFromIPWithoutAddRef(pMsg, ip);
		}
	}
finalize_it:
	if(iRet != RS_RET_OK) {
		/* best we can do: remove property */
		MsgSetRcvFromStr(pMsg, UCHAR_CONSTANT(""), 0, &propFromHost);
		prop.Destruct(&propFromHost);
	}
	MsgUnlock(pMsg);
	if(propFromHost != NULL)
		prop.Destruct(&propFromHost);
	RETiRet;
}


static inline void
getInputName(msg_t *pM, uchar **ppsz, int *plen)
{
	BEGINfunc
	if(pM == NULL || pM->pInputName == NULL) {
		*ppsz = UCHAR_CONSTANT("");
		*plen = 0;
	} else {
		prop.GetString(pM->pInputName, ppsz, plen);
	}
	ENDfunc
}


static inline uchar*
getRcvFromIP(msg_t *pM)
{
	uchar *psz;
	int len;
	BEGINfunc
	if(pM == NULL) {
		psz = UCHAR_CONSTANT("");
	} else {
		resolveDNS(pM); /* make sure we have a resolved entry */
		if(pM->pRcvFromIP == NULL)
			psz = UCHAR_CONSTANT("");
		else
			prop.GetString(pM->pRcvFromIP, &psz, &len);
	}
	ENDfunc
	return psz;
}


/* map a property name (C string) to a property ID */
rsRetVal
propNameStrToID(uchar *pName, propid_t *pPropID)
{
	DEFiRet;

	assert(pName != NULL);

	/* sometimes there are aliases to the original MonitoWare
	 * property names. These come after || in the ifs below. */
	if(!strcmp((char*) pName, "msg")) {
		*pPropID = PROP_MSG;
	} else if(!strcmp((char*) pName, "timestamp")
		  || !strcmp((char*) pName, "timereported")) {
		*pPropID = PROP_TIMESTAMP;
	} else if(!strcmp((char*) pName, "hostname") || !strcmp((char*) pName, "source")) {
		*pPropID = PROP_HOSTNAME;
	} else if(!strcmp((char*) pName, "syslogtag")) {
		*pPropID = PROP_SYSLOGTAG;
	} else if(!strcmp((char*) pName, "rawmsg")) {
		*pPropID = PROP_RAWMSG;
	} else if(!strcmp((char*) pName, "inputname")) {
		*pPropID = PROP_INPUTNAME;
	} else if(!strcmp((char*) pName, "fromhost")) {
		*pPropID = PROP_FROMHOST;
	} else if(!strcmp((char*) pName, "fromhost-ip")) {
		*pPropID = PROP_FROMHOST_IP;
	} else if(!strcmp((char*) pName, "pri")) {
		*pPropID = PROP_PRI;
	} else if(!strcmp((char*) pName, "pri-text")) {
		*pPropID = PROP_PRI_TEXT;
	} else if(!strcmp((char*) pName, "iut")) {
		*pPropID = PROP_IUT;
	} else if(!strcmp((char*) pName, "syslogfacility")) {
		*pPropID = PROP_SYSLOGFACILITY;
	} else if(!strcmp((char*) pName, "syslogfacility-text")) {
		*pPropID = PROP_SYSLOGFACILITY_TEXT;
	} else if(!strcmp((char*) pName, "syslogseverity") || !strcmp((char*) pName, "syslogpriority")) {
		*pPropID = PROP_SYSLOGSEVERITY;
	} else if(!strcmp((char*) pName, "syslogseverity-text") || !strcmp((char*) pName, "syslogpriority-text")) {
		*pPropID = PROP_SYSLOGSEVERITY_TEXT;
	} else if(!strcmp((char*) pName, "timegenerated")) {
		*pPropID = PROP_TIMEGENERATED;
	} else if(!strcmp((char*) pName, "programname")) {
		*pPropID = PROP_PROGRAMNAME;
	} else if(!strcmp((char*) pName, "protocol-version")) {
		*pPropID = PROP_PROTOCOL_VERSION;
	} else if(!strcmp((char*) pName, "structured-data")) {
		*pPropID = PROP_STRUCTURED_DATA;
	} else if(!strcmp((char*) pName, "app-name")) {
		*pPropID = PROP_APP_NAME;
	} else if(!strcmp((char*) pName, "procid")) {
		*pPropID = PROP_PROCID;
	} else if(!strcmp((char*) pName, "msgid")) {
		*pPropID = PROP_MSGID;
	} else if(!strcmp((char*) pName, "parsesuccess")) {
		*pPropID = PROP_PARSESUCCESS;
#ifdef USE_LIBUUID
	} else if(!strcmp((char*) pName, "uuid")) {
		*pPropID = PROP_UUID;
#endif
	/* here start system properties (those, that do not relate to the message itself */
	} else if(!strcmp((char*) pName, "$now")) {
		*pPropID = PROP_SYS_NOW;
	} else if(!strcmp((char*) pName, "$year")) {
		*pPropID = PROP_SYS_YEAR;
	} else if(!strcmp((char*) pName, "$month")) {
		*pPropID = PROP_SYS_MONTH;
	} else if(!strcmp((char*) pName, "$day")) {
		*pPropID = PROP_SYS_DAY;
	} else if(!strcmp((char*) pName, "$hour")) {
		*pPropID = PROP_SYS_HOUR;
	} else if(!strcmp((char*) pName, "$hhour")) {
		*pPropID = PROP_SYS_HHOUR;
	} else if(!strcmp((char*) pName, "$qhour")) {
		*pPropID = PROP_SYS_QHOUR;
	} else if(!strcmp((char*) pName, "$minute")) {
		*pPropID = PROP_SYS_MINUTE;
	} else if(!strcmp((char*) pName, "$myhostname")) {
		*pPropID = PROP_SYS_MYHOSTNAME;
	} else if(!strcmp((char*) pName, "$!all-json")) {
		*pPropID = PROP_CEE_ALL_JSON;
	} else if(!strncmp((char*) pName, "$!", 2)) {
		*pPropID = PROP_CEE;
	} else if(!strcmp((char*) pName, "$bom")) {
		*pPropID = PROP_SYS_BOM;
	} else if(!strcmp((char*) pName, "$uptime")) {
		*pPropID = PROP_SYS_UPTIME;
	} else {
		*pPropID = PROP_INVALID;
		iRet = RS_RET_VAR_NOT_FOUND;
	}

	RETiRet;
}


/* map a property name (string) to a property ID */
rsRetVal
propNameToID(cstr_t *pCSPropName, propid_t *pPropID)
{
	uchar *pName;
	DEFiRet;

	assert(pCSPropName != NULL);
	assert(pPropID != NULL);
	pName = rsCStrGetSzStrNoNULL(pCSPropName);
	iRet =  propNameStrToID(pName, pPropID);
	RETiRet;
}


/* map a property ID to a name string (useful for displaying) */
uchar *propIDToName(propid_t propID)
{
	switch(propID) {
		case PROP_MSG:
			return UCHAR_CONSTANT("msg");
		case PROP_TIMESTAMP:
			return UCHAR_CONSTANT("timestamp");
		case PROP_HOSTNAME:
			return UCHAR_CONSTANT("hostname");
		case PROP_SYSLOGTAG:
			return UCHAR_CONSTANT("syslogtag");
		case PROP_RAWMSG:
			return UCHAR_CONSTANT("rawmsg");
		case PROP_INPUTNAME:
			return UCHAR_CONSTANT("inputname");
		case PROP_FROMHOST:
			return UCHAR_CONSTANT("fromhost");
		case PROP_FROMHOST_IP:
			return UCHAR_CONSTANT("fromhost-ip");
		case PROP_PRI:
			return UCHAR_CONSTANT("pri");
		case PROP_PRI_TEXT:
			return UCHAR_CONSTANT("pri-text");
		case PROP_IUT:
			return UCHAR_CONSTANT("iut");
		case PROP_SYSLOGFACILITY:
			return UCHAR_CONSTANT("syslogfacility");
		case PROP_SYSLOGFACILITY_TEXT:
			return UCHAR_CONSTANT("syslogfacility-text");
		case PROP_SYSLOGSEVERITY:
			return UCHAR_CONSTANT("syslogseverity");
		case PROP_SYSLOGSEVERITY_TEXT:
			return UCHAR_CONSTANT("syslogseverity-text");
		case PROP_TIMEGENERATED:
			return UCHAR_CONSTANT("timegenerated");
		case PROP_PROGRAMNAME:
			return UCHAR_CONSTANT("programname");
		case PROP_PROTOCOL_VERSION:
			return UCHAR_CONSTANT("protocol-version");
		case PROP_STRUCTURED_DATA:
			return UCHAR_CONSTANT("structured-data");
		case PROP_APP_NAME:
			return UCHAR_CONSTANT("app-name");
		case PROP_PROCID:
			return UCHAR_CONSTANT("procid");
		case PROP_MSGID:
			return UCHAR_CONSTANT("msgid");
		case PROP_PARSESUCCESS:
			return UCHAR_CONSTANT("parsesuccess");
		case PROP_SYS_NOW:
			return UCHAR_CONSTANT("$NOW");
		case PROP_SYS_YEAR:
			return UCHAR_CONSTANT("$YEAR");
		case PROP_SYS_MONTH:
			return UCHAR_CONSTANT("$MONTH");
		case PROP_SYS_DAY:
			return UCHAR_CONSTANT("$DAY");
		case PROP_SYS_HOUR:
			return UCHAR_CONSTANT("$HOUR");
		case PROP_SYS_HHOUR:
			return UCHAR_CONSTANT("$HHOUR");
		case PROP_SYS_QHOUR:
			return UCHAR_CONSTANT("$QHOUR");
		case PROP_SYS_MINUTE:
			return UCHAR_CONSTANT("$MINUTE");
		case PROP_SYS_MYHOSTNAME:
			return UCHAR_CONSTANT("$MYHOSTNAME");
		case PROP_CEE:
			return UCHAR_CONSTANT("*CEE-based property*");
		case PROP_CEE_ALL_JSON:
			return UCHAR_CONSTANT("$!all-json");
		case PROP_SYS_BOM:
			return UCHAR_CONSTANT("$BOM");
		case PROP_UUID:
			return UCHAR_CONSTANT("uuid");
		default:
			return UCHAR_CONSTANT("*invalid property id*");
	}
}


/* This is common code for all Constructors. It is defined in an
 * inline'able function so that we can save a function call in the
 * actual constructors (otherwise, the msgConstruct would need
 * to call msgConstructWithTime(), which would require a
 * function call). Now, both can use this inline function. This
 * enables us to be optimal, but still have the code just once.
 * the new object or NULL if no such object could be allocated.
 * An object constructed via this function should only be destroyed
 * via "msgDestruct()". This constructor does not query system time
 * itself but rather uses a user-supplied value. This enables the caller
 * to do some tricks to save processing time (done, for example, in the
 * udp input).
 * NOTE: this constructor does NOT call calloc(), as we have many bytes
 * inside the structure which do not need to be cleared. bzero() will
 * heavily thrash the cache, so we do the init manually (which also
 * is the right thing to do with pointers, as they are not neccessarily
 * a binary 0 on all machines [but today almost always...]).
 * rgerhards, 2008-10-06
 */
static inline rsRetVal msgBaseConstruct(msg_t **ppThis)
{
	DEFiRet;
	msg_t *pM;

	assert(ppThis != NULL);
	CHKmalloc(pM = MALLOC(sizeof(msg_t)));
	objConstructSetObjInfo(pM); /* intialize object helper entities */

	/* initialize members in ORDER they appear in structure (think "cache line"!) */
	pM->flowCtlType = 0;
	pM->bParseSuccess = 0;
	pM->iRefCount = 1;
	pM->iSeverity = -1;
	pM->iFacility = -1;
	pM->iLenPROGNAME = -1;
	pM->offAfterPRI = 0;
	pM->offMSG = -1;
	pM->iProtocolVersion = 0;
	pM->msgFlags = 0;
	pM->iLenRawMsg = 0;
	pM->iLenMSG = 0;
	pM->iLenTAG = 0;
	pM->iLenHOSTNAME = 0;
	pM->pszRawMsg = NULL;
	pM->pszHOSTNAME = NULL;
	pM->pszRcvdAt3164 = NULL;
	pM->pszRcvdAt3339 = NULL;
	pM->pszRcvdAt_MySQL = NULL;
        pM->pszRcvdAt_PgSQL = NULL;
	pM->pszTIMESTAMP3164 = NULL;
	pM->pszTIMESTAMP3339 = NULL;
	pM->pszTIMESTAMP_MySQL = NULL;
        pM->pszTIMESTAMP_PgSQL = NULL;
	pM->pCSStrucData = NULL;
	pM->pCSAPPNAME = NULL;
	pM->pCSPROCID = NULL;
	pM->pCSMSGID = NULL;
	pM->pInputName = NULL;
	pM->pRcvFromIP = NULL;
	pM->rcvFrom.pRcvFrom = NULL;
	pM->pRuleset = NULL;
	pM->json = NULL;
	memset(&pM->tRcvdAt, 0, sizeof(pM->tRcvdAt));
	memset(&pM->tTIMESTAMP, 0, sizeof(pM->tTIMESTAMP));
	pM->TAG.pszTAG = NULL;
	pM->pszTimestamp3164[0] = '\0';
	pM->pszTimestamp3339[0] = '\0';
	pM->pszTIMESTAMP_SecFrac[0] = '\0';
	pM->pszRcvdAt_SecFrac[0] = '\0';
	pM->pszTIMESTAMP_Unix[0] = '\0';
	pM->pszRcvdAt_Unix[0] = '\0';
	pM->pszUUID = NULL;
	pthread_mutex_init(&pM->mut, NULL);

	/* DEV debugging only! dbgprintf("msgConstruct\t0x%x, ref 1\n", (int)pM);*/

	*ppThis = pM;

finalize_it:
	RETiRet;
}


/* "Constructor" for a msg "object". Returns a pointer to
 * the new object or NULL if no such object could be allocated.
 * An object constructed via this function should only be destroyed
 * via "msgDestruct()". This constructor does not query system time
 * itself but rather uses a user-supplied value. This enables the caller
 * to do some tricks to save processing time (done, for example, in the
 * udp input).
 * rgerhards, 2008-10-06
 */
rsRetVal msgConstructWithTime(msg_t **ppThis, struct syslogTime *stTime, time_t ttGenTime)
{
	DEFiRet;

	CHKiRet(msgBaseConstruct(ppThis));
	(*ppThis)->ttGenTime = ttGenTime;
	memcpy(&(*ppThis)->tRcvdAt, stTime, sizeof(struct syslogTime));
	memcpy(&(*ppThis)->tTIMESTAMP, stTime, sizeof(struct syslogTime));

finalize_it:
	RETiRet;
}


/* "Constructor" for a msg "object". Returns a pointer to
 * the new object or NULL if no such object could be allocated.
 * An object constructed via this function should only be destroyed
 * via "msgDestruct()". This constructor, for historical reasons,
 * also sets the two timestamps to the current time.
 */
rsRetVal msgConstruct(msg_t **ppThis)
{
	DEFiRet;

	CHKiRet(msgBaseConstruct(ppThis));
	/* we initialize both timestamps to contain the current time, so that they
	 * are consistent. Also, this saves us from doing any further time calls just
	 * to obtain a timestamp. The memcpy() should not really make a difference,
	 * especially as I think there is no codepath currently where it would not be
	 * required (after I have cleaned up the pathes ;)). -- rgerhards, 2008-10-02
	 */
	datetime.getCurrTime(&((*ppThis)->tRcvdAt), &((*ppThis)->ttGenTime));
	memcpy(&(*ppThis)->tTIMESTAMP, &(*ppThis)->tRcvdAt, sizeof(struct syslogTime));

finalize_it:
	RETiRet;
}


/* Special msg constructor, to be used when an object is deserialized.
 * we do only the base init as we know the properties will be set in
 * any case by the deserializer. We still do the "inexpensive" inits
 * just to be on the safe side. The whole process needs to be
 * refactored together with the msg serialization subsystem.
 */
rsRetVal
msgConstructForDeserializer(msg_t **ppThis)
{
	return msgBaseConstruct(ppThis);
}


/* some free handlers for (slightly) complicated cases... All of them may be called
 * with an empty element.
 */
static inline void freeTAG(msg_t *pThis)
{
	if(pThis->iLenTAG >= CONF_TAG_BUFSIZE)
		free(pThis->TAG.pszTAG);
}
static inline void freeHOSTNAME(msg_t *pThis)
{
	if(pThis->iLenHOSTNAME >= CONF_HOSTNAME_BUFSIZE)
		free(pThis->pszHOSTNAME);
}


BEGINobjDestruct(msg) /* be sure to specify the object type also in END and CODESTART macros! */
	int currRefCount;
#	if HAVE_MALLOC_TRIM
	int currCnt;
#	endif
CODESTARTobjDestruct(msg)
	/* DEV Debugging only ! dbgprintf("msgDestruct\t0x%lx, Ref now: %d\n", (unsigned long)pThis, pThis->iRefCount - 1); */
#	ifdef HAVE_ATOMIC_BUILTINS
		currRefCount = ATOMIC_DEC_AND_FETCH(&pThis->iRefCount, NULL);
#	else
		MsgLock(pThis);
		currRefCount = --pThis->iRefCount;
# 	endif
	if(currRefCount == 0)
	{
		/* DEV Debugging Only! dbgprintf("msgDestruct\t0x%lx, RefCount now 0, doing DESTROY\n", (unsigned long)pThis); */
		if(pThis->pszRawMsg != pThis->szRawMsg)
			free(pThis->pszRawMsg);
		freeTAG(pThis);
		freeHOSTNAME(pThis);
		if(pThis->pInputName != NULL)
			prop.Destruct(&pThis->pInputName);
		if((pThis->msgFlags & NEEDS_DNSRESOL) == 0) {
			if(pThis->rcvFrom.pRcvFrom != NULL)
				prop.Destruct(&pThis->rcvFrom.pRcvFrom);
		} else {
			free(pThis->rcvFrom.pfrominet);
		}
		if(pThis->pRcvFromIP != NULL)
			prop.Destruct(&pThis->pRcvFromIP);
		free(pThis->pszRcvdAt3164);
		free(pThis->pszRcvdAt3339);
		free(pThis->pszRcvdAt_MySQL);
		free(pThis->pszRcvdAt_PgSQL);
		free(pThis->pszTIMESTAMP_MySQL);
		free(pThis->pszTIMESTAMP_PgSQL);
		if(pThis->iLenPROGNAME >= CONF_PROGNAME_BUFSIZE)
			free(pThis->PROGNAME.ptr);
		if(pThis->pCSStrucData != NULL)
			rsCStrDestruct(&pThis->pCSStrucData);
		if(pThis->pCSAPPNAME != NULL)
			rsCStrDestruct(&pThis->pCSAPPNAME);
		if(pThis->pCSPROCID != NULL)
			rsCStrDestruct(&pThis->pCSPROCID);
		if(pThis->pCSMSGID != NULL)
			rsCStrDestruct(&pThis->pCSMSGID);
		if(pThis->json != NULL)
			json_object_put(pThis->json);
		if(pThis->pszUUID != NULL)
			free(pThis->pszUUID);
#	ifndef HAVE_ATOMIC_BUILTINS
		MsgUnlock(pThis);
# 	endif
		pthread_mutex_destroy(&pThis->mut);
		/* now we need to do our own optimization. Testing has shown that at least the glibc
		 * malloc() subsystem returns memory to the OS far too late in our case. So we need
		 * to help it a bit, by calling malloc_trim(), which will tell the alloc subsystem
		 * to consolidate and return to the OS. We keep 128K for our use, as a safeguard
		 * to too-frequent reallocs. But more importantly, we call this hook only every
		 * 100,000 messages (which is an approximation, as we do not work with atomic
		 * operations on the counter. --- rgerhards, 2009-06-22.
		 */
#		if HAVE_MALLOC_TRIM
		{	/* standard C requires a new block for a new variable definition!
			 * To simplify matters, we use modulo arithmetic and live with the fact
			 * that we trim too often when the counter wraps.
			 */
			static unsigned iTrimCtr = 1;
			currCnt = ATOMIC_INC_AND_FETCH_unsigned(&iTrimCtr, &mutTrimCtr);
			if(currCnt % 100000 == 0) {
				malloc_trim(128*1024);
			}
		}
#		endif
	} else {
#	ifndef HAVE_ATOMIC_BUILTINS
		MsgUnlock(pThis);
# 	endif
		pThis = NULL; /* tell framework not to destructing the object! */
	}
ENDobjDestruct(msg)


/* The macros below are used in MsgDup(). I use macros
 * to keep the fuction code somewhat more readyble. It is my
 * replacement for inline functions in CPP
 */
#define tmpCOPYSZ(name) \
	if(pOld->psz##name != NULL) { \
		if((pNew->psz##name = srUtilStrDup(pOld->psz##name, pOld->iLen##name)) == NULL) {\
			msgDestruct(&pNew);\
			return NULL;\
		}\
		pNew->iLen##name = pOld->iLen##name;\
	}

/* copy the CStr objects.
 * if the old value is NULL, we do not need to do anything because we
 * initialized the new value to NULL via calloc().
 */
#define tmpCOPYCSTR(name) \
	if(pOld->pCS##name != NULL) {\
		if(rsCStrConstructFromCStr(&(pNew->pCS##name), pOld->pCS##name) != RS_RET_OK) {\
			msgDestruct(&pNew);\
			return NULL;\
		}\
	}
/* Constructs a message object by duplicating another one.
 * Returns NULL if duplication failed. We do not need to lock the
 * message object here, because a fully-created msg object is never
 * allowed to be manipulated. For this, MsgDup() must be used, so MsgDup()
 * can never run into a situation where the message object is being
 * modified while its content is copied - it's forbidden by definition.
 * rgerhards, 2007-07-10
 */
msg_t* MsgDup(msg_t* pOld)
{
	msg_t* pNew;
	rsRetVal localRet;

	assert(pOld != NULL);

	BEGINfunc
	if(msgConstructWithTime(&pNew, &pOld->tTIMESTAMP, pOld->ttGenTime) != RS_RET_OK) {
		return NULL;
	}

	/* now copy the message properties */
	pNew->iRefCount = 1;
	pNew->iSeverity = pOld->iSeverity;
	pNew->iFacility = pOld->iFacility;
	pNew->msgFlags = pOld->msgFlags;
	pNew->iProtocolVersion = pOld->iProtocolVersion;
	pNew->ttGenTime = pOld->ttGenTime;
	pNew->offMSG = pOld->offMSG;
	pNew->iLenRawMsg = pOld->iLenRawMsg;
	pNew->iLenMSG = pOld->iLenMSG;
	pNew->iLenTAG = pOld->iLenTAG;
	pNew->iLenHOSTNAME = pOld->iLenHOSTNAME;
	if((pOld->msgFlags & NEEDS_DNSRESOL)) {
			localRet = msgSetFromSockinfo(pNew, pOld->rcvFrom.pfrominet);
			if(localRet != RS_RET_OK) {
				/* if something fails, we accept loss of this property, it is
				 * better than losing the whole message.
				 */
				pNew->msgFlags &= ~NEEDS_DNSRESOL;
				pNew->rcvFrom.pRcvFrom = NULL; /* make sure no dangling values */
			}
	} else {
		if(pOld->rcvFrom.pRcvFrom != NULL) {
			pNew->rcvFrom.pRcvFrom = pOld->rcvFrom.pRcvFrom;
			prop.AddRef(pNew->rcvFrom.pRcvFrom);
		}
	}
	if(pOld->pRcvFromIP != NULL) {
		pNew->pRcvFromIP = pOld->pRcvFromIP;
		prop.AddRef(pNew->pRcvFromIP);
	}
	if(pOld->pInputName != NULL) {
		pNew->pInputName = pOld->pInputName;
		prop.AddRef(pNew->pInputName);
	}
	if(pOld->iLenTAG > 0) {
		if(pOld->iLenTAG < CONF_TAG_BUFSIZE) {
			memcpy(pNew->TAG.szBuf, pOld->TAG.szBuf, pOld->iLenTAG + 1);
		} else {
			if((pNew->TAG.pszTAG = srUtilStrDup(pOld->TAG.pszTAG, pOld->iLenTAG)) == NULL) {
				msgDestruct(&pNew);
				return NULL;
			}
			pNew->iLenTAG = pOld->iLenTAG;
		}
	}
	if(pOld->iLenRawMsg < CONF_RAWMSG_BUFSIZE) {
		memcpy(pNew->szRawMsg, pOld->szRawMsg, pOld->iLenRawMsg + 1);
		pNew->pszRawMsg = pNew->szRawMsg;
	} else {
		tmpCOPYSZ(RawMsg);
	}
	if(pOld->pszHOSTNAME == NULL) {
		pNew->pszHOSTNAME = NULL;
	} else {
		if(pOld->iLenHOSTNAME < CONF_HOSTNAME_BUFSIZE) {
			memcpy(pNew->szHOSTNAME, pOld->szHOSTNAME, pOld->iLenHOSTNAME + 1);
			pNew->pszHOSTNAME = pNew->szHOSTNAME;
		} else {
			tmpCOPYSZ(HOSTNAME);
		}
	}

	tmpCOPYCSTR(StrucData);
	tmpCOPYCSTR(APPNAME);
	tmpCOPYCSTR(PROCID);
	tmpCOPYCSTR(MSGID);

	if(pOld->json != NULL)
		pNew->json = jsonDeepCopy(pOld->json);

	/* we do not copy all other cache properties, as we do not even know
	 * if they are needed once again. So we let them re-create if needed.
	 */

	ENDfunc
	return pNew;
}
#undef tmpCOPYSZ
#undef tmpCOPYCSTR


/* This method serializes a message object. That means the whole
 * object is modified into text form. That text form is suitable for
 * later reconstruction of the object by calling MsgDeSerialize().
 * The most common use case for this method is the creation of an
 * on-disk representation of the message object.
 * We do not serialize the cache properties. We re-create them when needed.
 * This saves us a lot of memory. Performance is no concern, as serializing
 * is a so slow operation that recration of the caches does not count. Also,
 * we do not serialize --currently none--, as this is only a helper variable
 * during msg construction - and never again used later.
 * rgerhards, 2008-01-03
 */
static rsRetVal MsgSerialize(msg_t *pThis, strm_t *pStrm)
{
	uchar *psz;
	int len;
	DEFiRet;

	assert(pThis != NULL);
	assert(pStrm != NULL);

	/* then serialize elements */
	CHKiRet(obj.BeginSerialize(pStrm, (obj_t*) pThis));
	objSerializeSCALAR(pStrm, iProtocolVersion, SHORT);
	objSerializeSCALAR(pStrm, iSeverity, SHORT);
	objSerializeSCALAR(pStrm, iFacility, SHORT);
	objSerializeSCALAR(pStrm, msgFlags, INT);
	objSerializeSCALAR(pStrm, ttGenTime, INT);
	objSerializeSCALAR(pStrm, tRcvdAt, SYSLOGTIME);
	objSerializeSCALAR(pStrm, tTIMESTAMP, SYSLOGTIME);

	CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszTAG"), PROPTYPE_PSZ, (void*)
		((pThis->iLenTAG < CONF_TAG_BUFSIZE) ? pThis->TAG.szBuf : pThis->TAG.pszTAG)));

	objSerializePTR(pStrm, pszRawMsg, PSZ);
	objSerializePTR(pStrm, pszHOSTNAME, PSZ);
	getInputName(pThis, &psz, &len);
	CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszInputName"), PROPTYPE_PSZ, (void*) psz));
	psz = getRcvFrom(pThis); 
	CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszRcvFrom"), PROPTYPE_PSZ, (void*) psz));
	psz = getRcvFromIP(pThis); 
	CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszRcvFromIP"), PROPTYPE_PSZ, (void*) psz));
	if(pThis->json != NULL) {
		psz = (uchar*) json_object_get_string(pThis->json);
		CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("json"), PROPTYPE_PSZ, (void*) psz));
	}

	objSerializePTR(pStrm, pCSStrucData, CSTR);
	objSerializePTR(pStrm, pCSAPPNAME, CSTR);
	objSerializePTR(pStrm, pCSPROCID, CSTR);
	objSerializePTR(pStrm, pCSMSGID, CSTR);
	
	objSerializePTR(pStrm, pszUUID, PSZ);

	if(pThis->pRuleset != NULL) {
		rulesetGetName(pThis->pRuleset);
		CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszRuleset"), PROPTYPE_PSZ,
			rulesetGetName(pThis->pRuleset)));
	}

	/* offset must be serialized after pszRawMsg, because we need that to obtain the correct
	 * MSG size.
	 */
	objSerializeSCALAR(pStrm, offMSG, SHORT);

	CHKiRet(obj.EndSerialize(pStrm));

finalize_it:
	RETiRet;
}


/* This is a helper for MsgDeserialize that re-inits the var object. This
 * whole construct should be replaced, var is really ready to be retired.
 * But as an interim help during refactoring let's introduce this function
 * here (and thus NOT as method of var object!). -- rgerhads, 2012-11-06
 */
static inline void
reinitVar(var_t *pVar)
{
	rsCStrDestruct(&pVar->pcsName); /* no longer needed */
	if(pVar->varType == VARTYPE_STR) {
		if(pVar->val.pStr != NULL)
			rsCStrDestruct(&pVar->val.pStr);
	}
}
/* deserialize the message again 
 * we deserialize the properties in the same order that we serialized them. Except
 * for some checks to cover downlevel version, we do not need to do all these
 * CPU intense name checkings.
 */
#define isProp(name) !rsCStrSzStrCmp(pVar->pcsName, (uchar*) name, sizeof(name) - 1)
rsRetVal
MsgDeserialize(msg_t *pMsg, strm_t *pStrm)
{
	prop_t *myProp;
	prop_t *propRcvFrom = NULL;
	prop_t *propRcvFromIP = NULL;
	struct json_tokener *tokener;
	struct json_object *json;
	var_t *pVar = NULL;
	DEFiRet;

	ISOBJ_TYPE_assert(pStrm, strm);

	CHKiRet(var.Construct(&pVar));
	CHKiRet(var.ConstructFinalize(pVar));

	CHKiRet(objDeserializeProperty(pVar, pStrm));
	if(isProp("iProtocolVersion")) {
		setProtocolVersion(pMsg, pVar->val.num);
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("iSeverity")) {
		pMsg->iSeverity = pVar->val.num;
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("iFacility")) {
		pMsg->iFacility = pVar->val.num;
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("msgFlags")) {
		pMsg->msgFlags = pVar->val.num;
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("ttGenTime")) {
		pMsg->ttGenTime = pVar->val.num;
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("tRcvdAt")) {
		memcpy(&pMsg->tRcvdAt, &pVar->val.vSyslogTime, sizeof(struct syslogTime));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("tTIMESTAMP")) {
		memcpy(&pMsg->tTIMESTAMP, &pVar->val.vSyslogTime, sizeof(struct syslogTime));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszTAG")) {
		MsgSetTAG(pMsg, rsCStrGetSzStrNoNULL(pVar->val.pStr), cstrLen(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszRawMsg")) {
		MsgSetRawMsg(pMsg, (char*) rsCStrGetSzStrNoNULL(pVar->val.pStr), cstrLen(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszHOSTNAME")) {
		MsgSetHOSTNAME(pMsg, rsCStrGetSzStrNoNULL(pVar->val.pStr), rsCStrLen(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszInputName")) {
		/* we need to create a property */ 
		CHKiRet(prop.Construct(&myProp));
		CHKiRet(prop.SetString(myProp, rsCStrGetSzStrNoNULL(pVar->val.pStr), rsCStrLen(pVar->val.pStr)));
		CHKiRet(prop.ConstructFinalize(myProp));
		MsgSetInputName(pMsg, myProp);
		prop.Destruct(&myProp);
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszRcvFrom")) {
		MsgSetRcvFromStr(pMsg, rsCStrGetSzStrNoNULL(pVar->val.pStr), rsCStrLen(pVar->val.pStr), &propRcvFrom);
		prop.Destruct(&propRcvFrom);
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszRcvFromIP")) {
		MsgSetRcvFromIPStr(pMsg, rsCStrGetSzStrNoNULL(pVar->val.pStr), rsCStrLen(pVar->val.pStr), &propRcvFromIP);
		prop.Destruct(&propRcvFromIP);
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("json")) {
		tokener = json_tokener_new();
		json = json_tokener_parse_ex(tokener, (char*)rsCStrGetSzStrNoNULL(pVar->val.pStr),
					     cstrLen(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pCSStrucData")) {
		MsgSetStructuredData(pMsg, (char*) rsCStrGetSzStrNoNULL(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pCSAPPNAME")) {
		MsgSetAPPNAME(pMsg, (char*) rsCStrGetSzStrNoNULL(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pCSPROCID")) {
		MsgSetPROCID(pMsg, (char*) rsCStrGetSzStrNoNULL(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pCSMSGID")) {
		MsgSetMSGID(pMsg, (char*) rsCStrGetSzStrNoNULL(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszUUID")) {
		pMsg->pszUUID = ustrdup(rsCStrGetSzStrNoNULL(pVar->val.pStr));
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	if(isProp("pszRuleset")) {
		MsgSetRulesetByName(pMsg, pVar->val.pStr);
		reinitVar(pVar);
		CHKiRet(objDeserializeProperty(pVar, pStrm));
	}
	/* "offMSG" must always be our last field, so we use this as an
	 * indicator if the sequence is correct. This is a bit questionable,
	 * but on the other hand it works decently AND we will probably replace
	 * the whole persisted format soon in any case. -- rgerhards, 2012-11-06
	 */
	if(!isProp("offMSG"))
		ABORT_FINALIZE(RS_RET_DS_PROP_SEQ_ERR);
	MsgSetMSGoffs(pMsg, pVar->val.num);
finalize_it:
	if(pVar != NULL)
		var.Destruct(&pVar);
	RETiRet;
}
#undef isProp


/* Increment reference count - see description of the "msg"
 * structure for details. As a convenience to developers,
 * this method returns the msg pointer that is passed to it.
 * It is recommended that it is called as follows:
 *
 * pSecondMsgPointer = MsgAddRef(pOrgMsgPointer);
 */
msg_t *MsgAddRef(msg_t *pM)
{
	assert(pM != NULL);
#	ifdef HAVE_ATOMIC_BUILTINS
		ATOMIC_INC(&pM->iRefCount, NULL);
#	else
		MsgLock(pM);
		pM->iRefCount++;
		MsgUnlock(pM);
#	endif
	/* DEV debugging only! dbgprintf("MsgAddRef\t0x%x done, Ref now: %d\n", (int)pM, pM->iRefCount);*/
	return(pM);
}


/* This functions tries to aquire the PROCID from TAG. Its primary use is
 * when a legacy syslog message has been received and should be forwarded as
 * syslog-protocol (or the PROCID is requested for any other reason).
 * In legacy syslog, the PROCID is considered to be the character sequence
 * between the first [ and the first ]. This usually are digits only, but we
 * do not check that. However, if there is no closing ], we do not assume we
 * can obtain a PROCID. Take in mind that not every legacy syslog message
 * actually has a PROCID.
 * rgerhards, 2005-11-24
 * THIS MUST be called with the message lock locked.
 */
static rsRetVal aquirePROCIDFromTAG(msg_t *pM)
{
	register int i;
	uchar *pszTag;
	DEFiRet;

	assert(pM != NULL);

	if(pM->pCSPROCID != NULL)
		return RS_RET_OK; /* we are already done ;) */

	if(getProtocolVersion(pM) != 0)
		return RS_RET_OK; /* we can only emulate if we have legacy format */

	pszTag = (uchar*) ((pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG);

	/* find first '['... */
	i = 0;
	while((i < pM->iLenTAG) && (pszTag[i] != '['))
		++i;
	if(!(i < pM->iLenTAG))
		return RS_RET_OK;	/* no [, so can not emulate... */
	
	++i; /* skip '[' */

	/* now obtain the PROCID string... */
	CHKiRet(cstrConstruct(&pM->pCSPROCID));
	while((i < pM->iLenTAG) && (pszTag[i] != ']')) {
		CHKiRet(cstrAppendChar(pM->pCSPROCID, pszTag[i]));
		++i;
	}

	if(!(i < pM->iLenTAG)) {
		/* oops... it looked like we had a PROCID, but now it has
		 * turned out this is not true. In this case, we need to free
		 * the buffer and simply return. Note that this is NOT an error
		 * case!
		 */
		cstrDestruct(&pM->pCSPROCID);
		FINALIZE;
	}

	/* OK, finaally we could obtain a PROCID. So let's use it ;) */
	CHKiRet(cstrFinalize(pM->pCSPROCID));

finalize_it:
	RETiRet;
}


/* Parse and set the "programname" for a given MSG object. Programname
 * is a BSD concept, it is the tag without any instance-specific information.
 * Precisely, the programname is terminated by either (whichever occurs first):
 * - end of tag
 * - nonprintable character
 * - ':'
 * - '['
 * - '/'
 * The above definition has been taken from the FreeBSD syslogd sources.
 * 
 * The program name is not parsed by default, because it is infrequently-used.
 * IMPORTANT: A locked message object must be provided, else a crash will occur.
 * rgerhards, 2005-10-19
 */
static inline rsRetVal
aquireProgramName(msg_t *pM)
{
	int i;
	uchar *pszTag, *pszProgName;
	DEFiRet;

	assert(pM != NULL);
	pszTag = (uchar*) ((pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG);
	for(  i = 0
	    ; (i < pM->iLenTAG) && isprint((int) pszTag[i])
	      && (pszTag[i] != '\0') && (pszTag[i] != ':')
	      && (pszTag[i] != '[')  && (pszTag[i] != '/')
	    ; ++i)
		; /* just search end of PROGNAME */
	if(i < CONF_PROGNAME_BUFSIZE) {
		pszProgName = pM->PROGNAME.szBuf;
	} else {
		CHKmalloc(pM->PROGNAME.ptr = malloc(i+1));
		pszProgName = pM->PROGNAME.ptr;
	}
	memcpy((char*)pszProgName, (char*)pszTag, i);
	pszProgName[i] = '\0';
	pM->iLenPROGNAME = i;
finalize_it:
	RETiRet;
}


/* Access methods - dumb & easy, not a comment for each ;)
 */
void setProtocolVersion(msg_t *pM, int iNewVersion)
{
	assert(pM != NULL);
	if(iNewVersion != 0 && iNewVersion != 1) {
		dbgprintf("Tried to set unsupported protocol version %d - changed to 0.\n", iNewVersion);
		iNewVersion = 0;
	}
	pM->iProtocolVersion = iNewVersion;
}

/* note: string is taken from constant pool, do NOT free */
char *getProtocolVersionString(msg_t *pM)
{
	assert(pM != NULL);
	return(pM->iProtocolVersion ? "1" : "0");
}

#ifdef USE_LIBUUID
/* note: libuuid seems not to be thread-safe, so we need
 * to get some safeguards in place.
 */
static void msgSetUUID(msg_t *pM)
{
	size_t lenRes = sizeof(uuid_t) * 2 + 1;
	char hex_char [] = "0123456789ABCDEF";
	unsigned int byte_nbr;
	uuid_t uuid;
	static pthread_mutex_t mutUUID = PTHREAD_MUTEX_INITIALIZER;

	dbgprintf("[MsgSetUUID] START\n");
	assert(pM != NULL);

	if((pM->pszUUID = (uchar*) MALLOC(lenRes)) == NULL) {
		pM->pszUUID = (uchar *)"";
	} else {
		pthread_mutex_lock(&mutUUID);
		uuid_generate(uuid);
		pthread_mutex_unlock(&mutUUID);
		for (byte_nbr = 0; byte_nbr < sizeof (uuid_t); byte_nbr++) {
			pM->pszUUID[byte_nbr * 2 + 0] = hex_char[uuid [byte_nbr] >> 4];
			pM->pszUUID[byte_nbr * 2 + 1] = hex_char[uuid [byte_nbr] & 15];
		}

		dbgprintf("[MsgSetUUID] UUID : %s LEN: %d \n", pM->pszUUID, (int)lenRes);
		pM->pszUUID[lenRes] = '\0';
	}
	dbgprintf("[MsgSetUUID] END\n");
}

void getUUID(msg_t *pM, uchar **pBuf, int *piLen)
{
	dbgprintf("[getUUID] START\n");
	if(pM == NULL) {
		dbgprintf("[getUUID] pM is NULL\n");
		*pBuf=	UCHAR_CONSTANT("");
		*piLen = 0;
	} else {
		if(pM->pszUUID == NULL) {
			dbgprintf("[getUUID] pM->pszUUID is NULL\n");
			MsgLock(pM);
			/* re-query, things may have changed in the mean time... */
			if(pM->pszUUID == NULL)
				msgSetUUID(pM);
			MsgUnlock(pM);
		} else { /* UUID already there we reuse it */
			dbgprintf("[getUUID] pM->pszUUID already exists\n");
		}
		*pBuf = pM->pszUUID;
		*piLen = sizeof(uuid_t) * 2;
	}
	dbgprintf("[getUUID] END\n");
}
#endif

void
getRawMsg(msg_t *pM, uchar **pBuf, int *piLen)
{
	if(pM == NULL) {
		*pBuf=  UCHAR_CONSTANT("");
		*piLen = 0;
	} else {
		if(pM->pszRawMsg == NULL) {
			*pBuf=  UCHAR_CONSTANT("");
			*piLen = 0;
		} else {
			*pBuf = pM->pszRawMsg;
			*piLen = pM->iLenRawMsg;
		}
	}
}


/* note: setMSGLen() is only for friends who really know what they
 * do. Setting an invalid length can be desasterous!
 */
void setMSGLen(msg_t *pM, int lenMsg)
{
	pM->iLenMSG = lenMsg;
}

int getMSGLen(msg_t *pM)
{
	return((pM == NULL) ? 0 : pM->iLenMSG);
}

uchar *getMSG(msg_t *pM)
{
	uchar *ret;
	if(pM == NULL)
		ret = UCHAR_CONSTANT("");
	else {
		if(pM->iLenMSG == 0)
			ret = UCHAR_CONSTANT("");
		else
			ret = pM->pszRawMsg + pM->offMSG;
	}
	return ret;
}


/* Get PRI value as integer */
static int getPRIi(msg_t *pM)
{
	return (pM->iFacility << 3) + (pM->iSeverity);
}


/* Get PRI value in text form
 */
char *
getPRI(msg_t *pM)
{
	/* PRI is a number in the range 0..191. Thus, we use a simple lookup table to obtain the
	 * string value. It looks a bit clumpsy here in code ;)
	 */
	int iPRI;

	if(pM == NULL)
		return "";

	iPRI = getPRIi(pM);
	return (iPRI > 191) ? "invld" : (char*)syslog_pri_names[iPRI].pszName;
}


char *
getTimeReported(msg_t *pM, enum tplFormatTypes eFmt)
{
	BEGINfunc
	if(pM == NULL)
		return "";

	switch(eFmt) {
	case tplFmtDefault:
	case tplFmtRFC3164Date:
	case tplFmtRFC3164BuggyDate:
		MsgLock(pM);
		if(pM->pszTIMESTAMP3164 == NULL) {
			pM->pszTIMESTAMP3164 = pM->pszTimestamp3164;
			datetime.formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164,
						     (eFmt == tplFmtRFC3164BuggyDate));
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP3164);
	case tplFmtMySQLDate:
		MsgLock(pM);
		if(pM->pszTIMESTAMP_MySQL == NULL) {
			if((pM->pszTIMESTAMP_MySQL = MALLOC(15)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestampToMySQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_MySQL);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP_MySQL);
        case tplFmtPgSQLDate:
                MsgLock(pM);
                if(pM->pszTIMESTAMP_PgSQL == NULL) {
                        if((pM->pszTIMESTAMP_PgSQL = MALLOC(21)) == NULL) {
                                MsgUnlock(pM);
                                return "";
                        }
                        datetime.formatTimestampToPgSQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_PgSQL);
                }
                MsgUnlock(pM);
                return(pM->pszTIMESTAMP_PgSQL);
	case tplFmtRFC3339Date:
		MsgLock(pM);
		if(pM->pszTIMESTAMP3339 == NULL) {
			pM->pszTIMESTAMP3339 = pM->pszTimestamp3339;
			datetime.formatTimestamp3339(&pM->tTIMESTAMP, pM->pszTIMESTAMP3339);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP3339);
	case tplFmtUnixDate:
		MsgLock(pM);
		if(pM->pszTIMESTAMP_Unix[0] == '\0') {
			datetime.formatTimestampUnix(&pM->tTIMESTAMP, pM->pszTIMESTAMP_Unix);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP_Unix);
	case tplFmtSecFrac:
		if(pM->pszTIMESTAMP_SecFrac[0] == '\0') {
			MsgLock(pM);
			/* re-check, may have changed while we did not hold lock */
			if(pM->pszTIMESTAMP_SecFrac[0] == '\0') {
				datetime.formatTimestampSecFrac(&pM->tTIMESTAMP, pM->pszTIMESTAMP_SecFrac);
			}
			MsgUnlock(pM);
		}
		return(pM->pszTIMESTAMP_SecFrac);
	}
	ENDfunc
	return "INVALID eFmt OPTION!";
}

static inline char *getTimeGenerated(msg_t *pM, enum tplFormatTypes eFmt)
{
	BEGINfunc
	if(pM == NULL)
		return "";

	switch(eFmt) {
	case tplFmtDefault:
		MsgLock(pM);
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = MALLOC(16)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 0);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3164);
	case tplFmtMySQLDate:
		MsgLock(pM);
		if(pM->pszRcvdAt_MySQL == NULL) {
			if((pM->pszRcvdAt_MySQL = MALLOC(15)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestampToMySQL(&pM->tRcvdAt, pM->pszRcvdAt_MySQL);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt_MySQL);
        case tplFmtPgSQLDate:
                MsgLock(pM);
                if(pM->pszRcvdAt_PgSQL == NULL) {
                        if((pM->pszRcvdAt_PgSQL = MALLOC(21)) == NULL) {
                                MsgUnlock(pM);
                                return "";
                        }
                        datetime.formatTimestampToPgSQL(&pM->tRcvdAt, pM->pszRcvdAt_PgSQL);
                }
                MsgUnlock(pM);
                return(pM->pszRcvdAt_PgSQL);
	case tplFmtRFC3164Date:
	case tplFmtRFC3164BuggyDate:
		MsgLock(pM);
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = MALLOC(16)) == NULL) {
					MsgUnlock(pM);
					return "";
				}
			datetime.formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164,
						     (eFmt == tplFmtRFC3164BuggyDate));
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3164);
	case tplFmtRFC3339Date:
		MsgLock(pM);
		if(pM->pszRcvdAt3339 == NULL) {
			if((pM->pszRcvdAt3339 = MALLOC(33)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestamp3339(&pM->tRcvdAt, pM->pszRcvdAt3339);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3339);
	case tplFmtUnixDate:
		MsgLock(pM);
		if(pM->pszRcvdAt_Unix[0] == '\0') {
			datetime.formatTimestampUnix(&pM->tRcvdAt, pM->pszRcvdAt_Unix);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt_Unix);
	case tplFmtSecFrac:
		if(pM->pszRcvdAt_SecFrac[0] == '\0') {
			MsgLock(pM);
			/* re-check, may have changed while we did not hold lock */
			if(pM->pszRcvdAt_SecFrac[0] == '\0') {
				datetime.formatTimestampSecFrac(&pM->tRcvdAt, pM->pszRcvdAt_SecFrac);
			}
			MsgUnlock(pM);
		}
		return(pM->pszRcvdAt_SecFrac);
	}
	ENDfunc
	return "INVALID eFmt OPTION!";
}


static inline char *getSeverity(msg_t *pM)
{
	char *name = NULL;

	if(pM == NULL)
		return "";

	if(pM->iSeverity < 0 || pM->iSeverity > 7) {
		name = "invld";
	} else {
		name = syslog_number_names[pM->iSeverity];
	}

	return name;
}


static inline char *getSeverityStr(msg_t *pM)
{
	char *name = NULL;

	if(pM == NULL)
		return "";

	if(pM->iSeverity < 0 || pM->iSeverity > 7) {
		name = "invld";
	} else {
		name = syslog_severity_names[pM->iSeverity];
	}

	return name;
}

static inline char *getFacility(msg_t *pM)
{
	char *name = NULL;

	if(pM == NULL)
		return "";

	if(pM->iFacility < 0 || pM->iFacility > 23) {
		name = "invld";
	} else {
		name = syslog_number_names[pM->iFacility];
	}

	return name;
}

static inline char *getFacilityStr(msg_t *pM)
{
        char *name = NULL;

        if(pM == NULL)
                return "";

	if(pM->iFacility < 0 || pM->iFacility > 23) {
		name = "invld";
	} else {
		name = syslog_fac_names[pM->iFacility];
	}

	return name;
}


/* set flow control state (if not called, the default - NO_DELAY - is used)
 * This needs no locking because it is only done while the object is
 * not fully constructed (which also means you must not call this
 * method after the msg has been handed over to a queue).
 * rgerhards, 2008-03-14
 */
rsRetVal
MsgSetFlowControlType(msg_t *pMsg, flowControl_t eFlowCtl)
{
	DEFiRet;
	assert(pMsg != NULL);
	assert(eFlowCtl == eFLOWCTL_NO_DELAY || eFlowCtl == eFLOWCTL_LIGHT_DELAY || eFlowCtl == eFLOWCTL_FULL_DELAY);

	pMsg->flowCtlType = eFlowCtl;

	RETiRet;
}

/* set offset after which PRI in raw msg starts
 * rgerhards, 2009-06-16
 */
rsRetVal
MsgSetAfterPRIOffs(msg_t *pMsg, short offs)
{
	assert(pMsg != NULL);
	pMsg->offAfterPRI = offs;
	return RS_RET_OK;
}


/* rgerhards 2004-11-24: set APP-NAME in msg object
 * This is not locked, because it either is called during message
 * construction (where we need no locking) or later as part of a function
 * which already obtained the lock. So in general, this function here must
 * only be called when it it safe to do so without it aquiring a lock.
 */
rsRetVal MsgSetAPPNAME(msg_t *pMsg, char* pszAPPNAME)
{
	DEFiRet;
	assert(pMsg != NULL);
	if(pMsg->pCSAPPNAME == NULL) {
		/* we need to obtain the object first */
		CHKiRet(rsCStrConstruct(&pMsg->pCSAPPNAME));
	}
	/* if we reach this point, we have the object */
	iRet = rsCStrSetSzStr(pMsg->pCSAPPNAME, (uchar*) pszAPPNAME);

finalize_it:
	RETiRet;
}


/* rgerhards 2004-11-24: set PROCID in msg object
 */
rsRetVal MsgSetPROCID(msg_t *pMsg, char* pszPROCID)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pMsg, msg);
	if(pMsg->pCSPROCID == NULL) {
		/* we need to obtain the object first */
		CHKiRet(cstrConstruct(&pMsg->pCSPROCID));
	}
	/* if we reach this point, we have the object */
	CHKiRet(rsCStrSetSzStr(pMsg->pCSPROCID, (uchar*) pszPROCID));
	CHKiRet(cstrFinalize(pMsg->pCSPROCID));

finalize_it:
	RETiRet;
}


/* check if we have a procid, and, if not, try to aquire/emulate it.
 * This must be called WITHOUT the message lock being held.
 * rgerhards, 2009-06-26
 */
static inline void preparePROCID(msg_t *pM, sbool bLockMutex)
{
	if(pM->pCSPROCID == NULL) {
		if(bLockMutex == LOCK_MUTEX)
			MsgLock(pM);
		/* re-query, things may have changed in the mean time... */
		if(pM->pCSPROCID == NULL)
			aquirePROCIDFromTAG(pM);
		if(bLockMutex == LOCK_MUTEX)
			MsgUnlock(pM);
	}
}


#if 0
/* rgerhards, 2005-11-24
 */
static inline int getPROCIDLen(msg_t *pM, sbool bLockMutex)
{
	assert(pM != NULL);
	preparePROCID(pM, bLockMutex);
	return (pM->pCSPROCID == NULL) ? 1 : rsCStrLen(pM->pCSPROCID);
}
#endif


/* rgerhards, 2005-11-24
 */
char *getPROCID(msg_t *pM, sbool bLockMutex)
{
	uchar *pszRet;

	ISOBJ_TYPE_assert(pM, msg);
	if(bLockMutex == LOCK_MUTEX)
		MsgLock(pM);
	preparePROCID(pM, MUTEX_ALREADY_LOCKED);
	if(pM->pCSPROCID == NULL)
		pszRet = UCHAR_CONSTANT("-");
	else 
		pszRet = rsCStrGetSzStrNoNULL(pM->pCSPROCID);
	if(bLockMutex == LOCK_MUTEX)
		MsgUnlock(pM);
	return (char*) pszRet;
}


/* rgerhards 2004-11-24: set MSGID in msg object
 */
rsRetVal MsgSetMSGID(msg_t *pMsg, char* pszMSGID)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pMsg, msg);
	if(pMsg->pCSMSGID == NULL) {
		/* we need to obtain the object first */
		CHKiRet(rsCStrConstruct(&pMsg->pCSMSGID));
	}
	/* if we reach this point, we have the object */
	iRet = rsCStrSetSzStr(pMsg->pCSMSGID, (uchar*) pszMSGID);

finalize_it:
	RETiRet;
}


/* Return state of last parser. If it had success, "OK" is returned, else
 * "FAIL". All from the constant pool.
 */
static inline char *getParseSuccess(msg_t *pM)
{
	return (pM->bParseSuccess) ? "OK" : "FAIL";
}


/* al, 2011-07-26: LockMsg to avoid race conditions
 */
static inline char *getMSGID(msg_t *pM)
{
	if (pM->pCSMSGID == NULL) {
		return "-"; 
	}
	else {
		MsgLock(pM);
		char* pszreturn = (char*) rsCStrGetSzStrNoNULL(pM->pCSMSGID);
		MsgUnlock(pM);
		return pszreturn; 
	}
}

/* rgerhards 2012-03-15: set parser success (an integer, acutally bool)
 */
void MsgSetParseSuccess(msg_t *pMsg, int bSuccess)
{
	assert(pMsg != NULL);
	pMsg->bParseSuccess = bSuccess;
}

/* rgerhards 2009-06-12: set associated ruleset
 */
void MsgSetRuleset(msg_t *pMsg, ruleset_t *pRuleset)
{
	assert(pMsg != NULL);
	pMsg->pRuleset = pRuleset;
}


/* set TAG in msg object
 * (rewritten 2009-06-18 rgerhards)
 */
void MsgSetTAG(msg_t *pMsg, uchar* pszBuf, size_t lenBuf)
{
	uchar *pBuf;
	assert(pMsg != NULL);

	freeTAG(pMsg);

	pMsg->iLenTAG = lenBuf;
	if(pMsg->iLenTAG < CONF_TAG_BUFSIZE) {
		/* small enough: use fixed buffer (faster!) */
		pBuf = pMsg->TAG.szBuf;
	} else {
		if((pBuf = (uchar*) MALLOC(pMsg->iLenTAG + 1)) == NULL) {
			/* truncate message, better than completely loosing it... */
			pBuf = pMsg->TAG.szBuf;
			pMsg->iLenTAG = CONF_TAG_BUFSIZE - 1;
		} else {
			pMsg->TAG.pszTAG = pBuf;
		}
	}

	memcpy(pBuf, pszBuf, pMsg->iLenTAG);
	pBuf[pMsg->iLenTAG] = '\0'; /* this also works with truncation! */
}


/* This function tries to emulate the TAG if none is
 * set. Its primary purpose is to provide an old-style TAG
 * when a syslog-protocol message has been received. Then,
 * the tag is APP-NAME "[" PROCID "]". The function first checks
 * if there is a TAG and, if not, if it can emulate it.
 * rgerhards, 2005-11-24
 */
static inline void tryEmulateTAG(msg_t *pM, sbool bLockMutex)
{
	size_t lenTAG;
	uchar bufTAG[CONF_TAG_MAXSIZE];
	assert(pM != NULL);

	if(bLockMutex == LOCK_MUTEX)
		MsgLock(pM);
	if(pM->iLenTAG > 0) {
		if(bLockMutex == LOCK_MUTEX)
			MsgUnlock(pM);
		return; /* done, no need to emulate */
	}
	
	if(getProtocolVersion(pM) == 1) {
		if(!strcmp(getPROCID(pM, MUTEX_ALREADY_LOCKED), "-")) {
			/* no process ID, use APP-NAME only */
			MsgSetTAG(pM, (uchar*) getAPPNAME(pM, MUTEX_ALREADY_LOCKED), getAPPNAMELen(pM, MUTEX_ALREADY_LOCKED));
		} else {
			/* now we can try to emulate */
			lenTAG = snprintf((char*)bufTAG, CONF_TAG_MAXSIZE, "%s[%s]",
					  getAPPNAME(pM, MUTEX_ALREADY_LOCKED), getPROCID(pM, MUTEX_ALREADY_LOCKED));
			bufTAG[sizeof(bufTAG)-1] = '\0'; /* just to make sure... */
			MsgSetTAG(pM, bufTAG, lenTAG);
		}
	}
	if(bLockMutex == LOCK_MUTEX)
		MsgUnlock(pM);
}


void
getTAG(msg_t *pM, uchar **ppBuf, int *piLen)
{
	if(pM == NULL) {
		*ppBuf = UCHAR_CONSTANT("");
		*piLen = 0;
	} else {
		if(pM->iLenTAG == 0)
			tryEmulateTAG(pM, LOCK_MUTEX);
		if(pM->iLenTAG == 0) {
			*ppBuf = UCHAR_CONSTANT("");
			*piLen = 0;
		} else {
			*ppBuf = (pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG;
			*piLen = pM->iLenTAG;
		}
	}
}


int getHOSTNAMELen(msg_t *pM)
{
	if(pM == NULL)
		return 0;
	else
		if(pM->pszHOSTNAME == NULL) {
			resolveDNS(pM);
			if(pM->rcvFrom.pRcvFrom == NULL)
				return 0;
			else
				return prop.GetStringLen(pM->rcvFrom.pRcvFrom);
		} else
			return pM->iLenHOSTNAME;
}


char *getHOSTNAME(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszHOSTNAME == NULL) {
			resolveDNS(pM);
			if(pM->rcvFrom.pRcvFrom == NULL) {
				return "";
			} else {
				uchar *psz;
				int len;
				prop.GetString(pM->rcvFrom.pRcvFrom, &psz, &len);
				return (char*) psz;
			}
		} else {
			return (char*) pM->pszHOSTNAME;
		}
}


uchar *getRcvFrom(msg_t *pM)
{
	uchar *psz;
	int len;
	BEGINfunc

	if(pM == NULL) {
		psz = UCHAR_CONSTANT("");
	} else {
		resolveDNS(pM);
		if(pM->rcvFrom.pRcvFrom == NULL)
			psz = UCHAR_CONSTANT("");
		else
			prop.GetString(pM->rcvFrom.pRcvFrom, &psz, &len);
	}
	ENDfunc
	return psz;
}


/* rgerhards 2004-11-24: set STRUCTURED DATA in msg object
 */
rsRetVal MsgSetStructuredData(msg_t *pMsg, char* pszStrucData)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pMsg, msg);
	if(pMsg->pCSStrucData == NULL) {
		/* we need to obtain the object first */
		CHKiRet(rsCStrConstruct(&pMsg->pCSStrucData));
	}
	/* if we reach this point, we have the object */
	iRet = rsCStrSetSzStr(pMsg->pCSStrucData, (uchar*) pszStrucData);

finalize_it:
	RETiRet;
}

/* get the length of the "STRUCTURED-DATA" sz string
 * rgerhards, 2005-11-24
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int getStructuredDataLen(msg_t *pM)
{
	return (pM->pCSStrucData == NULL) ? 1 : rsCStrLen(pM->pCSStrucData);
}
#endif


/* get the "STRUCTURED-DATA" as sz string
 * rgerhards, 2005-11-24
 */
static inline char *getStructuredData(msg_t *pM)
{
	uchar *pszRet;

	MsgLock(pM);
	if(pM->pCSStrucData == NULL)
		pszRet = UCHAR_CONSTANT("-");
	else 
		pszRet = rsCStrGetSzStrNoNULL(pM->pCSStrucData);
	MsgUnlock(pM);
	return (char*) pszRet;
}

/* get the "programname" as sz string
 * rgerhards, 2005-10-19
 */
uchar *getProgramName(msg_t *pM, sbool bLockMutex)
{
	if(pM->iLenPROGNAME == -1) {
		if(bLockMutex == LOCK_MUTEX) {
			MsgLock(pM);
			/* need to re-check, things may have change in between! */
			if(pM->iLenPROGNAME == -1)
				aquireProgramName(pM);
			MsgUnlock(pM);
		} else {
			aquireProgramName(pM);
		}
	}
	return (pM->iLenPROGNAME < CONF_PROGNAME_BUFSIZE) ? pM->PROGNAME.szBuf
						       : pM->PROGNAME.ptr;
}


/* This function tries to emulate APPNAME if it is not present. Its
 * main use is when we have received a log record via legacy syslog and
 * now would like to send out the same one via syslog-protocol.
 * MUST be called with the Msg Lock locked!
 */
static void tryEmulateAPPNAME(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME != NULL)
		return; /* we are already done */

	if(getProtocolVersion(pM) == 0) {
		/* only then it makes sense to emulate */
		MsgSetAPPNAME(pM, (char*)getProgramName(pM, MUTEX_ALREADY_LOCKED));
	}
}



/* check if we have a APPNAME, and, if not, try to aquire/emulate it.
 * This must be called WITHOUT the message lock being held.
 * rgerhards, 2009-06-26
 */
static inline void prepareAPPNAME(msg_t *pM, sbool bLockMutex)
{
	if(pM->pCSAPPNAME == NULL) {
		if(bLockMutex == LOCK_MUTEX)
			MsgLock(pM);

		/* re-query as things might have changed during locking */
		if(pM->pCSAPPNAME == NULL)
			tryEmulateAPPNAME(pM);

		if(bLockMutex == LOCK_MUTEX)
			MsgUnlock(pM);
	}
}

/* rgerhards, 2005-11-24
 */
char *getAPPNAME(msg_t *pM, sbool bLockMutex)
{
	uchar *pszRet;

	assert(pM != NULL);
	if(bLockMutex == LOCK_MUTEX)
		MsgLock(pM);
	prepareAPPNAME(pM, MUTEX_ALREADY_LOCKED);
	if(pM->pCSAPPNAME == NULL)
		pszRet = UCHAR_CONSTANT("");
	else 
		pszRet = rsCStrGetSzStrNoNULL(pM->pCSAPPNAME);
	if(bLockMutex == LOCK_MUTEX)
		MsgUnlock(pM);
	return (char*)pszRet;
}

/* rgerhards, 2005-11-24
 */
static int getAPPNAMELen(msg_t *pM, sbool bLockMutex)
{
	assert(pM != NULL);
	prepareAPPNAME(pM, bLockMutex);
	return (pM->pCSAPPNAME == NULL) ? 0 : rsCStrLen(pM->pCSAPPNAME);
}

/* rgerhards 2008-09-10: set pszInputName in msg object. This calls AddRef()
 * on the property, because this must be done in all current cases and there
 * is no case expected where this may not be necessary.
 * rgerhards, 2009-06-16
 */
void MsgSetInputName(msg_t *pThis, prop_t *inputName)
{
	assert(pThis != NULL);

	prop.AddRef(inputName);
	if(pThis->pInputName != NULL)
		prop.Destruct(&pThis->pInputName);
	pThis->pInputName = inputName;
}


/* Set the pfrominet socket store, so that we can obtain the peer at some
 * later time. Note that we do not check if pRcvFrom is already set, so this
 * function must only be called during message creation.
 * NOTE: msgFlags is NOT set. While this is somewhat a violation of layers,
 * it is done because it gains us some performance. So the caller must make
 * sure the message flags are properly maintained. For all current callers,
 * this is always the case and without extra effort required.
 * rgerhards, 2009-11-17
 */
rsRetVal
msgSetFromSockinfo(msg_t *pThis, struct sockaddr_storage *sa){ 
	DEFiRet;
	assert(pThis->rcvFrom.pRcvFrom == NULL);

	CHKmalloc(pThis->rcvFrom.pfrominet = malloc(sizeof(struct sockaddr_storage)));
	memcpy(pThis->rcvFrom.pfrominet, sa, sizeof(struct sockaddr_storage));

finalize_it:
	RETiRet;
}


/* rgerhards 2008-09-10: set RcvFrom name in msg object. This calls AddRef()
 * on the property, because this must be done in all current cases and there
 * is no case expected where this may not be necessary.
 * rgerhards, 2009-06-30
 */
void MsgSetRcvFrom(msg_t *pThis, prop_t *new)
{
	prop.AddRef(new);
	MsgSetRcvFromWithoutAddRef(pThis, new);
}


/* This is used to set the property via a string. This function should not be
 * called if there is a reliable way for a caller to make sure that the
 * same name can be used across multiple messages. However, if it can not
 * ensure that, calling this function is the second best thing, because it
 * will re-use the previously created property if it contained the same
 * name (but it works only for the immediate previous).
 * rgerhards, 2009-06-31
 */
void MsgSetRcvFromStr(msg_t *pThis, uchar *psz, int len, prop_t **ppProp)
{
	assert(pThis != NULL);
	assert(ppProp != NULL);

	prop.CreateOrReuseStringProp(ppProp, psz, len);
	MsgSetRcvFrom(pThis, *ppProp);
}


/* set RcvFromIP name in msg object. This calls AddRef()
 * on the property, because this must be done in all current cases and there
 * is no case expected where this may not be necessary.
 * rgerhards, 2009-06-30
 */
rsRetVal MsgSetRcvFromIP(msg_t *pThis, prop_t *new)
{
	assert(pThis != NULL);

	BEGINfunc
	prop.AddRef(new);
	MsgSetRcvFromIPWithoutAddRef(pThis, new);
	ENDfunc
	return RS_RET_OK;
}


/* This is used to set the property via a string. This function should not be
 * called if there is a reliable way for a caller to make sure that the
 * same name can be used across multiple messages. However, if it can not
 * ensure that, calling this function is the second best thing, because it
 * will re-use the previously created property if it contained the same
 * name (but it works only for the immediate previous).
 * rgerhards, 2009-06-31
 */
rsRetVal MsgSetRcvFromIPStr(msg_t *pThis, uchar *psz, int len, prop_t **ppProp)
{
	DEFiRet;
	assert(pThis != NULL);

	CHKiRet(prop.CreateOrReuseStringProp(ppProp, psz, len));
	MsgSetRcvFromIP(pThis, *ppProp);

finalize_it:
	RETiRet;
}


/* rgerhards 2004-11-09: set HOSTNAME in msg object
 * rgerhards, 2007-06-21:
 * Does not return anything. If an error occurs, the hostname is
 * simply not set. I have changed this behaviour. The only problem
 * we can run into is memory shortage. If we have such, it is better
 * to loose the hostname than the full message. So we silently ignore
 * that problem and hope that memory will be available the next time
 * we need it. The rest of the code already knows how to handle an
 * unset HOSTNAME.
 */
void MsgSetHOSTNAME(msg_t *pThis, uchar* pszHOSTNAME, int lenHOSTNAME)
{
	assert(pThis != NULL);

	freeHOSTNAME(pThis);

	pThis->iLenHOSTNAME = lenHOSTNAME;
	if(pThis->iLenHOSTNAME < CONF_HOSTNAME_BUFSIZE) {
		/* small enough: use fixed buffer (faster!) */
		pThis->pszHOSTNAME = pThis->szHOSTNAME;
	} else if((pThis->pszHOSTNAME = (uchar*) MALLOC(pThis->iLenHOSTNAME + 1)) == NULL) {
		/* truncate message, better than completely loosing it... */
		pThis->pszHOSTNAME = pThis->szHOSTNAME;
		pThis->iLenHOSTNAME = CONF_HOSTNAME_BUFSIZE - 1;
	}

	memcpy(pThis->pszHOSTNAME, pszHOSTNAME, pThis->iLenHOSTNAME);
	pThis->pszHOSTNAME[pThis->iLenHOSTNAME] = '\0'; /* this also works with truncation! */
}


/* set the offset of the MSG part into the raw msg buffer
 * Note that the offset may be higher than the length of the raw message 
 * (exactly by one). This can happen if we have a message that does not 
 * contain any MSG part.
 */
void MsgSetMSGoffs(msg_t *pMsg, short offs)
{
	ISOBJ_TYPE_assert(pMsg, msg);
	pMsg->offMSG = offs;
	if(offs > pMsg->iLenRawMsg) {
		assert(offs - 1 == pMsg->iLenRawMsg);
		pMsg->iLenMSG = 0;
	} else {
		pMsg->iLenMSG = pMsg->iLenRawMsg - offs;
	}
}


/* replace the MSG part of a message. The update actually takes place inside
 * rawmsg. 
 * There are two cases: either the new message will be larger than the new msg
 * or it will be less than or equal. If it is less than or equal, we can utilize
 * the previous message buffer. If it is larger, we can utilize the msg_t-included
 * message buffer if it fits in there. If this is not the case, we need to alloc
 * a new, larger, chunk and copy over the data to it. Note that this function is
 * (hopefully) relatively seldom being called, so some performance impact is
 * uncritical. In any case, pszMSG is copied, so if it was dynamically allocated,
 * the caller is responsible for freeing it.
 * rgerhards, 2009-06-23
 */
rsRetVal MsgReplaceMSG(msg_t *pThis, uchar* pszMSG, int lenMSG)
{
	int lenNew;
	uchar *bufNew;
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, msg);
	assert(pszMSG != NULL);

	lenNew = pThis->iLenRawMsg + lenMSG - pThis->iLenMSG;
	if(lenMSG > pThis->iLenMSG && lenNew >= CONF_RAWMSG_BUFSIZE) {
		/*  we have lost our "bet" and need to alloc a new buffer ;) */
		CHKmalloc(bufNew = MALLOC(lenNew + 1));
		memcpy(bufNew, pThis->pszRawMsg, pThis->offMSG);
		if(pThis->pszRawMsg != pThis->szRawMsg)
			free(pThis->pszRawMsg);
		pThis->pszRawMsg = bufNew;
	}

	if(lenMSG > 0)
		memcpy(pThis->pszRawMsg + pThis->offMSG, pszMSG, lenMSG);
	pThis->pszRawMsg[lenNew] = '\0'; /* this also works with truncation! */
	pThis->iLenRawMsg = lenNew;
	pThis->iLenMSG = lenMSG;

finalize_it:
	RETiRet;
}

/* set raw message in message object. Size of message is provided.
 * The function makes sure that the stored rawmsg is properly
 * terminated by '\0'.
 * rgerhards, 2009-06-16
 */
void MsgSetRawMsg(msg_t *pThis, char* pszRawMsg, size_t lenMsg)
{
	assert(pThis != NULL);
	if(pThis->pszRawMsg != pThis->szRawMsg)
		free(pThis->pszRawMsg);

	pThis->iLenRawMsg = lenMsg;
	if(pThis->iLenRawMsg < CONF_RAWMSG_BUFSIZE) {
		/* small enough: use fixed buffer (faster!) */
		pThis->pszRawMsg = pThis->szRawMsg;
	} else if((pThis->pszRawMsg = (uchar*) MALLOC(pThis->iLenRawMsg + 1)) == NULL) {
		/* truncate message, better than completely loosing it... */
		pThis->pszRawMsg = pThis->szRawMsg;
		pThis->iLenRawMsg = CONF_RAWMSG_BUFSIZE - 1;
	}

	memcpy(pThis->pszRawMsg, pszRawMsg, pThis->iLenRawMsg);
	pThis->pszRawMsg[pThis->iLenRawMsg] = '\0'; /* this also works with truncation! */
}


/* set raw message in message object. Size of message is not provided. This
 * function should only be used when it is unavoidable (and over time we should
 * try to remove it altogether).
 * rgerhards, 2009-06-16
 */
void MsgSetRawMsgWOSize(msg_t *pMsg, char* pszRawMsg)
{
	MsgSetRawMsg(pMsg, pszRawMsg, strlen(pszRawMsg));
}


/* Decode a priority into textual information like auth.emerg.
 * The variable pRes must point to a user-supplied buffer.
 * The pointer to the buffer
 * is also returned, what makes this functiona suitable for
 * use in printf-like functions.
 * Note: a buffer size of 20 characters is always sufficient.
 */
char *textpri(char *pRes, int pri)
{
	assert(pRes != NULL);
	memcpy(pRes, syslog_fac_names[LOG_FAC(pri)], len_syslog_fac_names[LOG_FAC(pri)]);
	pRes[len_syslog_fac_names[LOG_FAC(pri)]] = '.';
	memcpy(pRes+len_syslog_fac_names[LOG_FAC(pri)]+1,
	       syslog_severity_names[LOG_PRI(pri)],
	       len_syslog_severity_names[LOG_PRI(pri)]+1 /* for \0! */);
	return pRes;
}


/* This function returns the current date in different
 * variants. It is used to construct the $NOW series of
 * system properties. The returned buffer must be freed
 * by the caller when no longer needed. If the function
 * can not allocate memory, it returns a NULL pointer.
 * Added 2007-07-10 rgerhards
 */
typedef enum ENOWType { NOW_NOW, NOW_YEAR, NOW_MONTH, NOW_DAY, NOW_HOUR, NOW_HHOUR, NOW_QHOUR, NOW_MINUTE } eNOWType;
#define tmpBUFSIZE 16	/* size of formatting buffer */
static uchar *getNOW(eNOWType eNow, struct syslogTime *t)
{
	uchar *pBuf;

	if((pBuf = (uchar*) MALLOC(sizeof(uchar) * tmpBUFSIZE)) == NULL) {
		return NULL;
	}

	if(t->year == 0) { /* not yet set! */
		datetime.getCurrTime(t, NULL);
	}

	switch(eNow) {
	case NOW_NOW:
		memcpy(pBuf, two_digits[t->year/100], 2);
		memcpy(pBuf+2, two_digits[t->year%100], 2);
		pBuf[4] = '-';
		memcpy(pBuf+5, two_digits[(int)t->month], 2);
		pBuf[7] = '-';
		memcpy(pBuf+8, two_digits[(int)t->day], 3);
		break;
	case NOW_YEAR:
		memcpy(pBuf, two_digits[t->year/100], 2);
		memcpy(pBuf+2, two_digits[t->year%100], 3);
		break;
	case NOW_MONTH:
		memcpy(pBuf, two_digits[(int)t->month], 3);
		break;
	case NOW_DAY:
		memcpy(pBuf, two_digits[(int)t->day], 3);
		break;
	case NOW_HOUR:
		memcpy(pBuf, two_digits[(int)t->hour], 3);
		break;
	case NOW_HHOUR:
		memcpy(pBuf, two_digits[t->hour/30], 3);
		break;
	case NOW_QHOUR:
		memcpy(pBuf, two_digits[t->hour/15], 3);
		break;
	case NOW_MINUTE:
		memcpy(pBuf, two_digits[(int)t->minute], 3);
		break;
	}

	return(pBuf);
}
#undef tmpBUFSIZE /* clean up */


/* Get a CEE-Property as string value*/
rsRetVal
getCEEPropVal(msg_t *pM, es_str_t *propName, uchar **pRes, rs_size_t *buflen, unsigned short *pbMustBeFreed)
{
	uchar *name = NULL;
	uchar *leaf;
	struct json_object *parent;
	struct json_object *field;
	DEFiRet;

	if(*pbMustBeFreed)
		free(*pRes);
	*pRes = NULL;
	// TODO: mutex?
	if(pM->json == NULL) goto finalize_it;

	if(!es_strbufcmp(propName, (uchar*)"!", 1)) {
		field = pM->json;
	} else {
		name = (uchar*)es_str2cstr(propName, NULL);
		leaf = jsonPathGetLeaf(name, ustrlen(name));
		CHKiRet(jsonPathFindParent(pM, name, leaf, &parent, 1));
		field = json_object_object_get(parent, (char*)leaf);
	}
	if(field != NULL) {
		*pRes = (uchar*) strdup(json_object_get_string(field));
		*buflen = (int) ustrlen(*pRes);
		*pbMustBeFreed = 1;
	}

finalize_it:
	free(name);
	if(*pRes == NULL) {
		/* could not find any value, so set it to empty */
		*pRes = (unsigned char*)"";
		*pbMustBeFreed = 0;
	}
	RETiRet;
}


/* Get a CEE-Property as native json object
 */
rsRetVal
msgGetCEEPropJSON(msg_t *pM, es_str_t *propName, struct json_object **pjson)
{
	uchar *name = NULL;
	uchar *leaf;
	struct json_object *parent;
	DEFiRet;

	// TODO: mutex?
	if(pM->json == NULL) {
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	if(!es_strbufcmp(propName, (uchar*)"!", 1)) {
		*pjson = pM->json;
		FINALIZE;
	}
	name = (uchar*)es_str2cstr(propName, NULL);
	leaf = jsonPathGetLeaf(name, ustrlen(name));
	CHKiRet(jsonPathFindParent(pM, name, leaf, &parent, 1));
	*pjson = json_object_object_get(parent, (char*)leaf);
	if(*pjson == NULL) {
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

finalize_it:
	free(name);
	RETiRet;
}


/* Encode a JSON value and add it to provided string. Note that 
 * the string object may be NULL. In this case, it is created
 * if and only if escaping is needed.
 */
static rsRetVal
jsonAddVal(uchar *pSrc, unsigned buflen, es_str_t **dst)
{
	unsigned char c;
	es_size_t i;
	char numbuf[4];
	int j;
	DEFiRet;

	for(i = 0 ; i < buflen ; ++i) {
		c = pSrc[i];
		if(   (c >= 0x23 && c <= 0x5b)
		   || (c >= 0x5d /* && c <= 0x10FFFF*/)
		   || c == 0x20 || c == 0x21) {
			/* no need to escape */
			if(*dst != NULL)
				es_addChar(dst, c);
		} else {
			if(*dst == NULL) {
				if(i == 0) {
					/* we hope we have only few escapes... */
					*dst = es_newStr(buflen+10);
				} else {
					*dst = es_newStrFromBuf((char*)pSrc, i);
				}
				if(*dst == NULL) {
					ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
				}
			}
			/* we must escape, try RFC4627-defined special sequences first */
			switch(c) {
			case '\0':
				es_addBuf(dst, "\\u0000", 6);
				break;
			case '\"':
				es_addBuf(dst, "\\\"", 2);
				break;
			case '/':
				es_addBuf(dst, "\\/", 2);
				break;
			case '\\':
				es_addBuf(dst, "\\\\", 2);
				break;
			case '\010':
				es_addBuf(dst, "\\b", 2);
				break;
			case '\014':
				es_addBuf(dst, "\\f", 2);
				break;
			case '\n':
				es_addBuf(dst, "\\n", 2);
				break;
			case '\r':
				es_addBuf(dst, "\\r", 2);
				break;
			case '\t':
				es_addBuf(dst, "\\t", 2);
				break;
			default:
				/* TODO : proper Unicode encoding (see header comment) */
				for(j = 0 ; j < 4 ; ++j) {
					numbuf[3-j] = hexdigit[c % 16];
					c = c / 16;
				}
				es_addBuf(dst, "\\u", 2);
				es_addBuf(dst, numbuf, 4);
				break;
			}
		}
	}
finalize_it:
	RETiRet;
}


/* encode a property in JSON escaped format. This is a helper
 * to MsgGetProp. It needs to update all provided parameters.
 * Note: Code is borrowed from libee (my own code, so ASL 2.0
 * is fine with it); this function may later be replaced by
 * some "better" and more complete implementation (maybe from
 * libee or its helpers).
 * For performance reasons, we begin to copy the string only
 * when we recognice that we actually need to do some escaping.
 * rgerhards, 2012-03-16
 */
static rsRetVal
jsonEncode(uchar **ppRes, unsigned short *pbMustBeFreed, int *pBufLen)
{
	unsigned buflen;
	uchar *pSrc;
	es_str_t *dst = NULL;
	DEFiRet;

	pSrc = *ppRes;
	buflen = (*pBufLen == -1) ? ustrlen(pSrc) : *pBufLen;
	CHKiRet(jsonAddVal(pSrc, buflen, &dst));

	if(dst != NULL) {
		/* we updated the string and need to replace the
		 * previous data.
		 */
		if(*pbMustBeFreed)
			free(*ppRes);
		*ppRes = (uchar*)es_str2cstr(dst, NULL);
		*pbMustBeFreed = 1;
		*pBufLen = -1;
		es_deleteStr(dst);
	}

finalize_it:
	RETiRet;
}


/* Format a property as JSON field, that means
 * "name"="value"
 * where value is JSON-escaped (here we assume that the name
 * only contains characters from the valid character set).
 * Note: this function duplicates code from jsonEncode(). 
 * TODO: these two functions should be combined, at least if
 * that makes any sense from a performance PoV - definitely
 * something to consider at a later stage. rgerhards, 2012-04-19
 */
static rsRetVal
jsonField(struct templateEntry *pTpe, uchar **ppRes, unsigned short *pbMustBeFreed, int *pBufLen)
{
	unsigned buflen;
	uchar *pSrc;
	es_str_t *dst = NULL;
	DEFiRet;

	pSrc = *ppRes;
	buflen = (*pBufLen == -1) ? ustrlen(pSrc) : *pBufLen;
	/* we hope we have only few escapes... */
	dst = es_newStr(buflen+pTpe->lenFieldName+15);
	es_addChar(&dst, '"');
	es_addBuf(&dst, (char*)pTpe->fieldName, pTpe->lenFieldName);
	es_addBufConstcstr(&dst, "\":\"");
	CHKiRet(jsonAddVal(pSrc, buflen, &dst));
	es_addChar(&dst, '"');

	if(*pbMustBeFreed)
		free(*ppRes);
	/* we know we do not have \0 chars - so the size does not change */
	*pBufLen = es_strlen(dst);
	*ppRes = (uchar*)es_str2cstr(dst, NULL);
	*pbMustBeFreed = 1;
	es_deleteStr(dst);

finalize_it:
	RETiRet;
}


/* This function returns a string-representation of the 
 * requested message property. This is a generic function used
 * to abstract properties so that these can be easier
 * queried. Returns NULL if property could not be found.
 * Actually, this function is a big if..elseif. What it does
 * is simply to map property names (from MonitorWare) to the
 * message object data fields.
 *
 * In case we need string forms of propertis we do not
 * yet have in string form, we do a memory allocation that
 * is sufficiently large (in all cases). Once the string
 * form has been obtained, it is saved until the Msg object
 * is finally destroyed. This is so that we save the processing
 * time in the (likely) case that this property is requested
 * again. It also saves us a lot of dynamic memory management
 * issues in the upper layers, because we so can guarantee that
 * the buffer will remain static AND available during the lifetime
 * of the object. Please note that both the max size allocation as
 * well as keeping things in memory might like look like a 
 * waste of memory (some might say it actually is...) - we
 * deliberately accept this because performance is more important
 * to us ;)
 * rgerhards 2004-11-18
 * Parameter "bMustBeFreed" is set by this function. It tells the
 * caller whether or not the string returned must be freed by the
 * caller itself. It is is 0, the caller MUST NOT free it. If it is
 * 1, the caller MUST free it. Handling this wrongly leads to either
 * a memory leak of a program abort (do to double-frees or frees on
 * the constant memory pool). So be careful to do it right.
 * rgerhards 2004-11-23
 * regular expression support contributed by Andres Riancho merged
 * on 2005-09-13
 * changed so that it now an be called without a template entry (NULL).
 * In this case, only the (unmodified) property is returned. This will
 * be used in selector line processing.
 * rgerhards 2005-09-15
 */
/* a quick helper to save some writing: */
#define RET_OUT_OF_MEMORY { *pbMustBeFreed = 0;\
	*pPropLen = sizeof("**OUT OF MEMORY**") - 1; \
	return(UCHAR_CONSTANT("**OUT OF MEMORY**"));}
uchar *MsgGetProp(msg_t *pMsg, struct templateEntry *pTpe,
                 propid_t propid, es_str_t *propName, rs_size_t *pPropLen,
		 unsigned short *pbMustBeFreed, struct syslogTime *ttNow)
{
	uchar *pRes; /* result pointer */
	rs_size_t bufLen = -1; /* length of string or -1, if not known */
	uchar *pBufStart;
	uchar *pBuf;
	int iLen;
	short iOffs;
	enum tplFormatTypes datefmt;

	BEGINfunc
	assert(pMsg != NULL);
	assert(pbMustBeFreed != NULL);

#ifdef	FEATURE_REGEXP
	/* Variables necessary for regular expression matching */
	size_t nmatch = 10;
	regmatch_t pmatch[10];
#endif

	*pbMustBeFreed = 0;

	switch(propid) {
		case PROP_MSG:
			pRes = getMSG(pMsg);
			bufLen = getMSGLen(pMsg);
			break;
		case PROP_TIMESTAMP:
			if (pTpe != NULL)
				datefmt = pTpe->data.field.eDateFormat;
			else
				datefmt = tplFmtDefault;
			pRes = (uchar*)getTimeReported(pMsg, datefmt);
			break;
		case PROP_HOSTNAME:
			pRes = (uchar*)getHOSTNAME(pMsg);
			bufLen = getHOSTNAMELen(pMsg);
			break;
		case PROP_SYSLOGTAG:
			getTAG(pMsg, &pRes, &bufLen);
			break;
		case PROP_RAWMSG:
			getRawMsg(pMsg, &pRes, &bufLen);
			break;
		case PROP_INPUTNAME:
			getInputName(pMsg, &pRes, &bufLen);
			break;
		case PROP_FROMHOST:
			pRes = getRcvFrom(pMsg);
			break;
		case PROP_FROMHOST_IP:
			pRes = getRcvFromIP(pMsg);
			break;
		case PROP_PRI:
			pRes = (uchar*)getPRI(pMsg);
			break;
		case PROP_PRI_TEXT:
			pBuf = MALLOC(20 * sizeof(uchar));
			if(pBuf == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				pRes = (uchar*)textpri((char*)pBuf, getPRIi(pMsg));
			}
			break;
		case PROP_IUT:
			pRes = UCHAR_CONSTANT("1"); /* always 1 for syslog messages (a MonitorWare thing;)) */
			bufLen = 1;
			break;
		case PROP_SYSLOGFACILITY:
			pRes = (uchar*)getFacility(pMsg);
			break;
		case PROP_SYSLOGFACILITY_TEXT:
			pRes = (uchar*)getFacilityStr(pMsg);
			break;
		case PROP_SYSLOGSEVERITY:
			pRes = (uchar*)getSeverity(pMsg);
			break;
		case PROP_SYSLOGSEVERITY_TEXT:
			pRes = (uchar*)getSeverityStr(pMsg);
			break;
		case PROP_TIMEGENERATED:
			if (pTpe != NULL)
				datefmt = pTpe->data.field.eDateFormat;
			else
				datefmt = tplFmtDefault;
			pRes = (uchar*)getTimeGenerated(pMsg, datefmt);
			break;
		case PROP_PROGRAMNAME:
			pRes = getProgramName(pMsg, LOCK_MUTEX);
			break;
		case PROP_PROTOCOL_VERSION:
			pRes = (uchar*)getProtocolVersionString(pMsg);
			break;
		case PROP_STRUCTURED_DATA:
			pRes = (uchar*)getStructuredData(pMsg);
			break;
		case PROP_APP_NAME:
			pRes = (uchar*)getAPPNAME(pMsg, LOCK_MUTEX);
			break;
		case PROP_PROCID:
			pRes = (uchar*)getPROCID(pMsg, LOCK_MUTEX);
			break;
		case PROP_MSGID:
			pRes = (uchar*)getMSGID(pMsg);
			break;
#ifdef USE_LIBUUID
		case PROP_UUID:
			getUUID(pMsg, &pRes, &bufLen);
			break;
#endif
		case PROP_PARSESUCCESS:
			pRes = (uchar*)getParseSuccess(pMsg);
			break;
		case PROP_SYS_NOW:
			if((pRes = getNOW(NOW_NOW, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 10;
			}
			break;
		case PROP_SYS_YEAR:
			if((pRes = getNOW(NOW_YEAR, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 4;
			}
			break;
		case PROP_SYS_MONTH:
			if((pRes = getNOW(NOW_MONTH, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_DAY:
			if((pRes = getNOW(NOW_DAY, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_HOUR:
			if((pRes = getNOW(NOW_HOUR, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_HHOUR:
			if((pRes = getNOW(NOW_HHOUR, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_QHOUR:
			if((pRes = getNOW(NOW_QHOUR, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_MINUTE:
			if((pRes = getNOW(NOW_MINUTE, ttNow)) == NULL) {
				RET_OUT_OF_MEMORY;
			} else {
				*pbMustBeFreed = 1;
				bufLen = 2;
			}
			break;
		case PROP_SYS_MYHOSTNAME:
			pRes = glbl.GetLocalHostName();
			break;
		case PROP_CEE_ALL_JSON:
			if(pMsg->json == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = (uchar*) "{}";
				bufLen = 2;
				*pbMustBeFreed = 0;
			} else {
				pRes = (uchar*)strdup(json_object_get_string(pMsg->json));
				*pbMustBeFreed = 1;
			}
			break;
		case PROP_CEE:
			getCEEPropVal(pMsg, propName, &pRes, &bufLen, pbMustBeFreed);
			break;
		case PROP_SYS_BOM:
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = (uchar*) "\xEF\xBB\xBF";
			*pbMustBeFreed = 0;
			break;
		case PROP_SYS_UPTIME:
#			ifndef HAVE_SYSINFO_UPTIME
            /* An alternative on some systems (eg Solaris) is to scan
             * /var/adm/utmpx for last boot time.
             */
			pRes = (uchar*) "UPTIME NOT available on this system";
			*pbMustBeFreed = 0;
#			else
			{
			struct sysinfo s_info;

			if((pRes = (uchar*) MALLOC(sizeof(uchar) * 32)) == NULL) {
				RET_OUT_OF_MEMORY;
			}
			*pbMustBeFreed = 1;

			if(sysinfo(&s_info) < 0) {
				*pPropLen = sizeof("**SYSCALL FAILED**") - 1;
				return(UCHAR_CONSTANT("**SYSCALL FAILED**"));
			}

			snprintf((char*) pRes, sizeof(uchar) * 32, "%ld", s_info.uptime);
			}
#			endif
		break;
		default:
			/* there is no point in continuing, we may even otherwise render the
			 * error message unreadable. rgerhards, 2007-07-10
			 */
			dbgprintf("invalid property id: '%d'\n", propid);
			*pbMustBeFreed = 0;
			*pPropLen = sizeof("**INVALID PROPERTY NAME**") - 1;
			return UCHAR_CONSTANT("**INVALID PROPERTY NAME**");
	}

	/* If we did not receive a template pointer, we are already done... */
	if(pTpe == NULL || !pTpe->bComplexProcessing) {
		*pPropLen = (bufLen == -1) ? ustrlen(pRes) : bufLen;
		return pRes;
	}
	
	/* Now check if we need to make "temporary" transformations (these
	 * are transformations that do not go back into the message -
	 * memory must be allocated for them!).
	 */
	
	/* substring extraction */
	/* first we check if we need to extract by field number
	 * rgerhards, 2005-12-22
	 */
	if(pTpe->data.field.has_fields == 1) {
		size_t iCurrFld;
		uchar *pFld;
		uchar *pFldEnd;
		/* first, skip to the field in question. The field separator
		 * is always one character and is stored in the template entry.
		 */
		iCurrFld = 1;
		pFld = pRes;
		while(*pFld && iCurrFld < pTpe->data.field.iFieldNr) {
			/* skip fields until the requested field or end of string is found */
			while(*pFld && (uchar) *pFld != pTpe->data.field.field_delim)
				++pFld; /* skip to field terminator */
			if(*pFld == pTpe->data.field.field_delim) {
				++pFld; /* eat it */
#ifdef STRICT_GPLV3
				if (pTpe->data.field.field_expand != 0) {
					while (*pFld == pTpe->data.field.field_delim) {
						++pFld;
					}
				}
#endif
				++iCurrFld;
			}
		}
		dbgprintf("field requested %d, field found %d\n", pTpe->data.field.iFieldNr, (int) iCurrFld);
		
		if(iCurrFld == pTpe->data.field.iFieldNr) {
			/* field found, now extract it */
			/* first of all, we need to find the end */
			pFldEnd = pFld;
			while(*pFldEnd && *pFldEnd != pTpe->data.field.field_delim)
				++pFldEnd;
			--pFldEnd; /* we are already at the delimiter - so we need to
			            * step back a little not to copy it as part of the field. */
			/* we got our end pointer, now do the copy */
			/* TODO: code copied from below, this is a candidate for a separate function */
			iLen = pFldEnd - pFld + 1; /* the +1 is for an actual char, NOT \0! */
			pBufStart = pBuf = MALLOC((iLen + 1) * sizeof(char));
			if(pBuf == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				RET_OUT_OF_MEMORY;
			}
			/* now copy */
			memcpy(pBuf, pFld, iLen);
			bufLen = iLen;
			pBuf[iLen] = '\0'; /* terminate it */
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = pBufStart;
			*pbMustBeFreed = 1;
			if(*(pFldEnd+1) != '\0')
				++pFldEnd; /* OK, skip again over delimiter char */
		} else {
			/* field not found, return error */
			if(*pbMustBeFreed == 1)
				free(pRes);
			*pbMustBeFreed = 0;
			*pPropLen = sizeof("**FIELD NOT FOUND**") - 1;
			return UCHAR_CONSTANT("**FIELD NOT FOUND**");
		}
#ifdef FEATURE_REGEXP
	} else {
		/* Check for regular expressions */
		if (pTpe->data.field.has_regex != 0) {
			if (pTpe->data.field.has_regex == 2) {
				/* Could not compile regex before! */
				if (*pbMustBeFreed == 1) {
					free(pRes);
					*pbMustBeFreed = 0;
				}
				*pPropLen = sizeof("**NO MATCH** **BAD REGULAR EXPRESSION**") - 1;
				return UCHAR_CONSTANT("**NO MATCH** **BAD REGULAR EXPRESSION**");
			}

			dbgprintf("string to match for regex is: %s\n", pRes);

			if(objUse(regexp, LM_REGEXP_FILENAME) == RS_RET_OK) {
				short iTry = 0;
				uchar bFound = 0;
				iOffs = 0;
				/* first see if we find a match, iterating through the series of
				 * potential matches over the string.
				 */
				while(!bFound) {
					int iREstat;
					iREstat = regexp.regexec(&pTpe->data.field.re, (char*)(pRes + iOffs), nmatch, pmatch, 0);
					dbgprintf("regexec return is %d\n", iREstat);
					if(iREstat == 0) {
						if(pmatch[0].rm_so == -1) {
							dbgprintf("oops ... start offset of successful regexec is -1\n");
							break;
						}
						if(iTry == pTpe->data.field.iMatchToUse) {
							bFound = 1;
						} else {
							dbgprintf("regex found at offset %d, new offset %d, tries %d\n",
								  iOffs, (int) (iOffs + pmatch[0].rm_eo), iTry);
							iOffs += pmatch[0].rm_eo;
							++iTry;
						}
					} else {
						break;
					}
				}
				dbgprintf("regex: end search, found %d\n", bFound);
				if(!bFound) {
					/* we got no match! */
					if(pTpe->data.field.nomatchAction != TPL_REGEX_NOMATCH_USE_WHOLE_FIELD) {
						if (*pbMustBeFreed == 1) {
							free(pRes);
							*pbMustBeFreed = 0;
						}
						if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_DFLTSTR) {
							bufLen = sizeof("**NO MATCH**") - 1;
							pRes = UCHAR_CONSTANT("**NO MATCH**");
						} else if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_ZERO) {
							bufLen = 1;
							pRes = UCHAR_CONSTANT("0");
						} else {
							bufLen = 0;
							pRes = UCHAR_CONSTANT("");
						}
					}
				} else {
					/* Match- but did it match the one we wanted? */
					/* we got no match! */
					if(pmatch[pTpe->data.field.iSubMatchToUse].rm_so == -1) {
						if(pTpe->data.field.nomatchAction != TPL_REGEX_NOMATCH_USE_WHOLE_FIELD) {
							if (*pbMustBeFreed == 1) {
								free(pRes);
								*pbMustBeFreed = 0;
							}
							if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_DFLTSTR) {
								bufLen = sizeof("**NO MATCH**") - 1;
								pRes = UCHAR_CONSTANT("**NO MATCH**");
							} else if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_ZERO) {
								bufLen = 1;
								pRes = UCHAR_CONSTANT("0");
							} else {
								bufLen = 0;
								pRes = UCHAR_CONSTANT("");
							}
						}
					}
					/* OK, we have a usable match - we now need to malloc pB */
					int iLenBuf;
					uchar *pB;

					iLenBuf = pmatch[pTpe->data.field.iSubMatchToUse].rm_eo
						  - pmatch[pTpe->data.field.iSubMatchToUse].rm_so;
					pB = MALLOC((iLenBuf + 1) * sizeof(uchar));

					if (pB == NULL) {
						if (*pbMustBeFreed == 1)
							free(pRes);
						RET_OUT_OF_MEMORY;
					}

					/* Lets copy the matched substring to the buffer */
					memcpy(pB, pRes + iOffs +  pmatch[pTpe->data.field.iSubMatchToUse].rm_so, iLenBuf);
					bufLen = iLenBuf;
					pB[iLenBuf] = '\0';/* terminate string, did not happen before */

					if (*pbMustBeFreed == 1)
						free(pRes);
					pRes = pB;
					*pbMustBeFreed = 1;
				}
			} else {
				/* we could not load regular expression support. This is quite unexpected at
				 * this stage of processing (after all, the config parser found it), but so
				 * it is. We return an error in that case. -- rgerhards, 2008-03-07
				 */
				dbgprintf("could not get regexp object pointer, so regexp can not be evaluated\n");
				if (*pbMustBeFreed == 1) {
					free(pRes);
					*pbMustBeFreed = 0;
				}
				*pPropLen = sizeof("***REGEXP NOT AVAILABLE***") - 1;
				return UCHAR_CONSTANT("***REGEXP NOT AVAILABLE***");
			}
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	if(pTpe->data.field.iFromPos != 0 || pTpe->data.field.iToPos != 0) {
		/* we need to obtain a private copy */
		int iFrom, iTo;
		uchar *pSb;
		iFrom = pTpe->data.field.iFromPos;
		iTo = pTpe->data.field.iToPos;
		if(bufLen == -1)
			bufLen = ustrlen(pRes);
		if(pTpe->data.field.options.bFromPosEndRelative) {
			iFrom = (bufLen < iFrom) ? 0 : bufLen - iFrom;
			iTo = (bufLen < iTo)? 0 : bufLen - iTo;
		} else {
			/* need to zero-base to and from (they are 1-based!) */
			if(iFrom > 0)
				--iFrom;
			if(iTo > 0)
				--iTo;
		}
		if(iFrom == 0 && iTo >=  bufLen) { 
			/* in this case, the requested string is a superset of what we already have,
			 * so there is no need to do any processing. This is a frequent case for size-limited
			 * fields like TAG in the default forwarding template (so it is a useful optimization
			 * to check for this condition ;)). -- rgerhards, 2009-07-09
			 */
			; /*DO NOTHING*/
		} else {
			if(iTo > bufLen) /* iTo is very large, if no to-position is set in the template! */
				iTo = bufLen;
			iLen = iTo - iFrom + 1; /* the +1 is for an actual char, NOT \0! */
			pBufStart = pBuf = MALLOC((iLen + 1) * sizeof(char));
			if(pBuf == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				RET_OUT_OF_MEMORY;
			}
			pSb = pRes;
			if(iFrom) {
			/* skip to the start of the substring (can't do pointer arithmetic
			 * because the whole string might be smaller!!)
			 */
				while(*pSb && iFrom) {
					--iFrom;
					++pSb;
				}
			}
			/* OK, we are at the begin - now let's copy... */
			bufLen = iLen;
			while(*pSb && iLen) {
				*pBuf++ = *pSb;
				++pSb;
				--iLen;
			}
			*pBuf = '\0';
			bufLen -= iLen; /* subtract remaining length if the string was smaller! */
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = pBufStart;
			*pbMustBeFreed = 1;
		}
	}

	/* now check if we need to do our "SP if first char is non-space" hack logic */
	if(*pRes && pTpe->data.field.options.bSPIffNo1stSP) {
		/* here, we always destruct the buffer and return a new one */
		uchar cFirst = *pRes; /* save first char */
		if(*pbMustBeFreed == 1)
			free(pRes);
		pRes = (cFirst == ' ') ? UCHAR_CONSTANT("") : UCHAR_CONSTANT(" ");
		bufLen = (cFirst == ' ') ? 0 : 1;
		*pbMustBeFreed = 0;
	}

	if(*pRes) {
		/* case conversations (should go after substring, because so we are able to
		 * work on the smallest possible buffer).
		 */
		if(pTpe->data.field.eCaseConv != tplCaseConvNo) {
			/* we need to obtain a private copy */
			if(bufLen == -1)
				bufLen = ustrlen(pRes);
			uchar *pBStart;
			uchar *pB;
			uchar *pSrc;
			pBStart = pB = MALLOC((bufLen + 1) * sizeof(char));
			if(pB == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				RET_OUT_OF_MEMORY;
			}
			pSrc = pRes;
			while(*pSrc) {
				*pB++ = (pTpe->data.field.eCaseConv == tplCaseConvUpper) ?
					(uchar)toupper((int)*pSrc) : (uchar)tolower((int)*pSrc);
				/* currently only these two exist */
				++pSrc;
			}
			*pB = '\0';
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = pBStart;
			*pbMustBeFreed = 1;
		}

		/* now do control character dropping/escaping/replacement
		 * Only one of these can be used. If multiple options are given, the
		 * result is random (though currently there obviously is an order of
		 * preferrence, see code below. But this is NOT guaranteed.
		 * RGerhards, 2006-11-17
		 * We must copy the strings if we modify them, because they may either 
		 * point to static memory or may point into the message object, in which
		 * case we would actually modify the original property (which of course
		 * is wrong).
		 * This was found and fixed by varmojefkoj on 2007-09-11
		 */
		if(pTpe->data.field.options.bDropCC) {
			int iLenBuf = 0;
			uchar *pSrc = pRes;
			uchar *pDstStart;
			uchar *pDst;
			uchar bDropped = 0;
			
			while(*pSrc) {
				if(!iscntrl((int) *pSrc++))
					iLenBuf++;
				else
					bDropped = 1;
			}

			if(bDropped) {
				pDst = pDstStart = MALLOC(iLenBuf + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					RET_OUT_OF_MEMORY;
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(!iscntrl((int) *pSrc))
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = pDstStart;
				bufLen = iLenBuf;
				*pbMustBeFreed = 1;
			}
		} else if(pTpe->data.field.options.bSpaceCC) {
			uchar *pSrc;
			uchar *pDstStart;
			uchar *pDst;
			
			if(*pbMustBeFreed == 1) {
				/* in this case, we already work on dynamic
				 * memory, so there is no need to copy it - we can
				 * modify it in-place without any harm. This is a
				 * performance optiomization.
				 */
				for(pDst = pRes; *pDst; pDst++) {
					if(iscntrl((int) *pDst))
						*pDst = ' ';
				}
			} else {
				if(bufLen == -1)
					bufLen = ustrlen(pRes);
				pDst = pDstStart = MALLOC(bufLen + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					RET_OUT_OF_MEMORY;
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(iscntrl((int) *pSrc))
						*pDst++ = ' ';
					else
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				pRes = pDstStart;
				*pbMustBeFreed = 1;
			}
		} else if(pTpe->data.field.options.bEscapeCC) {
			/* we must first count how many control charactes are
			 * present, because we need this to compute the new string
			 * buffer length. While doing so, we also compute the string
			 * length.
			 */
			int iNumCC = 0;
			int iLenBuf = 0;
			uchar *pB;

			for(pB = pRes ; *pB ; ++pB) {
				++iLenBuf;
				if(iscntrl((int) *pB))
					++iNumCC;
			}

			if(iNumCC > 0) { /* if 0, there is nothing to escape, so we are done */
				/* OK, let's do the escaping... */
				uchar *pBStart;
				uchar szCCEsc[8]; /* buffer for escape sequence */
				int i;

				iLenBuf += iNumCC * 4;
				pBStart = pB = MALLOC((iLenBuf + 1) * sizeof(uchar));
				if(pB == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					RET_OUT_OF_MEMORY;
				}
				while(*pRes) {
					if(iscntrl((int) *pRes)) {
						snprintf((char*)szCCEsc, sizeof(szCCEsc), "#%3.3d", *pRes);
						for(i = 0 ; i < 4 ; ++i)
							*pB++ = szCCEsc[i];
					} else {
						*pB++ = *pRes;
					}
					++pRes;
				}
				*pB = '\0';
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = pBStart;
				bufLen = -1;
				*pbMustBeFreed = 1;
			}
		}
	}

	/* Take care of spurious characters to make the property safe
	 * for a path definition
	 */
	if(pTpe->data.field.options.bSecPathDrop || pTpe->data.field.options.bSecPathReplace) {
		if(pTpe->data.field.options.bSecPathDrop) {
			int iLenBuf = 0;
			uchar *pSrc = pRes;
			uchar *pDstStart;
			uchar *pDst;
			uchar bDropped = 0;
			
			while(*pSrc) {
				if(*pSrc++ != '/')
					iLenBuf++;
				else
					bDropped = 1;
			}
			
			if(bDropped) {
				pDst = pDstStart = MALLOC(iLenBuf + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					RET_OUT_OF_MEMORY;
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(*pSrc != '/')
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = pDstStart;
				bufLen = -1; /* TODO: can we do better? */
				*pbMustBeFreed = 1;
			}
		} else {
			uchar *pSrc;
			uchar *pDstStart;
			uchar *pDst;
			
			if(*pbMustBeFreed == 1) {
				/* here, again, we can modify the string as we already obtained
				 * a private buffer. As we do not change the size of that buffer,
				 * in-place modification is possible. This is a performance
				 * enhancement.
				 */
				for(pDst = pRes; *pDst; pDst++) {
					if(*pDst == '/')
						*pDst++ = '_';
				}
			} else {
				if(bufLen == -1)
					bufLen = ustrlen(pRes);
				pDst = pDstStart = MALLOC(bufLen + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					RET_OUT_OF_MEMORY;
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(*pSrc == '/')
						*pDst++ = '_';
					else
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				/* we must NOT check if it needs to be freed, because we have done
				 * this in the if above. So if we come to hear, the pSrc string needs
				 * not to be freed (and we do not need to care about it).
				 */
				pRes = pDstStart;
				*pbMustBeFreed = 1;
			}
		}
		
		/* check for "." and ".." (note the parenthesis in the if condition!) */
		if(*pRes == '\0') {
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = UCHAR_CONSTANT("_");
			bufLen = 1;
			*pbMustBeFreed = 0;
		} else if((*pRes == '.') && (*(pRes + 1) == '\0' || (*(pRes + 1) == '.' && *(pRes + 2) == '\0'))) {
			uchar *pTmp = pRes;

			if(*(pRes + 1) == '\0')
				pRes = UCHAR_CONSTANT("_");
			else
				pRes = UCHAR_CONSTANT("_.");;
			if(*pbMustBeFreed == 1)
				free(pTmp);
			*pbMustBeFreed = 0;
		}
	}

	/* Now drop last LF if present (pls note that this must not be done
	 * if bEscapeCC was set)!
	 */
	if(pTpe->data.field.options.bDropLastLF && !pTpe->data.field.options.bEscapeCC) {
		int iLn;
		uchar *pB;
		if(bufLen == -1)
			bufLen = ustrlen(pRes);
		iLn = bufLen;
		if(iLn > 0 && *(pRes + iLn - 1) == '\n') {
			/* we have a LF! */
			/* check if we need to obtain a private copy */
			if(*pbMustBeFreed == 0) {
				/* ok, original copy, need a private one */
				pB = MALLOC((iLn + 1) * sizeof(uchar));
				if(pB == NULL) {
					RET_OUT_OF_MEMORY;
				}
				memcpy(pB, pRes, iLn - 1);
				pRes = pB;
				*pbMustBeFreed = 1;
			}
			*(pRes + iLn - 1) = '\0'; /* drop LF ;) */
			--bufLen;
		}
	}

	/* finally, we need to check if the property should be formatted in CSV or JSON.
	 * For CSV we use RFC 4180, and always use double quotes. As of this writing,
	 * this should be the last action carried out on the property, but in the
	 * future there may be reasons to change that. -- rgerhards, 2009-04-02
	 */
	if(pTpe->data.field.options.bCSV) {
		/* we need to obtain a private copy, as we need to at least add the double quotes */
		int iBufLen;
		uchar *pBStart;
		uchar *pDst;
		uchar *pSrc;
		if(bufLen == -1)
			bufLen = ustrlen(pRes);
		iBufLen = bufLen;
		/* the malloc may be optimized, we currently use the worst case... */
		pBStart = pDst = MALLOC((2 * iBufLen + 3) * sizeof(uchar));
		if(pDst == NULL) {
			if(*pbMustBeFreed == 1)
				free(pRes);
			RET_OUT_OF_MEMORY;
		}
		pSrc = pRes;
		*pDst++ = '"'; /* starting quote */
		while(*pSrc) {
			if(*pSrc == '"')
				*pDst++ = '"'; /* need to add double double quote (see RFC4180) */
			*pDst++ = *pSrc++;
		}
		*pDst++ = '"';	/* ending quote */
		*pDst = '\0';
		if(*pbMustBeFreed == 1)
			free(pRes);
		pRes = pBStart;
		bufLen = -1;
		*pbMustBeFreed = 1;
	} else if(pTpe->data.field.options.bJSON) {
		jsonEncode(&pRes, pbMustBeFreed, &bufLen);
	} else if(pTpe->data.field.options.bJSONf) {
		jsonField(pTpe, &pRes, pbMustBeFreed, &bufLen);
	}

	*pPropLen = (bufLen == -1) ? ustrlen(pRes) : bufLen;

	ENDfunc
	return(pRes);
}


/* The function returns a cee variable suitable for use with RainerScript. 
 * Note: caller must free the returned string.
 * Note that we need to do a lot of conversions between es_str_t and cstr -- this will go away once
 * we have moved larger parts of rsyslog to es_str_t. Acceptable for the moment, especially as we intend
 * to rewrite the script engine as well!
 * rgerhards, 2010-12-03
 */
es_str_t*
msgGetCEEVarNew(msg_t *pMsg, char *name)
{
	uchar *leaf;
	char *val;
	es_str_t *estr = NULL;
	struct json_object *json, *parent;

	ISOBJ_TYPE_assert(pMsg, msg);

	if(pMsg->json == NULL) {
		estr = es_newStr(1);
		goto done;
	}
	leaf = jsonPathGetLeaf((uchar*)name, strlen(name));
	if(jsonPathFindParent(pMsg, (uchar*)name, leaf, &parent, 1) != RS_RET_OK) {
		estr = es_newStr(1);
		goto done;
	}
	json = json_object_object_get(parent, (char*)leaf);
	val = (char*)json_object_get_string(json);
	estr = es_newStrFromCStr(val, strlen(val));
done:
	return estr;
}


/* Return an es_str_t for given message property.
 */
es_str_t*
msgGetMsgVarNew(msg_t *pThis, uchar *name)
{
	rs_size_t propLen;
	uchar *pszProp = NULL;
	propid_t propid;
	unsigned short bMustBeFreed = 0;
	es_str_t *estr;

	ISOBJ_TYPE_assert(pThis, msg);

	/* always call MsgGetProp() without a template specifier */
	/* TODO: optimize propNameToID() call -- rgerhards, 2009-06-26 */
	propNameStrToID(name, &propid);
	pszProp = (uchar*) MsgGetProp(pThis, NULL, propid, NULL, &propLen, &bMustBeFreed, NULL);

	estr = es_newStrFromCStr((char*)pszProp, propLen);
	if(bMustBeFreed)
		free(pszProp);

	return estr;
}


/* This function can be used as a generic way to set properties.
 * We have to handle a lot of legacy, so our return value is not always
 * 100% correct (called functions do not always provide one, should
 * change over time).
 * rgerhards, 2008-01-07
 */
#define isProp(name) !rsCStrSzStrCmp(pProp->pcsName, (uchar*) name, sizeof(name) - 1)
rsRetVal MsgSetProperty(msg_t *pThis, var_t *pProp)
{
	prop_t *myProp;
	prop_t *propRcvFrom = NULL;
	prop_t *propRcvFromIP = NULL;
	struct json_tokener *tokener;
	struct json_object *json;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, msg);
	assert(pProp != NULL);

 	if(isProp("iProtocolVersion")) {
		setProtocolVersion(pThis, pProp->val.num);
 	} else if(isProp("iSeverity")) {
		pThis->iSeverity = pProp->val.num;
 	} else if(isProp("iFacility")) {
		pThis->iFacility = pProp->val.num;
 	} else if(isProp("msgFlags")) {
		pThis->msgFlags = pProp->val.num;
 	} else if(isProp("offMSG")) {
		MsgSetMSGoffs(pThis, pProp->val.num);
	} else if(isProp("pszRawMsg")) {
		MsgSetRawMsg(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr), cstrLen(pProp->val.pStr));
	} else if(isProp("pszUxTradMsg")) {
		/*IGNORE*/; /* this *was* a property, but does no longer exist */
	} else if(isProp("pszTAG")) {
		MsgSetTAG(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), cstrLen(pProp->val.pStr));
	} else if(isProp("pszInputName")) {
		/* we need to create a property */ 
		CHKiRet(prop.Construct(&myProp));
		CHKiRet(prop.SetString(myProp, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr)));
		CHKiRet(prop.ConstructFinalize(myProp));
		MsgSetInputName(pThis, myProp);
		prop.Destruct(&myProp);
	} else if(isProp("pszRcvFromIP")) {
		MsgSetRcvFromIPStr(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr), &propRcvFromIP);
		prop.Destruct(&propRcvFromIP);
	} else if(isProp("pszRcvFrom")) {
		MsgSetRcvFromStr(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr), &propRcvFrom);
		prop.Destruct(&propRcvFrom);
	} else if(isProp("pszHOSTNAME")) {
		MsgSetHOSTNAME(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr));
	} else if(isProp("pCSStrucData")) {
		MsgSetStructuredData(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pCSAPPNAME")) {
		MsgSetAPPNAME(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pCSPROCID")) {
		MsgSetPROCID(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pCSMSGID")) {
		MsgSetMSGID(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr));
 	} else if(isProp("ttGenTime")) {
		pThis->ttGenTime = pProp->val.num;
	} else if(isProp("tRcvdAt")) {
		memcpy(&pThis->tRcvdAt, &pProp->val.vSyslogTime, sizeof(struct syslogTime));
	} else if(isProp("tTIMESTAMP")) {
		memcpy(&pThis->tTIMESTAMP, &pProp->val.vSyslogTime, sizeof(struct syslogTime));
	} else if(isProp("pszRuleset")) {
		MsgSetRulesetByName(pThis, pProp->val.pStr);
	} else if(isProp("pszMSG")) {
		dbgprintf("no longer supported property pszMSG silently ignored\n");
	} else if(isProp("json")) {
		tokener = json_tokener_new();
		json = json_tokener_parse_ex(tokener, (char*)rsCStrGetSzStrNoNULL(pProp->val.pStr),
					     cstrLen(pProp->val.pStr));
		json_tokener_free(tokener);
		msgAddJSON(pThis, (uchar*)"!", json);
	} else {
		dbgprintf("unknown supported property '%s' silently ignored\n",
			  rsCStrGetSzStrNoNULL(pProp->pcsName));
	}

finalize_it:
	RETiRet;
}
#undef	isProp


/* get the severity - this is an entry point that
 * satisfies the base object class getSeverity semantics.
 * rgerhards, 2008-01-14
 */
rsRetVal
MsgGetSeverity(msg_t *pMsg, int *piSeverity)
{
	*piSeverity = pMsg->iSeverity;
	return RS_RET_OK;
}


static uchar *
jsonPathGetLeaf(uchar *name, int lenName)
{
	int i;
	for(i = lenName ; name[i] != '!' && i >= 0 ; --i)
		/* just skip */;
	if(name[i] == '!')
		++i;
	return name + i;
}


static rsRetVal
jsonPathFindNext(struct json_object *root, uchar **name, uchar *leaf,
		 struct json_object **found, int bCreate)
{
	uchar namebuf[1024];
	struct json_object *json;
	size_t i;
	uchar *p = *name;
	DEFiRet;

	if(*p == '!')
		++p;
	for(i = 0 ; *p && *p != '!' && p != leaf && i < sizeof(namebuf)-1 ; ++i, ++p)
		namebuf[i] = *p;
	if(i > 0) {
		namebuf[i] = '\0';
		dbgprintf("AAAA: next JSONPath elt: '%s'\n", namebuf);
		json = json_object_object_get(root, (char*)namebuf);
	} else
		json = root;
	if(json == NULL) {
		if(!bCreate) {
			ABORT_FINALIZE(RS_RET_JNAME_INVALID);
		} else {
			json = json_object_new_object();
			json_object_object_add(root, (char*)namebuf, json);
		}
	}

	*name = p;
	*found = json;
finalize_it:
	RETiRet;
}

static rsRetVal
jsonPathFindParent(msg_t *pM, uchar *name, uchar *leaf, struct json_object **parent, int bCreate)
{
	DEFiRet;
	*parent = pM->json;
	while(name < leaf-1) {
		jsonPathFindNext(*parent, &name, leaf, parent, bCreate);
	}
	RETiRet;
}

static rsRetVal
jsonMerge(struct json_object *existing, struct json_object *json)
{
	/* TODO: check & handle duplicate names */
	DEFiRet;
	struct json_object_iter it;

	json_object_object_foreachC(json, it) {
DBGPRINTF("AAAA jsonMerge adds '%s'\n", it.key);
		json_object_object_add(existing, it.key,
			json_object_get(it.val));
	}
	/* note: json-c does ref counting. We added all descandants refcounts
	 * in the loop above. So when we now free(_put) the root object, only
	 * root gets freed().
	 */
	json_object_put(json);
	RETiRet;
}

/* find a JSON structure element (field or container doesn't matter).  */
rsRetVal
jsonFind(msg_t *pM, es_str_t *propName, struct json_object **jsonres)
{
	uchar *name = NULL;
	uchar *leaf;
	struct json_object *parent;
	struct json_object *field;
	DEFiRet;

	if(pM->json == NULL) {
		field = NULL;
		goto finalize_it;
	}

	if(!es_strbufcmp(propName, (uchar*)"!", 1)) {
		field = pM->json;
	} else {
		name = (uchar*)es_str2cstr(propName, NULL);
		leaf = jsonPathGetLeaf(name, ustrlen(name));
		CHKiRet(jsonPathFindParent(pM, name, leaf, &parent, 0));
		field = json_object_object_get(parent, (char*)leaf);
	}
	*jsonres = field;

finalize_it:
	free(name);
	RETiRet;
}

rsRetVal
msgAddJSON(msg_t *pM, uchar *name, struct json_object *json)
{
	/* TODO: error checks! This is a quick&dirty PoC! */
	struct json_object *parent, *leafnode;
	uchar *leaf;
	DEFiRet;

	MsgLock(pM);
	if(name[0] == '!' && name[1] == '\0') {
		if(pM->json == NULL)
			pM->json = json;
		else
			CHKiRet(jsonMerge(pM->json, json));
	} else {
		if(pM->json == NULL) {
			/* now we need a root obj */
			pM->json = json_object_new_object();
		}
		leaf = jsonPathGetLeaf(name, ustrlen(name));
		CHKiRet(jsonPathFindParent(pM, name, leaf, &parent, 1));
		leafnode = json_object_object_get(parent, (char*)leaf);
		if(leafnode == NULL) {
			json_object_object_add(parent, (char*)leaf, json);
		} else {
			if(json_object_get_type(json) == json_type_object) {
				CHKiRet(jsonMerge(pM->json, json));
			} else {
//dbgprintf("AAAA: leafnode already exists, type is %d, update with %d\n", (int)json_object_get_type(leafnode), (int)json_object_get_type(json));
				/* TODO: improve the code below, however, the current
				 *       state is not really bad */
				if(json_object_get_type(leafnode) == json_type_object) {
					DBGPRINTF("msgAddJSON: trying to update a container "
						  "node with a leaf, name is '%s' - "
						  "forbidden\n", name);
					json_object_put(json);
					ABORT_FINALIZE(RS_RET_INVLD_SETOP);
				}
				/* json-c code indicates we can simply replace a
				 * json type. Unfortunaltely, this is not documented
				 * as part of the interface spec. We still use it,
				 * because it speeds up processing. If it does not work
				 * at some point, use
				 * json_object_object_del(parent, (char*)leaf);
				 * before adding. rgerhards, 2012-09-17
				 */
				json_object_object_add(parent, (char*)leaf, json);
			}
		}
	}

finalize_it:
	MsgUnlock(pM);
	RETiRet;
}

rsRetVal
msgDelJSON(msg_t *pM, uchar *name)
{
	struct json_object *parent, *leafnode;
	uchar *leaf;
	DEFiRet;

dbgprintf("AAAA: unset variable '%s'\n", name);
	MsgLock(pM);
	if(name[0] == '!' && name[1] == '\0') {
		/* strange, but I think we should permit this. After all,
		 * we trust rsyslog.conf to be written by the admin.
		 */
		DBGPRINTF("unsetting JSON root object\n");
		json_object_put(pM->json);
		pM->json = NULL;
	} else {
		if(pM->json == NULL) {
			/* now we need a root obj */
			pM->json = json_object_new_object();
		}
		leaf = jsonPathGetLeaf(name, ustrlen(name));
		CHKiRet(jsonPathFindParent(pM, name, leaf, &parent, 1));
		leafnode = json_object_object_get(parent, (char*)leaf);
DBGPRINTF("AAAA: unset found JSON value path '%s', " "leaf '%s', leafnode %p\n", name, leaf, leafnode);
		if(leafnode == NULL) {
			DBGPRINTF("unset JSON: could not find '%s'\n", name);
			ABORT_FINALIZE(RS_RET_JNAME_NOTFOUND);
		} else {
			DBGPRINTF("deleting JSON value path '%s', "
				  "leaf '%s', type %d\n",
				  name, leaf, json_object_get_type(leafnode));
			json_object_object_del(parent, (char*)leaf);
		}
	}

finalize_it:
	MsgUnlock(pM);
	RETiRet;
}

static struct json_object *
jsonDeepCopy(struct json_object *src)
{
	struct json_object *dst = NULL, *json;
	struct json_object_iter it;
	int arrayLen, i;

	if(src == NULL) goto done;

	switch(json_object_get_type(src)) {
	case json_type_boolean:
		dst = json_object_new_boolean(json_object_get_boolean(src));
		break;
	case json_type_double:
		dst = json_object_new_double(json_object_get_double(src));
		break;
	case json_type_int:
		dst = json_object_new_int(json_object_get_int(src));
		break;
	case json_type_string:
		dst = json_object_new_string(json_object_get_string(src));
		break;
	case json_type_object:
		dst = json_object_new_object();
		json_object_object_foreachC(src, it) {
			json = jsonDeepCopy(it.val);
			json_object_object_add(dst, it.key, json);
		}
		break;
	case json_type_array:
		arrayLen = json_object_array_length(src);
		dst = json_object_new_array();
		for(i = 0 ; i < arrayLen ; ++i) {
			json = json_object_array_get_idx(src, i);
			json = jsonDeepCopy(json);
			json_object_array_add(dst, json);
		}
		break;
	default:DBGPRINTF("jsonDeepCopy(): error unknown type %d\n",
			 json_object_get_type(src));
		dst = NULL;
		break;
	}
done:	return dst;
}


rsRetVal
msgSetJSONFromVar(msg_t *pMsg, uchar *varname, struct var *v)
{
	struct json_object *json = NULL;
	char *cstr;
	DEFiRet;
	switch(v->datatype) {
	case 'S':/* string */
		cstr = es_str2cstr(v->d.estr, NULL);
		json = json_object_new_string(cstr);
		free(cstr);
		break;
	case 'N':/* number (integer) */
		json = json_object_new_int((int) v->d.n);
		break;
	case 'J':/* native JSON */
		json = jsonDeepCopy(v->d.json);
		break;
	default:DBGPRINTF("msgSetJSONFromVar: unsupported datatype %c\n",
		v->datatype);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	msgAddJSON(pMsg, varname+1, json);
finalize_it:
	RETiRet;
}

/* dummy */
rsRetVal msgQueryInterface(void) { return RS_RET_NOT_IMPLEMENTED; }

/* Initialize the message class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-04
 */
BEGINObjClassInit(msg, 1, OBJ_IS_CORE_MODULE)
	/* request objects we use */
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_SERIALIZE, MsgSerialize);
	/* some more inits */
#	if HAVE_MALLOC_TRIM
	INIT_ATOMIC_HELPER_MUT(mutTrimCtr);
#	endif
ENDObjClassInit(msg)
/* vim:set ai:
 */
