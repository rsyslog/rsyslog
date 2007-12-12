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
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "syslogd.h"
#include "srUtils.h"
#include "template.h"
#include "msg.h"

static syslogCODE rs_prioritynames[] =
  {
    { "alert", LOG_ALERT },
    { "crit", LOG_CRIT },
    { "debug", LOG_DEBUG },
    { "emerg", LOG_EMERG },
    { "err", LOG_ERR },
    { "error", LOG_ERR },               /* DEPRECATED */
    { "info", LOG_INFO },
    { "none", INTERNAL_NOPRI },         /* INTERNAL */
    { "notice", LOG_NOTICE },
    { "panic", LOG_EMERG },             /* DEPRECATED */
    { "warn", LOG_WARNING },            /* DEPRECATED */
    { "warning", LOG_WARNING },
    { NULL, -1 }
  };

static syslogCODE rs_facilitynames[] =
  {
    { "auth", LOG_AUTH },
    { "authpriv", LOG_AUTHPRIV },
    { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON },
    { "ftp", LOG_FTP },
    { "kern", LOG_KERN },
    { "lpr", LOG_LPR },
    { "mail", LOG_MAIL },
    { "mark", INTERNAL_MARK },          /* INTERNAL */
    { "news", LOG_NEWS },
    { "security", LOG_AUTH },           /* DEPRECATED */
    { "syslog", LOG_SYSLOG },
    { "user", LOG_USER },
    { "uucp", LOG_UUCP },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { NULL, -1 }
  };

/* The following functions will support advanced output module
 * multithreading, once this is implemented. Currently, we
 * include them as hooks only. The idea is that we need to guard
 * some msg objects data fields against concurrent access if
 * we run on multiple threads. Please note that in any case this
 * is not necessary for calls from INPUT modules, because they
 * construct the message object and do this serially. Only when
 * the message is in the processing queue, multiple threads may
 * access a single object. Consequently, there are no guard functions
 * for "set" methods, as these are called during input. Only "get"
 * functions that modify important structures have them.
 * rgerhards, 2007-07-20
 */
#define MsgLock(pMsg)
#define MsgUnlock(pMsg)


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

	/* DEV debugging only! dbgprintf("MsgConstruct\t0x%x, ref 1\n", (int)pM);*/

	return(pM);
}


/* Destructor for a msg "object". Must be called to dispose
 * of a msg object.
 */
