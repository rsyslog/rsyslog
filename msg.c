/* msg.c
 * The msg object. Implementation of all msg-related functions
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "rsyslog.h"
#include "syslogd.h"
#include "template.h"
#include "srUtils.h"
#include "msg.h"

/* rgerhards 2004-11-09: helper routines for handling the
 * message object. We do only the most important things. It
 * is our firm hope that this will sooner or later be
 * obsoleted by liblogging.
 */


/* "Constructor" for a msg "object". Returns a pointer to
 * the new object or NULL if no such object could be allocated.
 * An object constructed via this function should only be destroyed
 * via "MsgDestruct()".
 */
msg_t* MsgConstruct(void)
{
	msg_t *pM;

	if((pM = calloc(1, sizeof(msg_t))) != NULL)
	{ /* initialize members that are non-zero */
		pM->iRefCount = 1;
		pM->iSyslogVers = -1;
		pM->iSeverity = -1;
		pM->iFacility = -1;
		getCurrTime(&(pM->tRcvdAt));
	}

	/* DEV debugging only! dprintf("MsgConstruct\t0x%x, ref 1\n", (int)pM);*/

	return(pM);
}


/* Destructor for a msg "object". Must be called to dispose
 * of a msg object.
 */
void MsgDestruct(msg_t * pM)
{
	assert(pM != NULL);
	/* DEV Debugging only ! dprintf("MsgDestruct\t0x%x, Ref now: %d\n", (int)pM, pM->iRefCount - 1); */
	if(--pM->iRefCount == 0)
	{
		/* DEV Debugging Only! dprintf("MsgDestruct\t0x%x, RefCount now 0, doing DESTROY\n", (int)pM); */
		if(pM->pszUxTradMsg != NULL)
			free(pM->pszUxTradMsg);
		if(pM->pszRawMsg != NULL)
			free(pM->pszRawMsg);
		if(pM->pszTAG != NULL)
			free(pM->pszTAG);
		if(pM->pszHOSTNAME != NULL)
			free(pM->pszHOSTNAME);
		if(pM->pszRcvFrom != NULL)
			free(pM->pszRcvFrom);
		if(pM->pszMSG != NULL)
			free(pM->pszMSG);
		if(pM->pszFacility != NULL)
			free(pM->pszFacility);
		if(pM->pszFacilityStr != NULL)
			free(pM->pszFacilityStr);
		if(pM->pszSeverity != NULL)
			free(pM->pszSeverity);
		if(pM->pszSeverityStr != NULL)
			free(pM->pszSeverityStr);
		if(pM->pszRcvdAt3164 != NULL)
			free(pM->pszRcvdAt3164);
		if(pM->pszRcvdAt3339 != NULL)
			free(pM->pszRcvdAt3339);
		if(pM->pszRcvdAt_MySQL != NULL)
			free(pM->pszRcvdAt_MySQL);
		if(pM->pszTIMESTAMP3164 != NULL)
			free(pM->pszTIMESTAMP3164);
		if(pM->pszTIMESTAMP3339 != NULL)
			free(pM->pszTIMESTAMP3339);
		if(pM->pszTIMESTAMP_MySQL != NULL)
			free(pM->pszTIMESTAMP_MySQL);
		if(pM->pszPRI != NULL)
			free(pM->pszPRI);
		free(pM);
	}
}


/* The macros below are used in MsgDup(). I use macros
 * to keep the fuction code somewhat more readyble. It is my
 * replacement for inline functions in CPP
 */
