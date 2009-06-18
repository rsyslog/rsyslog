/* msg.c
 * The msg object. Implementation of all msg-related functions
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
 * This file is under development and has not yet arrived at being fully
 * self-contained and a real object. So far, it is mostly an excerpt
 * of the "old" message code without any modifications. However, it
 * helps to have things at the right place one we go to the meat of it.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#include "rsyslog.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "template.h"
#include "msg.h"
#include "var.h"
#include "datetime.h"
#include "glbl.h"
#include "regexp.h"
#include "atomic.h"
#include "unicode-helper.h"
#include "ruleset.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(var)
DEFobjCurrIf(datetime)
DEFobjCurrIf(glbl)
DEFobjCurrIf(regexp)

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

/*syslog facility names (as of RFC5424) */
static char *syslog_fac_names[24] = { "kern", "user", "mail", "daemon", "auth", "syslog", "lpr",
			    	      "news", "uucp", "cron", "authpriv", "ftp", "ntp", "audit",
			    	      "alert", "clock", "local0", "local1", "local2", "local3",
			    	      "local4", "local5", "local6", "local7" };

/* table of severity names (in numerical order)*/
static char *syslog_severity_names[8] = { "emerg", "alert", "crit", "err", "warning", "notice", "info", "debug" };

/* numerical values as string - this is the most efficient approach to convert severity
 * and facility values to a numerical string... -- rgerhars, 2009-06-17
 */

static char *syslog_number_names[24] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
					 "15", "16", "17", "18", "19", "20", "21", "22", "23" };

/* some forward declarations */
static int getAPPNAMELen(msg_t *pM);
static int getProtocolVersion(msg_t *pM);

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
 * We now support locked and non-locked operations, depending on
 * the configuration of rsyslog. To support this, we use function
 * pointers. Initially, we start in non-locked mode. There, all
 * locking operations call into dummy functions. When locking is
 * enabled, the function pointers are changed to functions doing
 * actual work. We also introduced another MsgPrepareEnqueue() function
 * which initializes the locking structures, if needed. This is
 * necessary because internal messages during config file startup
 * processing are always created in non-locking mode. So we can
 * not initialize locking structures during constructions. We now
 * postpone this until when the message is fully constructed and
 * enqueued. Then we know the status of locking. This has a nice
 * side effect, and that is that during the initial creation of
 * the Msg object no locking needs to be done, which results in better
 * performance. -- rgerhards, 2008-01-05
 */
static void (*funcLock)(msg_t *pMsg);
static void (*funcUnlock)(msg_t *pMsg);
static void (*funcDeleteMutex)(msg_t *pMsg);
void (*funcMsgPrepareEnqueue)(msg_t *pMsg);
#if 1 /* This is a debug aid */
#define MsgLock(pMsg) 	funcLock(pMsg)
#define MsgUnlock(pMsg) funcUnlock(pMsg)
#else
#define MsgLock(pMsg) 	{dbgprintf("MsgLock line %d\n - ", __LINE__); funcLock(pMsg);; }
#define MsgUnlock(pMsg) {dbgprintf("MsgUnlock line %d - ", __LINE__); funcUnlock(pMsg); }
#endif

/* the next function is a dummy to be used by the looking functions
 * when the class is not yet running in an environment where locking
 * is necessary. Please note that the need to lock can (and will) change
 * during a single run. Typically, this is depending on the operation mode
 * of the message queues (which is operator-configurable). -- rgerhards, 2008-01-05
 */
static void MsgLockingDummy(msg_t __attribute__((unused)) *pMsg)
{
	/* empty be design */
}


/* The following function prepares a message for enqueue into the queue. This is
 * where a message may be accessed by multiple threads. This implementation here
 * is the version for multiple concurrent acces. It initializes the locking
 * structures.
 * TODO: change to an iRet interface! -- rgerhards, 2008-07-14
 */
static void MsgPrepareEnqueueLockingCase(msg_t *pThis)
{
	int iErr;
	BEGINfunc
	assert(pThis != NULL);
	iErr = pthread_mutexattr_init(&pThis->mutAttr);
	if(iErr != 0) {
		dbgprintf("error initializing mutex attribute in %s:%d, trying to continue\n",
		  	  __FILE__, __LINE__);
	}
	iErr = pthread_mutexattr_settype(&pThis->mutAttr, PTHREAD_MUTEX_RECURSIVE);
	if(iErr != 0) {
		dbgprintf("ERROR setting mutex attribute to recursive in %s:%d, trying to continue "
			 "but we will probably either abort or hang soon\n",
		  	  __FILE__, __LINE__);
		/* TODO: it makes very little sense to continue here,
		 * but it requires an iRet interface to gracefully shut
		 * down. We should do that over time. -- rgerhards, 2008-07-14
		 */
	}
	pthread_mutex_init(&pThis->mut, &pThis->mutAttr);

	/* we do no longer need the attribute. According to the
	 * POSIX spec, we can destroy it without affecting the
	 * initialized mutex (that used the attribute).
	 * rgerhards, 2008-07-14
	 */
	pthread_mutexattr_destroy(&pThis->mutAttr);
	pThis->bDoLock = 1;
	ENDfunc
}


/* ... and now the locking and unlocking implementations: */
static void MsgLockLockingCase(msg_t *pThis)
{
	/* DEV debug only! dbgprintf("MsgLock(0x%lx)\n", (unsigned long) pThis); */
	assert(pThis != NULL);
	if(pThis->bDoLock == 1) /* TODO: this is a testing hack, we should find a way with better performance! -- rgerhards, 2009-01-27 */
		pthread_mutex_lock(&pThis->mut);
}