void MsgDestruct(msg_t * pM)
{
	assert(pM != NULL);
	/* DEV Debugging only ! dbgprintf("MsgDestruct\t0x%x, Ref now: %d\n", (int)pM, pM->iRefCount - 1); */
	if(--pM->iRefCount == 0)
	{
		/* DEV Debugging Only! dbgprintf("MsgDestruct\t0x%x, RefCount now 0, doing DESTROY\n", (int)pM); */
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
		if(pM->pszRcvdAt_PgSQL != NULL)
			free(pM->pszRcvdAt_PgSQL);
		if(pM->pszTIMESTAMP3164 != NULL)
			free(pM->pszTIMESTAMP3164);
		if(pM->pszTIMESTAMP3339 != NULL)
			free(pM->pszTIMESTAMP3339);
		if(pM->pszTIMESTAMP_MySQL != NULL)
			free(pM->pszTIMESTAMP_MySQL);
		if(pM->pszTIMESTAMP_PgSQL != NULL)
			free(pM->pszTIMESTAMP_PgSQL);
		if(pM->pszPRI != NULL)
			free(pM->pszPRI);
		if(pM->pCSProgName != NULL)
			rsCStrDestruct(pM->pCSProgName);
		if(pM->pCSStrucData != NULL)
			rsCStrDestruct(pM->pCSStrucData);
		if(pM->pCSAPPNAME != NULL)
			rsCStrDestruct(pM->pCSAPPNAME);
		if(pM->pCSPROCID != NULL)
			rsCStrDestruct(pM->pCSPROCID);
		if(pM->pCSMSGID != NULL)
			rsCStrDestruct(pM->pCSMSGID);
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
	MsgLock();
	pM->iRefCount++;
	MsgUnlock();
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
		dbgprintf("Tried to set unsupported protocol version %d - changed to 0.\n", iNewVersion);
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

	MsgLock();
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
	MsgUnlock();

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
		MsgLock();
		if(pM->pszTIMESTAMP3164 == NULL) {
			if((pM->pszTIMESTAMP3164 = malloc(16)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164, 16);
		}
		MsgUnlock();
		return(pM->pszTIMESTAMP3164);
	case tplFmtMySQLDate:
		MsgLock();
		if(pM->pszTIMESTAMP_MySQL == NULL) {
			if((pM->pszTIMESTAMP_MySQL = malloc(15)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestampToMySQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_MySQL, 15);
		}
		MsgUnlock();
		return(pM->pszTIMESTAMP_MySQL);
        case tplFmtPgSQLDate:
                MsgLock();
                if(pM->pszTIMESTAMP_PgSQL == NULL) {
                        if((pM->pszTIMESTAMP_PgSQL = malloc(21)) == NULL) {
                                glblHadMemShortage = 1;
                                MsgUnlock();
                                return "";
                        }
                        formatTimestampToPgSQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_PgSQL, 21);
                }
                MsgUnlock();
                return(pM->pszTIMESTAMP_PgSQL);
	case tplFmtRFC3164Date:
		MsgLock();
		if(pM->pszTIMESTAMP3164 == NULL) {
			if((pM->pszTIMESTAMP3164 = malloc(16)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164, 16);
		}
		MsgUnlock();
		return(pM->pszTIMESTAMP3164);
	case tplFmtRFC3339Date:
		MsgLock();
		if(pM->pszTIMESTAMP3339 == NULL) {
			if((pM->pszTIMESTAMP3339 = malloc(33)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return ""; /* TODO: check this: can it cause a free() of constant memory?) */
			}
			formatTimestamp3339(&pM->tTIMESTAMP, pM->pszTIMESTAMP3339, 33);
		}
		MsgUnlock();
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
		MsgLock();
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = malloc(16)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		}
		MsgUnlock();
		return(pM->pszRcvdAt3164);
	case tplFmtMySQLDate:
		MsgLock();
		if(pM->pszRcvdAt_MySQL == NULL) {
			if((pM->pszRcvdAt_MySQL = malloc(15)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestampToMySQL(&pM->tRcvdAt, pM->pszRcvdAt_MySQL, 15);
		}
		MsgUnlock();
		return(pM->pszRcvdAt_MySQL);
        case tplFmtPgSQLDate:
                MsgLock();
                if(pM->pszRcvdAt_PgSQL == NULL) {
                        if((pM->pszRcvdAt_PgSQL = malloc(21)) == NULL) {
                                glblHadMemShortage = 1;
                                MsgUnlock();
                                return "";
                        }
                        formatTimestampToPgSQL(&pM->tRcvdAt, pM->pszRcvdAt_PgSQL, 21);
                }
                MsgUnlock();
                return(pM->pszRcvdAt_PgSQL);
	case tplFmtRFC3164Date:
		MsgLock();
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = malloc(16)) == NULL) {
					glblHadMemShortage = 1;
					MsgUnlock();
					return "";
				}
			formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		}
		MsgUnlock();
		return(pM->pszRcvdAt3164);
	case tplFmtRFC3339Date:
		MsgLock();
		if(pM->pszRcvdAt3339 == NULL) {
			if((pM->pszRcvdAt3339 = malloc(33)) == NULL) {
				glblHadMemShortage = 1;
				MsgUnlock();
				return "";
			}
			formatTimestamp3339(&pM->tRcvdAt, pM->pszRcvdAt3339, 33);
		}
		MsgUnlock();
		return(pM->pszRcvdAt3339);
	}
	return "INVALID eFmt OPTION!";
}


char *getSeverity(msg_t *pM)
{
	if(pM == NULL)
		return "";

	MsgLock();
	if(pM->pszSeverity == NULL) {
		/* we use a 2 byte buffer - can only be one digit */
		if((pM->pszSeverity = malloc(2)) == NULL) { MsgUnlock() ; return ""; }
		pM->iLenSeverity =
		   snprintf((char*)pM->pszSeverity, 2, "%d", pM->iSeverity);
	}
	MsgUnlock();
	return((char*)pM->pszSeverity);
}


char *getSeverityStr(msg_t *pM)
{
	syslogCODE *c;
	int val;
	char *name = NULL;

	if(pM == NULL)
		return "";

	MsgLock();
	if(pM->pszSeverityStr == NULL) {
		for(c = rs_prioritynames, val = pM->iSeverity; c->c_name; c++)
			if(c->c_val == val) {
				name = c->c_name;
				break;
			}
		if(name == NULL) {
			/* we use a 2 byte buffer - can only be one digit */
			if((pM->pszSeverityStr = malloc(2)) == NULL) { MsgUnlock() ; return ""; }
			pM->iLenSeverityStr =
				snprintf((char*)pM->pszSeverityStr, 2, "%d", pM->iSeverity);
		} else {
			if((pM->pszSeverityStr = (uchar*) strdup(name)) == NULL) { MsgUnlock() ; return ""; }
			pM->iLenSeverityStr = strlen((char*)name);
		}
	}
	MsgUnlock();
	return((char*)pM->pszSeverityStr);
}

char *getFacility(msg_t *pM)
{
	if(pM == NULL)
		return "";

	MsgLock();
	if(pM->pszFacility == NULL) {
		/* we use a 12 byte buffer - as of 
		 * syslog-protocol, facility can go
		 * up to 2^32 -1
		 */
		if((pM->pszFacility = malloc(12)) == NULL) { MsgUnlock() ; return ""; }
		pM->iLenFacility =
		   snprintf((char*)pM->pszFacility, 12, "%d", pM->iFacility);
	}
	MsgUnlock();
	return((char*)pM->pszFacility);
}

char *getFacilityStr(msg_t *pM)
{
        syslogCODE *c;
        int val;
        char *name = NULL;

        if(pM == NULL)
                return "";

	MsgLock();
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
			if((pM->pszFacilityStr = malloc(12)) == NULL) { MsgUnlock() ; return ""; }
			pM->iLenFacilityStr =
				snprintf((char*)pM->pszFacilityStr, 12, "%d", val >> 3);
                } else {
                        if((pM->pszFacilityStr = (uchar*)strdup(name)) == NULL) { MsgUnlock() ; return ""; }
                        pM->iLenFacilityStr = strlen((char*)name);
                }
        }
	MsgUnlock();
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
	MsgLock();
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	MsgUnlock();
	return (pM->pCSAPPNAME == NULL) ? 0 : rsCStrLen(pM->pCSAPPNAME);
}