#define tmpCOPYSZ(name) \
	if(pOld->psz##name != NULL) { \
		if((pNew->psz##name = srUtilStrDup(pOld->psz##name, pOld->iLen##name)) == NULL) {\
			MsgDestruct(pNew);\
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
			MsgDestruct(pNew);\
			return NULL;\
		}\
	}
/* Constructs a message object by duplicating another one.
 * Returns NULL if duplication failed.
 * rgerhards, 2007-07-10
 */
msg_t* MsgDup(msg_t* pOld)
{
	msg_t* pNew;

	assert(pOld != NULL);

	if((pNew = (msg_t*) calloc(1, sizeof(msg_t))) == NULL) {
		glblHadMemShortage = 1;
		return NULL;
	}

	/* now copy the message properties */
	pNew->iRefCount = 1;
	pNew->iSyslogVers = pOld->iSyslogVers;
	pNew->bParseHOSTNAME = pOld->bParseHOSTNAME;
	pNew->iSeverity = pOld->iSeverity;
	pNew->iFacility = pOld->iFacility;
	pNew->bParseHOSTNAME = pOld->bParseHOSTNAME;
	pNew->msgFlags = pOld->msgFlags;
	pNew->iProtocolVersion = pOld->iProtocolVersion;
	memcpy(&pNew->tRcvdAt, &pOld->tRcvdAt, sizeof(struct syslogTime));
	memcpy(&pNew->tTIMESTAMP, &pOld->tTIMESTAMP, sizeof(struct syslogTime));
	tmpCOPYSZ(Severity);
	tmpCOPYSZ(SeverityStr);
	tmpCOPYSZ(Facility);
	tmpCOPYSZ(FacilityStr);
	tmpCOPYSZ(PRI);
	tmpCOPYSZ(RawMsg);
	tmpCOPYSZ(MSG);
	tmpCOPYSZ(UxTradMsg);
	tmpCOPYSZ(TAG);
	tmpCOPYSZ(HOSTNAME);
	tmpCOPYSZ(RcvFrom);

	tmpCOPYCSTR(ProgName);
	tmpCOPYCSTR(StrucData);
	tmpCOPYCSTR(APPNAME);
	tmpCOPYCSTR(PROCID);
	tmpCOPYCSTR(MSGID);

	/* we do not copy all other cache properties, as we do not even know
	 * if they are needed once again. So we let them re-create if needed.
	 */

	return pNew;
}
#undef tmpCOPYSZ
#undef tmpCOPYCSTR


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
	pM->iRefCount++;
	/* DEV debugging only! dprintf("MsgAddRef\t0x%x done, Ref now: %d\n", (int)pM, pM->iRefCount);*/
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
 */
static rsRetVal aquirePROCIDFromTAG(msg_t *pM)
{
	register int i;
	int iRet;

	assert(pM != NULL);
	if(pM->pCSPROCID != NULL)
		return RS_RET_OK; /* we are already done ;) */

	if(getProtocolVersion(pM) != 0)
		return RS_RET_OK; /* we can only emulate if we have legacy format */

	/* find first '['... */
	i = 0;
	while((i < pM->iLenTAG) && (pM->pszTAG[i] != '['))
		++i;
	if(!(i < pM->iLenTAG))
		return RS_RET_OK;	/* no [, so can not emulate... */
	
	++i; /* skip '[' */

	/* now obtain the PROCID string... */
	if((pM->pCSPROCID = rsCStrConstruct()) == NULL) 
		return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
	rsCStrSetAllocIncrement(pM->pCSPROCID, 16);
	while((i < pM->iLenTAG) && (pM->pszTAG[i] != ']')) {
		if((iRet = rsCStrAppendChar(pM->pCSPROCID, pM->pszTAG[i])) != RS_RET_OK)
			return iRet;
		++i;
	}

	if(!(i < pM->iLenTAG)) {
		/* oops... it looked like we had a PROCID, but now it has
		 * turned out this is not true. In this case, we need to free
		 * the buffer and simply return. Note that this is NOT an error
		 * case!
		 */
		rsCStrDestruct(pM->pCSPROCID);
		pM->pCSPROCID = NULL;
		return RS_RET_OK;
	}

	/* OK, finaally we could obtain a PROCID. So let's use it ;) */
	if((iRet = rsCStrFinish(pM->pCSPROCID)) != RS_RET_OK)
		return iRet;

	return RS_RET_OK;
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
 * If it is needed, this function should be called first. It checks if it is
 * already set and extracts it, if not.
 * A message object must be provided, else a crash will occur.
 * rgerhards, 2005-10-19
 */
static rsRetVal aquireProgramName(msg_t *pM)
{
	register int i;
	int iRet;

	assert(pM != NULL);
	if(pM->pCSProgName == NULL) {
		/* ok, we do not yet have it. So let's parse the TAG
		 * to obtain it.
		 */
		if((pM->pCSProgName = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pM->pCSProgName, 33);
		for(  i = 0
		    ; (i < pM->iLenTAG) && isprint((int) pM->pszTAG[i])
		      && (pM->pszTAG[i] != '\0') && (pM->pszTAG[i] != ':')
		      && (pM->pszTAG[i] != '[')  && (pM->pszTAG[i] != '/')
		    ; ++i) {
			if((iRet = rsCStrAppendChar(pM->pCSProgName, pM->pszTAG[i])) != RS_RET_OK)
				return iRet;
		}
		if((iRet = rsCStrFinish(pM->pCSProgName)) != RS_RET_OK)
			return iRet;
	}
	return RS_RET_OK;
}


/* This function moves the HOSTNAME inside the message object to the
 * TAG. It is a specialised function used to handle the condition when
 * a message without HOSTNAME is being processed. The missing HOSTNAME
 * is only detected at a later stage, during TAG processing, so that
 * we already had set the HOSTNAME property and now need to move it to
 * the TAG. Of course, we could do this via a couple of get/set methods,
 * but it is far more efficient to do it via this specialised method.
 * This is especially important as this can be a very common case, e.g.
 * when BSD syslog is acting as a sender.
 * rgerhards, 2005-11-10.
 */
void moveHOSTNAMEtoTAG(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pszTAG != NULL)
		free(pM->pszTAG);
	pM->pszTAG = pM->pszHOSTNAME;
	pM->iLenTAG = pM->iLenHOSTNAME;
	pM->pszHOSTNAME = NULL;
	pM->iLenHOSTNAME = 0;
}

/* Access methods - dumb & easy, not a comment for each ;)
 */
void setProtocolVersion(msg_t *pM, int iNewVersion)
{
	assert(pM != NULL);
	if(iNewVersion != 0 && iNewVersion != 1) {
		dprintf("Tried to set unsupported protocol version %d - changed to 0.\n", iNewVersion);
		iNewVersion = 0;
	}
	pM->iProtocolVersion = iNewVersion;
}

int getProtocolVersion(msg_t *pM)
{
	assert(pM != NULL);
	return(pM->iProtocolVersion);
}

/* note: string is taken from constant pool, do NOT free */
char *getProtocolVersionString(msg_t *pM)
{
	assert(pM != NULL);
	return(pM->iProtocolVersion ? "1" : "0");
}

int getMSGLen(msg_t *pM)
{
	return((pM == NULL) ? 0 : pM->iLenMSG);
}


char *getRawMsg(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszRawMsg == NULL)
			return "";
		else
			return (char*)pM->pszRawMsg;
}

char *getUxTradMsg(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszUxTradMsg == NULL)
			return "";
		else
			return (char*)pM->pszUxTradMsg;
}