static void MsgUnlockLockingCase(msg_t *pThis)
{
	/* DEV debug only! dbgprintf("MsgUnlock(0x%lx)\n", (unsigned long) pThis); */
	assert(pThis != NULL);
	if(pThis->bDoLock == 1) /* TODO: this is a testing hack, we should find a way with better performance! -- rgerhards, 2009-01-27 */
		pthread_mutex_unlock(&pThis->mut);
}

/* delete the mutex object on message destruction (locking case)
 */
static void MsgDeleteMutexLockingCase(msg_t *pThis)
{
	assert(pThis != NULL);
	pthread_mutex_destroy(&pThis->mut);
}

/* enable multiple concurrent access on the message object
 * This works on a class-wide basis and can bot be undone.
 * That is, if it is once enabled, it can not be disabled during
 * the same run. When this function is called, no other thread
 * must manipulate message objects. Then we would have race conditions,
 * but guarding against this is counter-productive because it
 * would cost additional time. Plus, it would be a programming error.
 * rgerhards, 2008-01-05
 */
rsRetVal MsgEnableThreadSafety(void)
{
	DEFiRet;
	funcLock = MsgLockLockingCase;
	funcUnlock = MsgUnlockLockingCase;
	funcMsgPrepareEnqueue = MsgPrepareEnqueueLockingCase;
	funcDeleteMutex = MsgDeleteMutexLockingCase;
	RETiRet;
}

/* end locking functions */


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
 * rgerhards, 2008-10-06
 */