/* rgerhards, 2005-11-24
 */
char *getAPPNAME(msg_t *pM)
{
	assert(pM != NULL);
	MsgLock();
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	MsgUnlock();
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
	MsgLock();
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	MsgUnlock();
	return (pM->pCSPROCID == NULL) ? 1 : rsCStrLen(pM->pCSPROCID);
}


/* rgerhards, 2005-11-24
 */
char *getPROCID(msg_t *pM)
{
	assert(pM != NULL);
	MsgLock();
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	MsgUnlock();
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
void MsgAssignTAG(msg_t *pMsg, uchar *pBuf)
{
	assert(pMsg != NULL);
	pMsg->iLenTAG = (pBuf == NULL) ? 0 : strlen((char*)pBuf);
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
		dbgprintf("Could not allocate memory in MsgSetTAG()\n");
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
	uchar *pBuf;
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
			snprintf((char*)pBuf, iTAGLen, "%s[%s]", getAPPNAME(pM), getPROCID(pM));
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
	char *ret;

	MsgLock();
	if(pM == NULL)
		ret = "";
	else {
		tryEmulateTAG(pM);
		if(pM->pszTAG == NULL)
			ret = "";
		else
			ret = (char*) pM->pszTAG;
	}
	MsgUnlock();
	return(ret);
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
	MsgLock();
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dbgprintf("error %d returned by aquireProgramName() in getProgramNameLen()\n", iRet);
		MsgUnlock();
		return 0; /* best we can do (consistent wiht what getProgramName() returns) */
	}
	MsgUnlock();

	return (pM->pCSProgName == NULL) ? 0 : rsCStrLen(pM->pCSProgName);
}


/* get the "programname" as sz string
 * rgerhards, 2005-10-19
 */