char *getMSG(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszMSG == NULL)
			return "";
		else
			return (char*)pM->pszMSG;
}


/* Get PRI value in text form */
char *getPRI(msg_t *pM)
{
	if(pM == NULL)
		return "";

	if(pM->pszPRI == NULL) {
		/* OK, we need to construct it... 
		 * we use a 5 byte buffer - as of 
		 * RFC 3164, it can't be longer. Should it
		 * still be, snprintf will truncate...
		 */
		if((pM->pszPRI = malloc(5)) == NULL) return "";
		pM->iLenPRI = snprintf((char*)pM->pszPRI, 5, "%d",
			LOG_MAKEPRI(pM->iFacility, pM->iSeverity));
	}

	return (char*)pM->pszPRI;
}


/* Get PRI value as integer */
int getPRIi(msg_t *pM)
{
	assert(pM != NULL);
	return (pM->iFacility << 3) + (pM->iSeverity);
}


char *getTimeReported(msg_t *pM, enum tplFormatTypes eFmt)
{
	if(pM == NULL)
		return "";

	switch(eFmt) {
	case tplFmtDefault:
		if(pM->pszTIMESTAMP3164 == NULL) {
			if((pM->pszTIMESTAMP3164 = malloc(16)) == NULL) return "";
			formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164, 16);
		}
		return(pM->pszTIMESTAMP3164);
	case tplFmtMySQLDate:
		if(pM->pszTIMESTAMP_MySQL == NULL) {
			if((pM->pszTIMESTAMP_MySQL = malloc(15)) == NULL) return "";
			formatTimestampToMySQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_MySQL, 15);
		}
		return(pM->pszTIMESTAMP_MySQL);
	case tplFmtRFC3164Date:
		if(pM->pszTIMESTAMP3164 == NULL) {
			if((pM->pszTIMESTAMP3164 = malloc(16)) == NULL) return "";
			formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164, 16);
		}
		return(pM->pszTIMESTAMP3164);
	case tplFmtRFC3339Date:
		if(pM->pszTIMESTAMP3339 == NULL) {
			if((pM->pszTIMESTAMP3339 = malloc(33)) == NULL) return "";
			formatTimestamp3339(&pM->tTIMESTAMP, pM->pszTIMESTAMP3339, 33);
		}
		return(pM->pszTIMESTAMP3339);
	}
	return "INVALID eFmt OPTION!";
}