static inline rsRetVal msgBaseConstruct(msg_t **ppThis)
{
	DEFiRet;
	msg_t *pM;

	assert(ppThis != NULL);
	if((pM = calloc(1, sizeof(msg_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	/* initialize members that are non-zero */
	pM->iRefCount = 1;
	pM->iSeverity = -1;
	pM->iFacility = -1;
	objConstructSetObjInfo(pM);

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


/* some free handlers for (slightly) complicated cases... All of them may be called
 * with an empty element.
 */
static inline void freeTAG(msg_t *pThis)
{
	if(pThis->iLenTAG >= CONF_TAG_BUFSIZE)
		free(pThis->TAG.pszTAG);
}


BEGINobjDestruct(msg) /* be sure to specify the object type also in END and CODESTART macros! */
	int currRefCount;
CODESTARTobjDestruct(msg)
	/* DEV Debugging only ! dbgprintf("msgDestruct\t0x%lx, Ref now: %d\n", (unsigned long)pThis, pThis->iRefCount - 1); */
#	ifdef HAVE_ATOMIC_BUILTINS
		currRefCount = ATOMIC_DEC_AND_FETCH(pThis->iRefCount);
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
		free(pThis->pszHOSTNAME);
		free(pThis->pszInputName);
		free(pThis->pszRcvFrom);
		free(pThis->pszRcvFromIP);
		free(pThis->pszMSG);
		free(pThis->pszRcvdAt3164);
		free(pThis->pszRcvdAt3339);
		free(pThis->pszRcvdAt_SecFrac);
		free(pThis->pszRcvdAt_MySQL);
		free(pThis->pszRcvdAt_PgSQL);
		free(pThis->pszTIMESTAMP_SecFrac);
		free(pThis->pszTIMESTAMP_MySQL);
		free(pThis->pszTIMESTAMP_PgSQL);
		if(pThis->pCSProgName != NULL)
			rsCStrDestruct(&pThis->pCSProgName);
		if(pThis->pCSStrucData != NULL)
			rsCStrDestruct(&pThis->pCSStrucData);
		if(pThis->pCSAPPNAME != NULL)
			rsCStrDestruct(&pThis->pCSAPPNAME);
		if(pThis->pCSPROCID != NULL)
			rsCStrDestruct(&pThis->pCSPROCID);
		if(pThis->pCSMSGID != NULL)
			rsCStrDestruct(&pThis->pCSMSGID);
#	ifndef HAVE_ATOMIC_BUILTINS
		MsgUnlock(pThis);
# 	endif
		funcDeleteMutex(pThis);
	} else {
		MsgUnlock(pThis);
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

	assert(pOld != NULL);

	BEGINfunc
	if(msgConstructWithTime(&pNew, &pOld->tTIMESTAMP, pOld->ttGenTime) != RS_RET_OK) {
		return NULL;
	}

	/* now copy the message properties */
	pNew->iRefCount = 1;
	pNew->iSeverity = pOld->iSeverity;
	pNew->iFacility = pOld->iFacility;
	pNew->bParseHOSTNAME = pOld->bParseHOSTNAME;
	pNew->msgFlags = pOld->msgFlags;
	pNew->iProtocolVersion = pOld->iProtocolVersion;
	pNew->ttGenTime = pOld->ttGenTime;
	/* enable this, if someone actually uses UxTradMsg, delete after some  time has
	 * passed and nobody complained -- rgerhards, 2009-06-16
	pNew->offAfterPRI = pOld->offAfterPRI;
	*/
	if(pOld->iLenTAG > 0) {
		if(pOld->iLenTAG < CONF_TAG_BUFSIZE) {
			memcpy(pNew->TAG.szBuf, pOld->TAG.szBuf, pOld->iLenTAG);
		} else {
			if((pNew->TAG.pszTAG = srUtilStrDup(pOld->TAG.pszTAG, pOld->iLenTAG)) == NULL) {
				msgDestruct(&pNew);
				return NULL;
			}
			pNew->iLenTAG = pOld->iLenTAG;
		}
	}
	tmpCOPYSZ(RawMsg);
	tmpCOPYSZ(MSG);
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
 * we do not serialize bParseHOSTNAME, as this is only a helper variable
 * during msg construction - and never again used later.
 * rgerhards, 2008-01-03
 */
static rsRetVal MsgSerialize(msg_t *pThis, strm_t *pStrm)
{
	DEFiRet;

	assert(pThis != NULL);
	assert(pStrm != NULL);

	CHKiRet(obj.BeginSerialize(pStrm, (obj_t*) pThis));
	objSerializeSCALAR(pStrm, iProtocolVersion, SHORT);
	objSerializeSCALAR(pStrm, iSeverity, SHORT);
	objSerializeSCALAR(pStrm, iFacility, SHORT);
	objSerializeSCALAR(pStrm, msgFlags, INT);
	objSerializeSCALAR(pStrm, ttGenTime, INT);
	objSerializeSCALAR(pStrm, tRcvdAt, SYSLOGTIME);
	objSerializeSCALAR(pStrm, tTIMESTAMP, SYSLOGTIME);
	/* enable this, if someone actually uses UxTradMsg, delete after some  time has
	 * passed and nobody complained -- rgerhards, 2009-06-16
	objSerializeSCALAR(pStrm, offsAfterPRI, SHORT);
	*/

	objSerializePTR(pStrm, pszRawMsg, PSZ);
	objSerializePTR(pStrm, pszMSG, PSZ);

	CHKiRet(obj.SerializeProp(pStrm, UCHAR_CONSTANT("pszTAG"), PROPTYPE_PSZ, (void*)
		((pThis->iLenTAG < CONF_TAG_BUFSIZE) ? pThis->TAG.szBuf : pThis->TAG.pszTAG)));

	objSerializePTR(pStrm, pszHOSTNAME, PSZ);
	objSerializePTR(pStrm, pszInputName, PSZ);
	objSerializePTR(pStrm, pszRcvFrom, PSZ);
	objSerializePTR(pStrm, pszRcvFromIP, PSZ);

	objSerializePTR(pStrm, pCSStrucData, CSTR);
	objSerializePTR(pStrm, pCSAPPNAME, CSTR);
	objSerializePTR(pStrm, pCSPROCID, CSTR);
	objSerializePTR(pStrm, pCSMSGID, CSTR);

	CHKiRet(obj.EndSerialize(pStrm));

finalize_it:
	RETiRet;
}


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
		ATOMIC_INC(pM->iRefCount);
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
	rsCStrSetAllocIncrement(pM->pCSPROCID, 16);
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
 * If it is needed, this function should be called first. It checks if it is
 * already set and extracts it, if not.
 * A message object must be provided, else a crash will occur.
 * rgerhards, 2005-10-19
 */
static rsRetVal aquireProgramName(msg_t *pM)
{
	register int i;
	uchar *pszTag;
	DEFiRet;

	assert(pM != NULL);
	if(pM->pCSProgName == NULL) {
		/* ok, we do not yet have it. So let's parse the TAG
		 * to obtain it.
		 */
		pszTag = (uchar*) ((pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG);
		CHKiRet(cstrConstruct(&pM->pCSProgName));
		rsCStrSetAllocIncrement(pM->pCSProgName, 33);
		for(  i = 0
		    ; (i < pM->iLenTAG) && isprint((int) pszTag[i])
		      && (pszTag[i] != '\0') && (pszTag[i] != ':')
		      && (pszTag[i] != '[')  && (pszTag[i] != '/')
		    ; ++i) {
			CHKiRet(cstrAppendChar(pM->pCSProgName, pszTag[i]));
		}
		CHKiRet(cstrFinalize(pM->pCSProgName));
	}
finalize_it:
	RETiRet;
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
 * NOTE ********* 2009-06-18 / rgerhards *************
 * This function is being obsoleted by the new handling. I keep it for
 * a while, and for oversize tags it is somewhat less optimal than in previous
 * versions. This should only happen very seldom.
 */
void moveHOSTNAMEtoTAG(msg_t *pM)
{
	assert(pM != NULL);
	MsgSetTAG(pM, pM->pszHOSTNAME, pM->iLenHOSTNAME);
	free(pM->pszHOSTNAME);
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

static int getProtocolVersion(msg_t *pM)
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


static char *getRawMsg(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszRawMsg == NULL)
			return "";
		else
			return (char*)pM->pszRawMsg;
}


/* enable this, if someone actually uses UxTradMsg, delete after some  time has
 * passed and nobody complained -- rgerhards, 2009-06-16
char *getUxTradMsg(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else
		return (char*)pM->pszRawMsg + pM->offAfterPRI;
}
*/


int getMSGLen(msg_t *pM)
{
	return((pM == NULL) ? 0 : pM->iLenMSG);
}

char *getMSG(msg_t *pM)
{
	if(pM == NULL)
		return "";
	else {
		if(pM->pszMSG == NULL)
			return "";
		else
			return (char*)pM->pszMSG;
	}
}


/* Get PRI value as integer */
static int getPRIi(msg_t *pM)
{
	return (pM->iFacility << 3) + (pM->iSeverity);
}


/* Get PRI value in text form
 */
static inline char *getPRI(msg_t *pM)
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


static inline char *getTimeReported(msg_t *pM, enum tplFormatTypes eFmt)
{
	BEGINfunc
	if(pM == NULL)
		return "";

	switch(eFmt) {
	case tplFmtDefault:
	case tplFmtRFC3164Date:
		MsgLock(pM);
		if(pM->pszTIMESTAMP3164 == NULL) {
			pM->pszTIMESTAMP3164 = pM->pszTimestamp3164;
			datetime.formatTimestamp3164(&pM->tTIMESTAMP, pM->pszTIMESTAMP3164, 16);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP3164);
	case tplFmtMySQLDate:
		MsgLock(pM);
		if(pM->pszTIMESTAMP_MySQL == NULL) {
			if((pM->pszTIMESTAMP_MySQL = malloc(15)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestampToMySQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_MySQL, 15);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP_MySQL);
        case tplFmtPgSQLDate:
                MsgLock(pM);
                if(pM->pszTIMESTAMP_PgSQL == NULL) {
                        if((pM->pszTIMESTAMP_PgSQL = malloc(21)) == NULL) {
                                MsgUnlock(pM);
                                return "";
                        }
                        datetime.formatTimestampToPgSQL(&pM->tTIMESTAMP, pM->pszTIMESTAMP_PgSQL, 21);
                }
                MsgUnlock(pM);
                return(pM->pszTIMESTAMP_PgSQL);
	case tplFmtRFC3339Date:
		MsgLock(pM);
		if(pM->pszTIMESTAMP3339 == NULL) {
			pM->pszTIMESTAMP3339 = pM->pszTimestamp3339;
			datetime.formatTimestamp3339(&pM->tTIMESTAMP, pM->pszTIMESTAMP3339, 33);
		}
		MsgUnlock(pM);
		return(pM->pszTIMESTAMP3339);
	case tplFmtSecFrac:
		MsgLock(pM);
		if(pM->pszTIMESTAMP_SecFrac == NULL) {
			if((pM->pszTIMESTAMP_SecFrac = malloc(10)) == NULL) {
				MsgUnlock(pM);
				return ""; /* TODO: check this: can it cause a free() of constant memory?) */
			}
			datetime.formatTimestampSecFrac(&pM->tTIMESTAMP, pM->pszTIMESTAMP_SecFrac, 10);
		}
		MsgUnlock(pM);
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
			if((pM->pszRcvdAt3164 = malloc(16)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3164);
	case tplFmtMySQLDate:
		MsgLock(pM);
		if(pM->pszRcvdAt_MySQL == NULL) {
			if((pM->pszRcvdAt_MySQL = malloc(15)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestampToMySQL(&pM->tRcvdAt, pM->pszRcvdAt_MySQL, 15);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt_MySQL);
        case tplFmtPgSQLDate:
                MsgLock(pM);
                if(pM->pszRcvdAt_PgSQL == NULL) {
                        if((pM->pszRcvdAt_PgSQL = malloc(21)) == NULL) {
                                MsgUnlock(pM);
                                return "";
                        }
                        datetime.formatTimestampToPgSQL(&pM->tRcvdAt, pM->pszRcvdAt_PgSQL, 21);
                }
                MsgUnlock(pM);
                return(pM->pszRcvdAt_PgSQL);
	case tplFmtRFC3164Date:
		MsgLock(pM);
		if(pM->pszRcvdAt3164 == NULL) {
			if((pM->pszRcvdAt3164 = malloc(16)) == NULL) {
					MsgUnlock(pM);
					return "";
				}
			datetime.formatTimestamp3164(&pM->tRcvdAt, pM->pszRcvdAt3164, 16);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3164);
	case tplFmtRFC3339Date:
		MsgLock(pM);
		if(pM->pszRcvdAt3339 == NULL) {
			if((pM->pszRcvdAt3339 = malloc(33)) == NULL) {
				MsgUnlock(pM);
				return "";
			}
			datetime.formatTimestamp3339(&pM->tRcvdAt, pM->pszRcvdAt3339, 33);
		}
		MsgUnlock(pM);
		return(pM->pszRcvdAt3339);
	case tplFmtSecFrac:
		MsgLock(pM);
		if(pM->pszRcvdAt_SecFrac == NULL) {
			if((pM->pszRcvdAt_SecFrac = malloc(10)) == NULL) {
				MsgUnlock(pM);
				return ""; /* TODO: check this: can it cause a free() of constant memory?) */
			}
			datetime.formatTimestampSecFrac(&pM->tRcvdAt, pM->pszRcvdAt_SecFrac, 10);
		}
		MsgUnlock(pM);
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
 * TODO: revisit msg locking code!
 */
rsRetVal MsgSetAPPNAME(msg_t *pMsg, char* pszAPPNAME)
{
	DEFiRet;
	assert(pMsg != NULL);
	if(pMsg->pCSAPPNAME == NULL) {
		/* we need to obtain the object first */
		CHKiRet(rsCStrConstruct(&pMsg->pCSAPPNAME));
		rsCStrSetAllocIncrement(pMsg->pCSAPPNAME, 128);
	}
	/* if we reach this point, we have the object */
	iRet = rsCStrSetSzStr(pMsg->pCSAPPNAME, (uchar*) pszAPPNAME);

finalize_it:
	RETiRet;
}


static void tryEmulateAPPNAME(msg_t *pM); /* forward reference */
/* rgerhards, 2005-11-24
 */
char *getAPPNAME(msg_t *pM)
{
	assert(pM != NULL);
	MsgLock(pM);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	MsgUnlock(pM);
	return (pM->pCSAPPNAME == NULL) ? "" : (char*) rsCStrGetSzStrNoNULL(pM->pCSAPPNAME);
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
	iRet = rsCStrSetSzStr(pMsg->pCSPROCID, (uchar*) pszPROCID);
	CHKiRet(cstrFinalize(pMsg->pCSPROCID));

finalize_it:
	RETiRet;
}

/* rgerhards, 2005-11-24
 */
static inline int getPROCIDLen(msg_t *pM)
{
	assert(pM != NULL);
	MsgLock(pM);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	MsgUnlock(pM);
	return (pM->pCSPROCID == NULL) ? 1 : rsCStrLen(pM->pCSPROCID);
}


/* rgerhards, 2005-11-24
 */
char *getPROCID(msg_t *pM)
{
	char* pszRet;

	ISOBJ_TYPE_assert(pM, msg);
	MsgLock(pM);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	pszRet = (pM->pCSPROCID == NULL) ? "-" : (char*) cstrGetSzStrNoNULL(pM->pCSPROCID);
	//pszRet = (pM->pCSPROCID == NULL) ? "-" : (char*) rsCStrGetSzStrNoNULL(pM->pCSPROCID);
	MsgUnlock(pM);
	return pszRet;
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
		rsCStrSetAllocIncrement(pMsg->pCSMSGID, 128);
	}
	/* if we reach this point, we have the object */
	iRet = rsCStrSetSzStr(pMsg->pCSMSGID, (uchar*) pszMSGID);

finalize_it:
	RETiRet;
}


/* rgerhards, 2005-11-24
 */
static inline char *getMSGID(msg_t *pM)
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
	MsgSetTAG(pMsg, pBuf, ustrlen(pBuf));
	free(pBuf);
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
	if(pMsg->iLenTAG < CONF_RAWMSG_BUFSIZE) {
		/* small enough: use fixed buffer (faster!) */
		pBuf = pMsg->TAG.szBuf;
	} else if((pBuf = (uchar*) malloc(pMsg->iLenTAG + 1)) == NULL) {
		/* truncate message, better than completely loosing it... */
		pBuf = pMsg->TAG.szBuf;
		pMsg->iLenTAG = CONF_RAWMSG_BUFSIZE - 1;
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
static void tryEmulateTAG(msg_t *pM)
{
	int iTAGLen;
	uchar *pBuf;
	assert(pM != NULL);

	if(pM->iLenTAG > 0)
		return; /* done, no need to emulate */
	
	if(getProtocolVersion(pM) == 1) {
		if(!strcmp(getPROCID(pM), "-")) {
			/* no process ID, use APP-NAME only */
			MsgSetTAG(pM, (uchar*) getAPPNAME(pM), getAPPNAMELen(pM));
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


static inline char *getTAG(msg_t *pM)
{
	char *ret;

	if(pM == NULL)
		ret = "";
	else {
		MsgLock(pM);
		tryEmulateTAG(pM);
		if(pM->iLenTAG == 0)
			ret = "";
		else
			ret = (char*) ((pM->iLenTAG < CONF_TAG_BUFSIZE) ? pM->TAG.szBuf : pM->TAG.pszTAG);
		MsgUnlock(pM);
	}
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


static uchar *getInputName(msg_t *pM)
{
	if(pM == NULL)
		return (uchar*) "";
	else
		if(pM->pszInputName == NULL)
			return (uchar*) "";
		else
			return pM->pszInputName;
}


uchar *getRcvFrom(msg_t *pM)
{
	if(pM == NULL)
		return UCHAR_CONSTANT("");
	else
		if(pM->pszRcvFrom == NULL)
			return UCHAR_CONSTANT("");
		else
			return pM->pszRcvFrom;
}


uchar *getRcvFromIP(msg_t *pM)
{
	if(pM == NULL)
		return (uchar*) "";
	else
		if(pM->pszRcvFromIP == NULL)
			return (uchar*) "";
		else
			return pM->pszRcvFromIP;
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
		rsCStrSetAllocIncrement(pMsg->pCSStrucData, 128);
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
	return (pM->pCSStrucData == NULL) ? "-" : (char*) rsCStrGetSzStrNoNULL(pM->pCSStrucData);
}



/* get the length of the "programname" sz string
 * rgerhards, 2005-10-19
 */
int getProgramNameLen(msg_t *pM)
{
	int iRet;

	assert(pM != NULL);
	MsgLock(pM);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dbgprintf("error %d returned by aquireProgramName() in getProgramNameLen()\n", iRet);
		MsgUnlock(pM);
		return 0; /* best we can do (consistent wiht what getProgramName() returns) */
	}
	MsgUnlock(pM);

	return (pM->pCSProgName == NULL) ? 0 : rsCStrLen(pM->pCSProgName);
}


/* get the "programname" as sz string
 * rgerhards, 2005-10-19
 */
char *getProgramName(msg_t *pM) /* this is the non-locking version for internal use */
{
	int iRet;
	char *pszRet;

	assert(pM != NULL);
	MsgLock(pM);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dbgprintf("error %d returned by aquireProgramName() in getProgramName()\n", iRet);
		pszRet = ""; /* best we can do */
	} else {
		pszRet = (pM->pCSProgName == NULL) ? "" : (char*) rsCStrGetSzStrNoNULL(pM->pCSProgName);
	}

	MsgUnlock(pM);
	return pszRet;
}
/* The code below was an approach without PTHREAD_MUTEX_RECURSIVE
 * However, it turned out to be quite complex. So far, we use recursive
 * locking, which is OK from a performance point of view, especially as
 * we do not anticipate that multithreading msg objects is used often.
 * However, we may re-think about using non-recursive locking and I leave this
 * code in here to conserve the idea. -- rgerhards, 2008-01-05
 */
#if 0
static char *getProgramNameNoLock(msg_t *pM) /* this is the non-locking version for internal use */
{
	int iRet;

	assert(pM != NULL);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dbgprintf("error %d returned by aquireProgramName() in getProgramName()\n", iRet);
		return ""; /* best we can do */
	}

	return (pM->pCSProgName == NULL) ? "" : (char*) rsCStrGetSzStrNoNULL(pM->pCSProgName);
}
char *getProgramName(msg_t *pM) /* this is the external callable version */
{
	char *pszRet;

	MsgLock(pM);
	pszRet = getProgramNameNoLock(pM);
	MsgUnlock(pM);
	return pszRet;
}
/* an alternative approach has been: */
/* The macro below is used to generate external function definitions
 * for such functions that may also be called internally (and thus have
 * both a locking and non-locking implementation. Over time, we could
 * reconsider how we handle that. -- rgerhards, 2008-01-05
 */
#define EXT_LOCKED_FUNC(fName, ret) \
ret fName(msg_t *pM) \
{ \
	ret valRet; \
	MsgLock(pM); \
	valRet = fName##NoLock(pM); \
	MsgUnlock(pM); \
	return(valRet); \
}
EXT_LOCKED_FUNC(getProgramName, char*)
/* in this approach, the external function is provided by the macro and
 * needs not to be writen.
 */
#endif /* #if 0 -- saved code */


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
static int getAPPNAMELen(msg_t *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	return (pM->pCSAPPNAME == NULL) ? 0 : rsCStrLen(pM->pCSAPPNAME);
}

/* rgerhards 2008-09-10: set pszInputName in msg object
 * rgerhards, 2009-06-16
 */
void MsgSetInputName(msg_t *pMsg, uchar* pszInputName, size_t lenInputName)
{
	assert(pMsg != NULL);
	free(pMsg->pszInputName);
	pMsg->iLenInputName = lenInputName;
	if((pMsg->pszInputName = malloc(pMsg->iLenInputName + 1)) != NULL) {
		memcpy(pMsg->pszInputName, pszInputName, pMsg->iLenInputName + 1);
	}
}

/* rgerhards 2004-11-16: set pszRcvFrom in msg object
 */
void MsgSetRcvFrom(msg_t *pMsg, uchar* pszRcvFrom)
{
	assert(pMsg != NULL);
	free(pMsg->pszRcvFrom);

	pMsg->iLenRcvFrom = ustrlen(pszRcvFrom);
	if((pMsg->pszRcvFrom = malloc(pMsg->iLenRcvFrom + 1)) != NULL) {
		memcpy(pMsg->pszRcvFrom, pszRcvFrom, pMsg->iLenRcvFrom + 1);
	}
}


/* rgerhards 2005-05-16: set pszRcvFromIP in msg object */
rsRetVal
MsgSetRcvFromIP(msg_t *pMsg, uchar* pszRcvFromIP)
{
	DEFiRet;
	assert(pMsg != NULL);
	if(pMsg->pszRcvFromIP != NULL) {
		free(pMsg->pszRcvFromIP);
		pMsg->iLenRcvFromIP = 0;
	}

	CHKmalloc(pMsg->pszRcvFromIP = (uchar*)strdup((char*)pszRcvFromIP));
	pMsg->iLenRcvFromIP = strlen((char*)pszRcvFromIP);
finalize_it:
	RETiRet;
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
	if(pMsg->pszHOSTNAME != NULL)
		free(pMsg->pszHOSTNAME);
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
void MsgSetHOSTNAME(msg_t *pMsg, uchar* pszHOSTNAME)
{
	assert(pMsg != NULL);
	free(pMsg->pszHOSTNAME);

	pMsg->iLenHOSTNAME = ustrlen(pszHOSTNAME);
	if((pMsg->pszHOSTNAME = malloc(pMsg->iLenHOSTNAME + 1)) != NULL)
		memcpy(pMsg->pszHOSTNAME, pszHOSTNAME, pMsg->iLenHOSTNAME + 1);
	else
		DBGPRINTF("Could not allocate memory in MsgSetHOSTNAME()\n");
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

/* set raw message in message object. Size of message is provided.
 * rgerhards, 2009-06-16
 */
void MsgSetRawMsg(msg_t *pMsg, char* pszRawMsg, size_t lenMsg)
{
	assert(pMsg != NULL);
	if(pMsg->pszRawMsg != pMsg->szRawMsg)
		free(pMsg->pszRawMsg);

	pMsg->iLenRawMsg = lenMsg;
	if(pMsg->iLenRawMsg < CONF_RAWMSG_BUFSIZE) {
		/* small enough: use fixed buffer (faster!) */
		pMsg->pszRawMsg = pMsg->szRawMsg;
	} else if((pMsg->pszRawMsg = (uchar*) malloc(pMsg->iLenRawMsg + 1)) == NULL) {
		/* truncate message, better than completely loosing it... */
		pMsg->pszRawMsg = pMsg->szRawMsg;
		pMsg->iLenRawMsg = CONF_RAWMSG_BUFSIZE - 1;
	}

	memcpy(pMsg->pszRawMsg, pszRawMsg, pMsg->iLenRawMsg);
	pMsg->pszRawMsg[pMsg->iLenRawMsg] = '\0'; /* this also works with truncation! */
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
 * The variable pRes must point to a user-supplied buffer and
 * pResLen must contain its size. The pointer to the buffer
 * is also returned, what makes this functiona suitable for
 * use in printf-like functions.
 * Note: a buffer size of 20 characters is always sufficient.
 * Interface to this function changed 2007-06-15 by RGerhards
 */
char *textpri(char *pRes, size_t pResLen, int pri)
{
	assert(pRes != NULL);
	assert(pResLen > 0);

	snprintf(pRes, pResLen, "%s.%s<%d>", syslog_fac_names[LOG_FAC(pri)],
		 syslog_severity_names[LOG_PRI(pri)], pri);

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
static uchar *getNOW(eNOWType eNow)
{
	uchar *pBuf;
	struct syslogTime t;

	if((pBuf = (uchar*) malloc(sizeof(uchar) * tmpBUFSIZE)) == NULL) {
		return NULL;
	}

	datetime.getCurrTime(&t, NULL);
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
	case NOW_HHOUR:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.minute / 30);
		break;
	case NOW_QHOUR:
		snprintf((char*) pBuf, tmpBUFSIZE, "%2.2d", t.minute / 15);
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
                 cstr_t *pCSPropName, unsigned short *pbMustBeFreed)
{
	uchar *pName;
	char *pRes; /* result pointer */
	int bufLen = -1; /* length of string or -1, if not known */
	char *pBufStart;
	char *pBuf;
	int iLen;
	short iOffs;

#ifdef	FEATURE_REGEXP
	/* Variables necessary for regular expression matching */
	size_t nmatch = 10;
	regmatch_t pmatch[10];
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
		bufLen = getMSGLen(pMsg);
	} else if(!strcmp((char*) pName, "timestamp")
		  || !strcmp((char*) pName, "timereported")) {
		pRes = getTimeReported(pMsg, pTpe->data.field.eDateFormat);
	} else if(!strcmp((char*) pName, "hostname") || !strcmp((char*) pName, "source")) {
		pRes = getHOSTNAME(pMsg);
	} else if(!strcmp((char*) pName, "syslogtag")) {
		pRes = getTAG(pMsg);
	} else if(!strcmp((char*) pName, "rawmsg")) {
		pRes = getRawMsg(pMsg);
	/* enable this, if someone actually uses UxTradMsg, delete after some  time has
	 * passed and nobody complained -- rgerhards, 2009-06-16
	} else if(!strcmp((char*) pName, "uxtradmsg")) {
		pRes = getUxTradMsg(pMsg);
	*/
	} else if(!strcmp((char*) pName, "inputname")) {
		pRes = (char*) getInputName(pMsg);
	} else if(!strcmp((char*) pName, "fromhost")) {
		pRes = (char*) getRcvFrom(pMsg);
	} else if(!strcmp((char*) pName, "fromhost-ip")) {
		pRes = (char*) getRcvFromIP(pMsg);
	} else if(!strcmp((char*) pName, "pri")) {
		pRes = getPRI(pMsg);
	} else if(!strcmp((char*) pName, "pri-text")) {
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
	} else if(!strcmp((char*) pName, "programname")) {
		pRes = getProgramName(pMsg);
	} else if(!strcmp((char*) pName, "protocol-version")) {
		pRes = getProtocolVersionString(pMsg);
	} else if(!strcmp((char*) pName, "structured-data")) {
		pRes = getStructuredData(pMsg);
	} else if(!strcmp((char*) pName, "app-name")) {
		pRes = getAPPNAME(pMsg);
	} else if(!strcmp((char*) pName, "procid")) {
		pRes = getPROCID(pMsg);
	} else if(!strcmp((char*) pName, "msgid")) {
		pRes = getMSGID(pMsg);
	/* here start system properties (those, that do not relate to the message itself */
	} else if(!strcmp((char*) pName, "$now")) {
		if((pRes = (char*) getNOW(NOW_NOW)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$year")) {
		if((pRes = (char*) getNOW(NOW_YEAR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$month")) {
		if((pRes = (char*) getNOW(NOW_MONTH)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$day")) {
		if((pRes = (char*) getNOW(NOW_DAY)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$hour")) {
		if((pRes = (char*) getNOW(NOW_HOUR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$hhour")) {
		if((pRes = (char*) getNOW(NOW_HHOUR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$qhour")) {
		if((pRes = (char*) getNOW(NOW_QHOUR)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$minute")) {
		if((pRes = (char*) getNOW(NOW_MINUTE)) == NULL) {
			return "***OUT OF MEMORY***";
		} else
			*pbMustBeFreed = 1;	/* all of these functions allocate dyn. memory */
	} else if(!strcmp((char*) pName, "$myhostname")) {
		pRes = (char*) glbl.GetLocalHostName();
	} else {
		/* there is no point in continuing, we may even otherwise render the
		 * error message unreadable. rgerhards, 2007-07-10
		 */
		dbgprintf("invalid property name: '%s'\n", pName);
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
				if (pTpe->data.field.field_expand != 0) {
					while (*pFld == pTpe->data.field.field_delim) {
						++pFld;
					}
				}
				++iCurrFld;
			}
		}
		dbgprintf("field requested %d, field found %d\n", pTpe->data.field.iToPos, (int) iCurrFld);
		
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
		bufLen = iLen;
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
				return "**NO MATCH** **BAD REGULAR EXPRESSION**";

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
					iREstat = regexp.regexec(&pTpe->data.field.re, pRes + iOffs, nmatch, pmatch, 0);
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
								  iOffs, iOffs + pmatch[0].rm_eo, iTry);
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
						if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_DFLTSTR)
							return "**NO MATCH**";
						else if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_ZERO)
							return "0";
						else
							return "";
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
							if(pTpe->data.field.nomatchAction == TPL_REGEX_NOMATCH_USE_DFLTSTR)
								return "**NO MATCH**";
							else
								return "";
						}
					}
					/* OK, we have a usable match - we now need to malloc pB */
					int iLenBuf;
					char *pB;

					iLenBuf = pmatch[pTpe->data.field.iSubMatchToUse].rm_eo
						  - pmatch[pTpe->data.field.iSubMatchToUse].rm_so;
					pB = (char *) malloc((iLenBuf + 1) * sizeof(char));

					if (pB == NULL) {
						if (*pbMustBeFreed == 1)
							free(pRes);
						*pbMustBeFreed = 0;
						return "**OUT OF MEMORY ALLOCATING pBuf**";
					}

					/* Lets copy the matched substring to the buffer */
					memcpy(pB, pRes + iOffs +  pmatch[pTpe->data.field.iSubMatchToUse].rm_so, iLenBuf);
					bufLen = iLenBuf - 1;
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
				return "***REGEXP NOT AVAILABLE***";
			}
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	/* now check if we need to do our "SP if first char is non-space" hack logic */
	if(*pRes && pTpe->data.field.options.bSPIffNo1stSP) {
		/* here, we always destruct the buffer and return a new one */
		uchar cFirst = *pRes; /* save first char */
		if(*pbMustBeFreed == 1)
			free(pRes);
		pRes = (cFirst == ' ') ? "" : " ";
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
				bufLen = strlen(pRes);
			char *pBStart;
			char *pB;
			char *pSrc;
			pBStart = pB = malloc((bufLen + 1) * sizeof(char));
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
				bufLen = iLenBuf;
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
				if(bufLen == -1)
					bufLen = strlen(pRes);
				pDst = pDstStart = malloc(bufLen + 1);
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
				bufLen = -1; /* TODO: can we do better? */
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
				if(bufLen == -1)
					bufLen = strlen(pRes);
				pDst = pDstStart = malloc(bufLen + 1);
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
			bufLen = 1;
			*pbMustBeFreed = 0;
		}
	}

	/* Now drop last LF if present (pls note that this must not be done
	 * if bEscapeCC was set)!
	 */
	if(pTpe->data.field.options.bDropLastLF && !pTpe->data.field.options.bEscapeCC) {
		int iLn;
		char *pB;
		if(bufLen == -1)
			bufLen = strlen(pRes);
		iLn = bufLen;
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
			--bufLen;
		}
	}

	/* finally, we need to check if the property should be formatted in CSV
	 * format (we use RFC 4180, and always use double quotes). As of this writing,
	 * this should be the last action carried out on the property, but in the
	 * future there may be reasons to change that. -- rgerhards, 2009-04-02
	 */
	if(pTpe->data.field.options.bCSV) {
		/* we need to obtain a private copy, as we need to at least add the double quotes */
		int iBufLen;
		char *pBStart;
		char *pDst;
		char *pSrc;
		if(bufLen == -1)
			bufLen = strlen(pRes);
		iBufLen = bufLen;
		/* the malloc may be optimized, we currently use the worst case... */
		pBStart = pDst = malloc((2 * iBufLen + 3) * sizeof(char));
		if(pDst == NULL) {
			if(*pbMustBeFreed == 1)
				free(pRes);
			*pbMustBeFreed = 0;
			return "**OUT OF MEMORY**";
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
	}

	/*dbgprintf("MsgGetProp(\"%s\"): \"%s\"\n", pName, pRes); only for verbose debug logging */
	return(pRes);
}


/* The returns a message variable suitable for use with RainerScript. Most importantly, this means
 * that the value is returned in a var_t object. The var_t is constructed inside this function and
 * MUST be freed by the caller.
 * rgerhards, 2008-02-25
 */
rsRetVal
msgGetMsgVar(msg_t *pThis, cstr_t *pstrPropName, var_t **ppVar)
{
	DEFiRet;
	var_t *pVar;
	uchar *pszProp = NULL;
	cstr_t *pstrProp;
	unsigned short bMustBeFreed = 0;

	ISOBJ_TYPE_assert(pThis, msg);
	ASSERT(pstrPropName != NULL);
	ASSERT(ppVar != NULL);

	/* make sure we have a var_t instance */
	CHKiRet(var.Construct(&pVar));
	CHKiRet(var.ConstructFinalize(pVar));

	/* always call MsgGetProp() without a template specifier */
	pszProp = (uchar*) MsgGetProp(pThis, NULL, pstrPropName, &bMustBeFreed);

	/* now create a string object out of it and hand that over to the var */
	CHKiRet(rsCStrConstructFromszStr(&pstrProp, pszProp));
	CHKiRet(var.SetString(pVar, pstrProp));

	/* finally store var */
	*ppVar = pVar;

finalize_it:
	if(bMustBeFreed)
		free(pszProp);

	RETiRet;
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
	} else if(isProp("pszRawMsg")) {
		MsgSetRawMsg(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr), cstrLen(pProp->val.pStr));
 	/* enable this, if someone actually uses UxTradMsg, delete after some  time has
	 * passed and nobody complained -- rgerhards, 2009-06-16
	} else if(isProp("offAfterPRI")) {
		pThis->offAfterPRI = pProp->val.num;
	*/
	} else if(isProp("pszMSG")) {
		MsgSetMSG(pThis, (char*) rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pszUxTradMsg")) {
		/*IGNORE*/; /* this *was* a property, but does no longer exist */
	} else if(isProp("pszTAG")) {
		MsgSetTAG(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), cstrLen(pProp->val.pStr));
	} else if(isProp("pszInputName")) {
		MsgSetInputName(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr), rsCStrLen(pProp->val.pStr));
	} else if(isProp("pszRcvFromIP")) {
		MsgSetRcvFromIP(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pszRcvFrom")) {
		MsgSetRcvFrom(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr));
	} else if(isProp("pszHOSTNAME")) {
		MsgSetHOSTNAME(pThis, rsCStrGetSzStrNoNULL(pProp->val.pStr));
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
	}

	RETiRet;
}
#undef	isProp


/* This is a construction finalizer that must be called after all properties
 * have been set. It does some final work on the message object. After this
 * is done, the object is considered ready for full processing.
 * rgerhards, 2008-07-08
 */
static rsRetVal msgConstructFinalizer(msg_t *pThis)
{
	MsgPrepareEnqueue(pThis);
	return RS_RET_OK;
}


/* get the severity - this is an entry point that
 * satisfies the base object class getSeverity semantics.
 * rgerhards, 2008-01-14
 */
static rsRetVal
MsgGetSeverity(obj_t *pThis, int *piSeverity)
{
	ISOBJ_TYPE_assert(pThis, msg);
	assert(piSeverity != NULL);
	*piSeverity = ((msg_t*) pThis)->iSeverity;
	return RS_RET_OK;
}


/* dummy */
rsRetVal msgQueryInterface(void) { return RS_RET_NOT_IMPLEMENTED; }

/* Initialize the message class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-01-04
 */
BEGINObjClassInit(msg, 1, OBJ_IS_CORE_MODULE)
	/* request objects we use */
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_SERIALIZE, MsgSerialize);
	OBJSetMethodHandler(objMethod_SETPROPERTY, MsgSetProperty);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, msgConstructFinalizer);
	OBJSetMethodHandler(objMethod_GETSEVERITY, MsgGetSeverity);
	/* initially, we have no need to lock message objects */
	funcLock = MsgLockingDummy;
	funcUnlock = MsgLockingDummy;
	funcDeleteMutex = MsgLockingDummy;
	funcMsgPrepareEnqueue = MsgLockingDummy;
ENDObjClassInit(msg)
/* vim:set ai:
 */