char *getProgramName(msg_t *pM)
{
	int iRet;

	assert(pM != NULL);
	MsgLock();
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dbgprintf("error %d returned by aquireProgramName() in getProgramName()\n", iRet);
		MsgUnlock();
		return ""; /* best we can do */
	}
	MsgUnlock();

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
		dbgprintf("Could not allocate memory in MsgSetHOSTNAME()\n");
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
		dbgprintf("Could not allocate memory for pszUxTradMsg buffer.");

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
	if((pMsg->pszMSG = (uchar*) malloc(pMsg->iLenMSG + 1)) != NULL)
		memcpy(pMsg->pszMSG, pszMSG, pMsg->iLenMSG + 1);
	else
		dbgprintf("MsgSetMSG could not allocate memory for pszMSG buffer.");
}

/* rgerhards 2004-11-11: set RawMsg in msg object
 */
void MsgSetRawMsg(msg_t *pMsg, char* pszRawMsg)
{
	assert(pMsg != NULL);
	if(pMsg->pszRawMsg != NULL)
		free(pMsg->pszRawMsg);

	pMsg->iLenRawMsg = strlen(pszRawMsg);
	if((pMsg->pszRawMsg = (uchar*) malloc(pMsg->iLenRawMsg + 1)) != NULL)
		memcpy(pMsg->pszRawMsg, pszRawMsg, pMsg->iLenRawMsg + 1);
	else
		dbgprintf("Could not allocate memory for pszRawMsg buffer.");
}


/* Decode a priority into textual information like auth.emerg.
 * The variable pRes must point to a user-supplied buffer and
 * pResLen must contain its size. The pointer to the buffer
 * is also returned, what makes this functiona suitable for
 * use in printf-like functions.
 * Note: a buffer size of 20 characters is always sufficient.
 * Interface to this function changed 2007-06-15 by RGerhards
 */
char *textpri(char *pRes, size_t pResLen, int pri)
{
	syslogCODE *c_pri, *c_fac;

	assert(pRes != NULL);
	assert(pResLen > 0);

	for (c_fac = rs_facilitynames; c_fac->c_name && !(c_fac->c_val == LOG_FAC(pri)<<3); c_fac++);
	for (c_pri = rs_prioritynames; c_pri->c_name && !(c_pri->c_val == LOG_PRI(pri)); c_pri++);

	snprintf (pRes, pResLen, "%s.%s<%d>", c_fac->c_name, c_pri->c_name, pri);

	return pRes;
}


/* This function returns the current date in different
 * variants. It is used to construct the $NOW series of
 * system properties. The returned buffer must be freed
 * by the caller when no longer needed. If the function
 * can not allocate memory, it returns a NULL pointer.
 * Added 2007-07-10 rgerhards
 */