char *getTimeGenerated(msg_t *pM, enum tplFormatTypes eFmt)
{
	if(pM == NULL)
		return "";

	switch(eFmt) {
	case tplFmtDefault:
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = malloc(16)) == NULL) return "";
			formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		}
		return(pM->pszRcvdAt3164);
	case tplFmtMySQLDate:
		if(pM->pszRcvdAt_MySQL == NULL) {
			if((pM->pszRcvdAt_MySQL = malloc(15)) == NULL) return "";
			formatTimestampToMySQL(&pM->tRcvdAt, pM->pszRcvdAt_MySQL, 15);
		}
		return(pM->pszRcvdAt_MySQL);
	case tplFmtRFC3164Date:
		if((pM->pszRcvdAt3164 = malloc(16)) == NULL) return "";
		formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		return(pM->pszRcvdAt3164);
	case tplFmtRFC3339Date:
		if(pM->pszRcvdAt3339 == NULL) {
			if((pM->pszRcvdAt3339 = malloc(33)) == NULL) return "";
			formatTimestamp3339(&pM->tRcvdAt, pM->pszRcvdAt3339, 33);
		}
		return(pM->pszRcvdAt3339);
	}
	return "INVALID eFmt OPTION!";
}


char *getSeverity(msg_t *pM)
{
	if(pM == NULL)
		return "";

	if(pM->pszSeverity == NULL) {
		/* we use a 2 byte buffer - can only be one digit */
		if((pM->pszSeverity = malloc(2)) == NULL) return "";
		pM->iLenSeverity =
		   snprintf((char*)pM->pszSeverity, 2, "%d", pM->iSeverity);
	}
	return((char*)pM->pszSeverity);
}


char *getSeverityStr(msg_t *pM)
{
	syslogCODE *c;
	int val;
	char *name = NULL;

	if(pM == NULL)
		return "";

	if(pM->pszSeverityStr == NULL) {
		for(c = rs_prioritynames, val = pM->iSeverity; c->c_name; c++)
			if(c->c_val == val) {
				name = c->c_name;
				break;
			}
		if(name == NULL) {
			/* we use a 2 byte buffer - can only be one digit */
			if((pM->pszSeverityStr = malloc(2)) == NULL) return "";
			pM->iLenSeverityStr =
				snprintf((char*)pM->pszSeverityStr, 2, "%d", pM->iSeverity);
		} else {
			if((pM->pszSeverityStr = (uchar*) strdup(name)) == NULL) return "";
			pM->iLenSeverityStr = strlen((char*)name);
		}
	}
	return((char*)pM->pszSeverityStr);
}

char *getFacility(msg_t *pM)
{
	if(pM == NULL)
		return "";

	if(pM->pszFacility == NULL) {
		/* we use a 12 byte buffer - as of 
		 * syslog-protocol, facility can go
		 * up to 2^32 -1
		 */
		if((pM->pszFacility = malloc(12)) == NULL) return "";
		pM->iLenFacility =
		   snprintf((char*)pM->pszFacility, 12, "%d", pM->iFacility);
	}
	return((char*)pM->pszFacility);
}

char *getFacilityStr(msg_t *pM)
{
        syslogCODE *c;
        int val;
        char *name = NULL;

        if(pM == NULL)
                return "";

        if(pM->pszFacilityStr == NULL) {
                for(c = rs_facilitynames, val = pM->iFacility << 3; c->c_name; c++)
                        if(c->c_val == val) {
                                name = c->c_name;
                                break;
                        }
                if(name == NULL) {
			/* we use a 12 byte buffer - as of 
			 * syslog-protocol, facility can go
			 * up to 2^32 -1
			 */
			if((pM->pszFacilityStr = malloc(12)) == NULL) return "";
			pM->iLenFacilityStr =
				snprintf((char*)pM->pszFacilityStr, 12, "%d", val >> 3);
                } else {
                        if((pM->pszFacilityStr = (uchar*)strdup(name)) == NULL) return "";
                        pM->iLenFacilityStr = strlen((char*)name);
                }
        }
        return((char*)pM->pszFacilityStr);
}


/* rgerhards 2004-11-24: set APP-NAME in msg object
 */
rsRetVal MsgSetAPPNAME(msg_t *pMsg, char* pszAPPNAME)
{
	assert(pMsg != NULL);
	if(pMsg->pCSAPPNAME == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSAPPNAME = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSAPPNAME, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSAPPNAME, (uchar*) pszAPPNAME);
}


/* This function tries to emulate APPNAME if it is not present. Its
 * main use is when we have received a log record via legacy syslog and
 * now would like to send out the same one via syslog-protocol.
 */
static void tryEmulateAPPNAME(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME != NULL)
		return; /* we are already done */

	if(getProtocolVersion(pM) == 0) {
		/* only then it makes sense to emulate */
		MsgSetAPPNAME(pM, getProgramName(pM));
	}
}


/* rgerhards, 2005-11-24
 */
int getAPPNAMELen(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	return (pM->pCSAPPNAME == NULL) ? 0 : rsCStrLen(pM->pCSAPPNAME);
}


/* rgerhards, 2005-11-24
 */
char *getAPPNAME(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	return (pM->pCSAPPNAME == NULL) ? "" : (char*) rsCStrGetSzStrNoNULL(pM->pCSAPPNAME);
}




/* rgerhards 2004-11-24: set PROCID in msg object
 */
rsRetVal MsgSetPROCID(msg_t *pMsg, char* pszPROCID)
{
	assert(pMsg != NULL);
	if(pMsg->pCSPROCID == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSPROCID = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSPROCID, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSPROCID, (uchar*) pszPROCID);
}

/* rgerhards, 2005-11-24
 */
int getPROCIDLen(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	return (pM->pCSPROCID == NULL) ? 1 : rsCStrLen(pM->pCSPROCID);
}


/* rgerhards, 2005-11-24
 */
char *getPROCID(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	return (pM->pCSPROCID == NULL) ? "-" : (char*) rsCStrGetSzStrNoNULL(pM->pCSPROCID);
}


/* rgerhards 2004-11-24: set MSGID in msg object
 */
rsRetVal MsgSetMSGID(msg_t *pMsg, char* pszMSGID)
{
	assert(pMsg != NULL);
	if(pMsg->pCSMSGID == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSMSGID = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSMSGID, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSMSGID, (uchar*) pszMSGID);
}

/* rgerhards, 2005-11-24
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int getMSGIDLen(msg_t *pM)
{
	return (pM->pCSMSGID == NULL) ? 1 : rsCStrLen(pM->pCSMSGID);
}
#endif


/* rgerhards, 2005-11-24
 */
char *getMSGID(msg_t *pM)
{
	return (pM->pCSMSGID == NULL) ? "-" : (char*) rsCStrGetSzStrNoNULL(pM->pCSMSGID);
}


/* Set the TAG to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetTAG().
 * rgerhards 2004-11-19
 */
void MsgAssignTAG(msg_t *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	pMsg->iLenTAG = (pBuf == NULL) ? 0 : strlen(pBuf);
	pMsg->pszTAG =  (uchar*) pBuf;
}


/* rgerhards 2004-11-16: set TAG in msg object
 */