typedef enum ENOWType { NOW_NOW, NOW_YEAR, NOW_MONTH, NOW_DAY, NOW_HOUR, NOW_MINUTE } eNOWType;
#define tmpBUFSIZE 16	/* size of formatting buffer */
static uchar *getNOW(eNOWType eNow)
{
	uchar *pBuf;
	struct syslogTime t;

	if((pBuf = (uchar*) malloc(sizeof(uchar) * tmpBUFSIZE)) == NULL) {
		glblHadMemShortage = 1;
		return NULL;
	}

	getCurrTime(&t);
	switch(eNow) {
	case NOW_NOW:
		snprintf((char*) pBuf, tmpBUFSIZE, "%4.4d-%2.2d-%2.2d", t.year, t.month, t.day);
		break;
	case NOW_YEAR:
		snprintf((char*) pBuf, tmpBUFSIZE, "%4.4d", t.year);
		break;
	case NOW_MONTH:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.month);
		break;
	case NOW_DAY:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.day);
		break;
	case NOW_HOUR:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.hour);
		break;
	case NOW_MINUTE:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.minute);
		break;
	}

	return(pBuf);
}
#undef tmpBUFSIZE /* clean up */


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
 * 1, the caller MUST free 1. Handling this wrongly leads to either
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
char *MsgGetProp(msg_t *pMsg, struct templateEntry *pTpe,
                 rsCStrObj *pCSPropName, unsigned short *pbMustBeFreed)
{
	uchar *pName;
	char *pRes; /* result pointer */
	char *pBufStart;
	char *pBuf;
	int iLen;

#ifdef	FEATURE_REGEXP
	/* Variables necessary for regular expression matching */
	size_t nmatch = 2;
	regmatch_t pmatch[2];
#endif

	assert(pMsg != NULL);
	assert(pbMustBeFreed != NULL);

	if(pCSPropName == NULL) {
		assert(pTpe != NULL);
		pName = pTpe->data.field.pPropRepl;
	} else {
		pName = rsCStrGetSzStrNoNULL(pCSPropName);
	}
	*pbMustBeFreed = 0;

	/* sometimes there are aliases to the original MonitoWare
	 * property names. These come after || in the ifs below. */
	if(!strcmp((char*) pName, "msg")) {
		pRes = getMSG(pMsg);
	} else if(!strcmp((char*) pName, "rawmsg")) {
		pRes = getRawMsg(pMsg);
	} else if(!strcmp((char*) pName, "UxTradMsg")) {
		pRes = getUxTradMsg(pMsg);
	} else if(!strcmp((char*) pName, "FROMHOST")) {
		pRes = getRcvFrom(pMsg);
	} else if(!strcmp((char*) pName, "source")
		  || !strcmp((char*) pName, "HOSTNAME")) {
		pRes = getHOSTNAME(pMsg);
	} else if(!strcmp((char*) pName, "syslogtag")) {
		pRes = getTAG(pMsg);
	} else if(!strcmp((char*) pName, "PRI")) {
		pRes = getPRI(pMsg);
	} else if(!strcmp((char*) pName, "PRI-text")) {
		pBuf = malloc(20 * sizeof(char));
		if(pBuf == NULL) {
			*pbMustBeFreed = 0;
			return "**OUT OF MEMORY**";
		} else {
			*pbMustBeFreed = 1;
			pRes = textpri(pBuf, 20, getPRIi(pMsg));
		}
	} else if(!strcmp((char*) pName, "iut")) {
		pRes = "1"; /* always 1 for syslog messages (a MonitorWare thing;)) */
	} else if(!strcmp((char*) pName, "syslogfacility")) {
		pRes = getFacility(pMsg);
	} else if(!strcmp((char*) pName, "syslogfacility-text")) {
		pRes = getFacilityStr(pMsg);
	} else if(!strcmp((char*) pName, "syslogseverity") || !strcmp((char*) pName, "syslogpriority")) {
		pRes = getSeverity(pMsg);
	} else if(!strcmp((char*) pName, "syslogseverity-text") || !strcmp((char*) pName, "syslogpriority-text")) {
		pRes = getSeverityStr(pMsg);
	} else if(!strcmp((char*) pName, "timegenerated")) {
		pRes = getTimeGenerated(pMsg, pTpe->data.field.eDateFormat);
	} else if(!strcmp((char*) pName, "timereported")
		  || !strcmp((char*) pName, "TIMESTAMP")) {
		pRes = getTimeReported(pMsg, pTpe->data.field.eDateFormat);
	} else if(!strcmp((char*) pName, "programname")) {
		pRes = getProgramName(pMsg);
	} else if(!strcmp((char*) pName, "PROTOCOL-VERSION")) {
		pRes = getProtocolVersionString(pMsg);
	} else if(!strcmp((char*) pName, "STRUCTURED-DATA")) {
		pRes = getStructuredData(pMsg);
	} else if(!strcmp((char*) pName, "APP-NAME")) {
		pRes = getAPPNAME(pMsg);
	} else if(!strcmp((char*) pName, "PROCID")) {
		pRes = getPROCID(pMsg);
	} else if(!strcmp((char*) pName, "MSGID")) {
		pRes = getMSGID(pMsg);
	/* here start system properties (those, that do not relate to the message itself */
	} else if(!strcmp((char*) pName, "$NOW")) {
		if((pRes = (char*) getNOW(NOW_NOW)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$YEAR")) {
		if((pRes = (char*) getNOW(NOW_YEAR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$MONTH")) {
		if((pRes = (char*) getNOW(NOW_MONTH)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$DAY")) {
		if((pRes = (char*) getNOW(NOW_DAY)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$HOUR")) {
		if((pRes = (char*) getNOW(NOW_HOUR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$MINUTE")) {
		if((pRes = (char*) getNOW(NOW_MINUTE)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else {
		/* there is no point in continuing, we may even otherwise render the
		 * error message unreadable. rgerhards, 2007-07-10
		 */
		return "**INVALID PROPERTY NAME**";
	}

	/* If we did not receive a template pointer, we are already done... */
	if(pTpe == NULL) {
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
		char *pFld;
		char *pFldEnd;
		/* first, skip to the field in question. The field separator
		 * is always one character and is stored in the template entry.
		 */
		iCurrFld = 1;
		pFld = pRes;
		while(*pFld && iCurrFld < pTpe->data.field.iToPos) {
			/* skip fields until the requested field or end of string is found */
			while(*pFld && (uchar) *pFld != pTpe->data.field.field_delim)
				++pFld; /* skip to field terminator */
			if(*pFld == pTpe->data.field.field_delim) {
				++pFld; /* eat it */
				++iCurrFld;
			}
		}
		dbgprintf("field requested %d, field found %d\n", pTpe->data.field.iToPos, iCurrFld);
		
		if(iCurrFld == pTpe->data.field.iToPos) {
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
			pBufStart = pBuf = malloc((iLen + 1) * sizeof(char));
			if(pBuf == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				*pbMustBeFreed = 0;
				return "**OUT OF MEMORY**";
			}
			/* now copy */
			memcpy(pBuf, pFld, iLen);
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
			return "**FIELD NOT FOUND**";
		}
	} else if(pTpe->data.field.iFromPos != 0 || pTpe->data.field.iToPos != 0) {
		/* we need to obtain a private copy */
		int iFrom, iTo;
		char *pSb;
		iFrom = pTpe->data.field.iFromPos;
		iTo = pTpe->data.field.iToPos;
		/* need to zero-base to and from (they are 1-based!) */
		if(iFrom > 0)
			--iFrom;
		if(iTo > 0)
			--iTo;
		iLen = iTo - iFrom + 1; /* the +1 is for an actual char, NOT \0! */
		pBufStart = pBuf = malloc((iLen + 1) * sizeof(char));
		if(pBuf == NULL) {
			if(*pbMustBeFreed == 1)
				free(pRes);
			*pbMustBeFreed = 0;
			return "**OUT OF MEMORY**";
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
		while(*pSb && iLen) {
			*pBuf++ = *pSb;
			++pSb;
			--iLen;
		}
		*pBuf = '\0';
		if(*pbMustBeFreed == 1)
			free(pRes);
		pRes = pBufStart;
		*pbMustBeFreed = 1;
#ifdef FEATURE_REGEXP
	} else {
		/* Check for regular expressions */
		if (pTpe->data.field.has_regex != 0) {
			if (pTpe->data.field.has_regex == 2)
				/* Could not compile regex before! */
				return
				    "**NO MATCH** **BAD REGULAR EXPRESSION**";

			dbgprintf("debug: String to match for regex is: %s\n",
			        pRes);

			if (0 != regexec(&pTpe->data.field.re, pRes, nmatch,
				    pmatch, 0)) {
				/* we got no match! */
				return "**NO MATCH**";
			} else {
				/* Match! */
				/* I need to malloc pB */
				int iLenBuf;
				char *pB;

				iLenBuf = pmatch[1].rm_eo - pmatch[1].rm_so;
				pB = (char *) malloc((iLenBuf + 1) * sizeof(char));

				if (pB == NULL) {
					if (*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY ALLOCATING pBuf**";
				}

				/* Lets copy the matched substring to the buffer */
				memcpy(pB, pRes + pmatch[1].rm_so, iLenBuf);
				pB[iLenBuf] = '\0';/* terminate string, did not happen before */

				if (*pbMustBeFreed == 1)
					free(pRes);
				pRes = pB;
				*pbMustBeFreed = 1;
			}
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	if(*pRes) {
		/* case conversations (should go after substring, because so we are able to
		 * work on the smallest possible buffer).
		 */
		if(pTpe->data.field.eCaseConv != tplCaseConvNo) {
			/* we need to obtain a private copy */
			int iBufLen = strlen(pRes);
			char *pBStart;
			char *pB;
			char *pSrc;
			pBStart = pB = malloc((iBufLen + 1) * sizeof(char));
			if(pB == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				*pbMustBeFreed = 0;
				return "**OUT OF MEMORY**";
			}
			pSrc = pRes;
			while(*pSrc) {
				*pB++ = (pTpe->data.field.eCaseConv == tplCaseConvUpper) ?
					(char)toupper((int)*pSrc) : (char)tolower((int)*pSrc);
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
			char *pSrc = pRes;
			char *pDstStart;
			char *pDst;
			char bDropped = 0;
			
			while(*pSrc) {
				if(!iscntrl((int) *pSrc++))
					iLenBuf++;
				else
					bDropped = 1;
			}

			if(bDropped) {
				pDst = pDstStart = malloc(iLenBuf + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(!iscntrl((int) *pSrc))
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = pDstStart;
				*pbMustBeFreed = 1;
			}
		} else if(pTpe->data.field.options.bSpaceCC) {
			char *pSrc;
			char *pDstStart;
			char *pDst;
			
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
				pDst = pDstStart = malloc(strlen(pRes) + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
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
			char *pB;

			for(pB = pRes ; *pB ; ++pB) {
				++iLenBuf;
				if(iscntrl((int) *pB))
					++iNumCC;
			}

			if(iNumCC > 0) { /* if 0, there is nothing to escape, so we are done */
				/* OK, let's do the escaping... */
				char *pBStart;
				char szCCEsc[8]; /* buffer for escape sequence */
				int i;

				iLenBuf += iNumCC * 4;
				pBStart = pB = malloc((iLenBuf + 1) * sizeof(char));
				if(pB == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
				}
				while(*pRes) {
					if(iscntrl((int) *pRes)) {
						snprintf(szCCEsc, sizeof(szCCEsc), "#%3.3d", *pRes);
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
			char *pSrc = pRes;
			char *pDstStart;
			char *pDst;
			char bDropped = 0;
			
			while(*pSrc) {
				if(*pSrc++ != '/')
					iLenBuf++;
				else
					bDropped = 1;
			}
			
			if(bDropped) {
				pDst = pDstStart = malloc(iLenBuf + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
				}
				for(pSrc = pRes; *pSrc; pSrc++) {
					if(*pSrc != '/')
						*pDst++ = *pSrc;
				}
				*pDst = '\0';
				if(*pbMustBeFreed == 1)
					free(pRes);
				pRes = pDstStart;
				*pbMustBeFreed = 1;
			}
		} else {
			char *pSrc;
			char *pDstStart;
			char *pDst;
			
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
				pDst = pDstStart = malloc(strlen(pRes) + 1);
				if(pDst == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
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
		if((*pRes == '.') && (*(pRes + 1) == '\0' || (*(pRes + 1) == '.' && *(pRes + 2) == '\0'))) {
			char *pTmp = pRes;

			if(*(pRes + 1) == '\0')
				pRes = "_";
			else
				pRes = "_.";;
			if(*pbMustBeFreed == 1)
				free(pTmp);
			*pbMustBeFreed = 0;
		} else if(*pRes == '\0') {
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = "_";
			*pbMustBeFreed = 0;
		}
	}

	/* Now drop last LF if present (pls note that this must not be done
	 * if bEscapeCC was set!
	 */
	if(pTpe->data.field.options.bDropLastLF && !pTpe->data.field.options.bEscapeCC) {
		int iLn = strlen(pRes);
		char *pB;
		if(iLn > 0 && *(pRes + iLn - 1) == '\n') {
			/* we have a LF! */
			/* check if we need to obtain a private copy */
			if(*pbMustBeFreed == 0) {
				/* ok, original copy, need a private one */
				pB = malloc((iLn + 1) * sizeof(char));
				if(pB == NULL) {
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
				}
				memcpy(pB, pRes, iLn - 1);
				pRes = pB;
				*pbMustBeFreed = 1;
			}
			*(pRes + iLn - 1) = '\0'; /* drop LF ;) */
		}
	}

	/*dbgprintf("MsgGetProp(\"%s\"): \"%s\"\n", pName, pRes); only for verbose debug logging */
	return(pRes);
}


/*
 * vi:set ai:
 */