void MsgSetTAG(msg_t *pMsg, char* pszTAG)
{
	assert(pMsg != NULL);
	if(pMsg->pszTAG != NULL)
		free(pMsg->pszTAG);
	pMsg->iLenTAG = strlen(pszTAG);
	if((pMsg->pszTAG = malloc(pMsg->iLenTAG + 1)) != NULL)
		memcpy(pMsg->pszTAG, pszTAG, pMsg->iLenTAG + 1);
	else
		dprintf("Could not allocate memory in MsgSetTAG()\n");
}


/* This function tries to emulate the TAG if none is
 * set. Its primary purpose is to provide an old-style TAG
 * when a syslog-protocol message has been received. Then,
 * the tag is APP-NAME "[" PROCID "]". The function first checks
 * if there is a TAG and, if not, if it can emulate it.
 * rgerhards, 2005-11-24
 */
static void tryEmulateTAG(msg_t *pM)
{
	int iTAGLen;
	char *pBuf;
	assert(pM != NULL);

	if(pM->pszTAG != NULL) 
		return; /* done, no need to emulate */
	
	if(getProtocolVersion(pM) == 1) {
		if(!strcmp(getPROCID(pM), "-")) {
			/* no process ID, use APP-NAME only */
			MsgSetTAG(pM, getAPPNAME(pM));
		} else {
			/* now we can try to emulate */
			iTAGLen = getAPPNAMELen(pM) + getPROCIDLen(pM) + 3;
			if((pBuf = malloc(iTAGLen * sizeof(char))) == NULL)
				return; /* nothing we can do */
			snprintf(pBuf, iTAGLen, "%s[%s]", getAPPNAME(pM), getPROCID(pM));
			MsgAssignTAG(pM, pBuf);
		}
	}
}


#if 0 /* This method is currently not called, be we like to preserve it */
static int getTAGLen(msg_t *pM)
{
	if(pM == NULL)
		return 0;
	else {
		tryEmulateTAG(pM);
		if(pM->pszTAG == NULL)
			return 0;
		else
			return pM->iLenTAG;
	}
}
#endif


char *getTAG(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else {
		tryEmulateTAG(pM);
		if(pM->pszTAG == NULL)
			return "";
		else
			return (char*) pM->pszTAG;
	}
}


int getHOSTNAMELen(msg_t *pM)
{
	if(pM == NULL)
		return 0;
	else
		if(pM->pszHOSTNAME == NULL)
			return 0;
		else
			return pM->iLenHOSTNAME;
}


char *getHOSTNAME(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszHOSTNAME == NULL)
			return "";
		else
			return (char*) pM->pszHOSTNAME;
}


char *getRcvFrom(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszRcvFrom == NULL)
			return "";
		else
			return (char*) pM->pszRcvFrom;
}
/* rgerhards 2004-11-24: set STRUCTURED DATA in msg object
 */
rsRetVal MsgSetStructuredData(msg_t *pMsg, char* pszStrucData)
{
	assert(pMsg != NULL);
	if(pMsg->pCSStrucData == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSStrucData = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSStrucData, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSStrucData, (uchar*) pszStrucData);
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
char *getStructuredData(msg_t *pM)
{
	return (pM->pCSStrucData == NULL) ? "-" : (char*) rsCStrGetSzStrNoNULL(pM->pCSStrucData);
}



/* get the length of the "programname" sz string
 * rgerhards, 2005-10-19
 */
int getProgramNameLen(msg_t *pM)
{
	int iRet;

	assert(pM != NULL);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dprintf("error %d returned by aquireProgramName() in getProgramNameLen()\n", iRet);
		return 0; /* best we can do (consistent wiht what getProgramName() returns) */
	}

	return (pM->pCSProgName == NULL) ? 0 : rsCStrLen(pM->pCSProgName);
}


/* get the "programname" as sz string
 * rgerhards, 2005-10-19
 */
char *getProgramName(msg_t *pM)
{
	int iRet;

	assert(pM != NULL);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dprintf("error %d returned by aquireProgramName() in getProgramName()\n", iRet);
		return ""; /* best we can do */
	}

	return (pM->pCSProgName == NULL) ? "" : (char*) rsCStrGetSzStrNoNULL(pM->pCSProgName);
}


/* rgerhards 2004-11-16: set pszRcvFrom in msg object
 */
void MsgSetRcvFrom(msg_t *pMsg, char* pszRcvFrom)
{
	assert(pMsg != NULL);
	if(pMsg->pszRcvFrom != NULL)
		free(pMsg->pszRcvFrom);

	pMsg->iLenRcvFrom = strlen(pszRcvFrom);
	if((pMsg->pszRcvFrom = malloc(pMsg->iLenRcvFrom + 1)) != NULL) {
		memcpy(pMsg->pszRcvFrom, pszRcvFrom, pMsg->iLenRcvFrom + 1);
	}
}


/* Set the HOSTNAME to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetHOSTNAME().
 * rgerhards 2004-11-19
 */
void MsgAssignHOSTNAME(msg_t *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	assert(pBuf != NULL);
	pMsg->iLenHOSTNAME = strlen(pBuf);
	pMsg->pszHOSTNAME = (uchar*) pBuf;
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
void MsgSetHOSTNAME(msg_t *pMsg, char* pszHOSTNAME)
{
	assert(pMsg != NULL);
	if(pMsg->pszHOSTNAME != NULL)
		free(pMsg->pszHOSTNAME);

	pMsg->iLenHOSTNAME = strlen(pszHOSTNAME);
	if((pMsg->pszHOSTNAME = malloc(pMsg->iLenHOSTNAME + 1)) != NULL)
		memcpy(pMsg->pszHOSTNAME, pszHOSTNAME, pMsg->iLenHOSTNAME + 1);
	else
		dprintf("Could not allocate memory in MsgSetHOSTNAME()\n");
}


/* Set the UxTradMsg to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetUxTradMsg().
 * rgerhards 2004-11-19
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static void MsgAssignUxTradMsg(msg_t *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	assert(pBuf != NULL);
	pMsg->iLenUxTradMsg = strlen(pBuf);
	pMsg->pszUxTradMsg = pBuf;
}
#endif


/* rgerhards 2004-11-17: set the traditional Unix message in msg object
 */
int MsgSetUxTradMsg(msg_t *pMsg, char* pszUxTradMsg)
{
	assert(pMsg != NULL);
	assert(pszUxTradMsg != NULL);
	pMsg->iLenUxTradMsg = strlen(pszUxTradMsg);
	if(pMsg->pszUxTradMsg != NULL)
		free(pMsg->pszUxTradMsg);
	if((pMsg->pszUxTradMsg = malloc(pMsg->iLenUxTradMsg + 1)) != NULL)
		memcpy(pMsg->pszUxTradMsg, pszUxTradMsg, pMsg->iLenUxTradMsg + 1);
	else
		dprintf("Could not allocate memory for pszUxTradMsg buffer.");

	return(0);
}


/* rgerhards 2004-11-09: set MSG in msg object
 */
void MsgSetMSG(msg_t *pMsg, char* pszMSG)
{
	assert(pMsg != NULL);
	assert(pszMSG != NULL);

	if(pMsg->pszMSG != NULL)
		free(pMsg->pszMSG);

	pMsg->iLenMSG = strlen(pszMSG);
	if((pMsg->pszMSG = malloc(pMsg->iLenMSG + 1)) != NULL)
		memcpy(pMsg->pszMSG, pszMSG, pMsg->iLenMSG + 1);
	else
		dprintf("MsgSetMSG could not allocate memory for pszMSG buffer.");
}

/* rgerhards 2004-11-11: set RawMsg in msg object
 */
void MsgSetRawMsg(msg_t *pMsg, char* pszRawMsg)
{
	assert(pMsg != NULL);
	if(pMsg->pszRawMsg != NULL)
		free(pMsg->pszRawMsg);

	pMsg->iLenRawMsg = strlen(pszRawMsg);
	if((pMsg->pszRawMsg = malloc(pMsg->iLenRawMsg + 1)) != NULL)
		memcpy(pMsg->pszRawMsg, pszRawMsg, pMsg->iLenRawMsg + 1);
	else
		dprintf("Could not allocate memory for pszRawMsg buffer.");
}




/*
 * vi:set ai:
 */
