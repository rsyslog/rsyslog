/**
 * \brief This is the main file of the rsyslogd daemon.
 *
 * Please visit the rsyslog project at
 *
 * http://www.rsyslog.com
 *
 * to learn more about it and discuss any questions you may have.
 *
 * rsyslog had initially been forked from the sysklogd project.
 * I would like to express my thanks to the developers of the sysklogd
 * package - without it, I would have had a much harder start...
 *
 * As of this writing (2008-01-03), there have been numerous changes to
 * the original package. Be very careful when you apply some of your
 * sysklogd knowledge to rsyslog.
 * 
 * This Project was intiated and is maintained by
 * Rainer Gerhards <rgerhards@hq.adiscon.com>. See
 * AUTHORS to learn who helped make it become a reality.
 *
 * If you have questions about rsyslogd in general, please email
 * info@adiscon.com. To learn more about rsyslogd, please visit
 * http://www.rsyslog.com.
 *
 * \author Rainer Gerhards <rgerhards@adiscon.com>
 * \date 2003-10-17
 *       Some initial modifications on the sysklogd package to support
 *       liblogging. These have actually not yet been merged to the
 *       source you see currently (but they hopefully will)
 *
 * \date 2004-10-28
 *       Restarted the modifications of sysklogd. This time, we
 *       focus on a simpler approach first. The initial goal is to
 *       provide MySQL database support (so that syslogd can log
 *       to the database).
 *
 * rsyslog - An Enhanced syslogd Replacement.
 * Copyright 2003-2008 Rainer Gerhards and Adiscon GmbH.
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

/* change the following setting to e.g. 32768 if you would like to
 * support large message sizes for IHE (32k is the current maximum
 * needed for IHE). I was initially tempted to increase it to 32k,
 * but there is a large memory footprint with the current
 * implementation in rsyslog. This will change as the processing
 * changes, but I have re-set it to 1k, because the vast majority
 * of messages is below that and the memory savings is huge, at
 * least compared to the overall memory footprint.
 *
 * If you intend to receive Windows Event Log data (e.g. via
 * EventReporter - www.eventreporter.com), you might want to 
 * increase this number to an even higher value, as event
 * log messages can be very lengthy.
 * rgerhards, 2005-07-05
 *
 * during my recent testing, it showed that 4k seems to be
 * the typical maximum for UDP based syslog. This is a IP stack
 * restriction. Not always ... but very often. If you go beyond
 * that value, be sure to test that rsyslogd actually does what
 * you think it should do ;) Also, it is a good idea to check the
 * doc set for anything on IHE - it most probably has information on
 * message sizes.
 * rgerhards, 2005-08-05
 * 
 * I have increased the default message size to 2048 to be in sync
 * with recent IETF syslog standardization efforts.
 * rgerhards, 2006-11-30
 *
 * I have removed syslogdPanic(). That function was supposed to be used
 * for logging in low-memory conditons. Ever since it was introduced, it
 * was a wrapper for dbgprintf(). A more intelligent choice was hard to
 * find. After all, if we are short on memory, doing anything fance will
 * again cause memory problems. I have now modified the code so that
 * those elements for which we do not get memory are simply discarded.
 * That might be a single property like the TAG, but it might also be
 * a complete message. The overall goal of this code change is to keep
 * rsyslogd up and running, while we sacrifice some messages to reach
 * that goal. It also keeps the code cleaner. A real out of memory
 * condition is highly unlikely. If it happens, there will probably be
 * much more trouble on the system in question. Anyhow - rsyslogd will
 * most probably be able to survive it and carry on with processing
 * once the situation has been resolved.
 */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */

#define CONT_LINE	1		/* Allow continuation lines */

#ifdef MTRACE
#include <mcheck.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#define GNU_SOURCE
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <dlfcn.h>
#include <assert.h>

#include <sys/syslog.h>
#include <sys/param.h>
#ifdef	__sun
#include <errno.h>
#else
#include <sys/errno.h>
#endif
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/time.h>

#if HAVE_SYS_TIMESPEC_H
# include <sys/timespec.h>
#endif

#include <sys/resource.h>
#include <signal.h>

#include <dirent.h>
#include <glob.h>
#include <sys/types.h>

#if HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef USE_NETZIP
#include <zlib.h>
#endif

#include "pidfile.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "parse.h"
#include "msg.h"
#include "modules.h"
#include "action.h"
#include "tcpsyslog.h"
#include "iminternal.h"
#include "cfsysline.h"
#include "omshell.h"
#include "omusrmsg.h"
#include "omfwd.h"
#include "omfile.h"
#include "omdiscard.h"
#include "threads.h"
#include "queue.h"
#include "stream.h"
#include "wti.h"
#include "wtp.h"

/* We define our own set of syslog defintions so that we
 * do not need to rely on (possibly different) implementations.
 * 2007-07-19 rgerhards
 */
/* missing definitions for solaris
 * 2006-02-16 Rger
 */
#ifdef __sun
#	define LOG_AUTHPRIV LOG_AUTH
#endif
#define	LOG_MAKEPRI(fac, pri)	(((fac) << 3) | (pri))
#define	LOG_PRI(p)	((p) & LOG_PRIMASK)
#define	LOG_FAC(p)	(((p) & LOG_FACMASK) >> 3)
#define INTERNAL_NOPRI  0x10    /* the "no priority" priority */
#define LOG_FTP         (11<<3) /* ftp daemon */
#define INTERNAL_MARK   LOG_MAKEPRI((LOG_NFACILITIES<<3), 0)


#ifndef UTMP_FILE
#ifdef UTMP_FILENAME
#define UTMP_FILE UTMP_FILENAME
#else
#ifdef _PATH_UTMP
#define UTMP_FILE _PATH_UTMP
#else
#define UTMP_FILE "/etc/utmp"
#endif
#endif
#endif

#ifndef _PATH_LOGCONF 
#define _PATH_LOGCONF	"/etc/rsyslog.conf"
#endif

#ifndef _PATH_MODDIR
#define _PATH_MODDIR	"/lib/rsyslog/"
#endif

#if defined(SYSLOGD_PIDNAME)
#undef _PATH_LOGPID
#if defined(FSSTND)
#ifdef BSD
#define _PATH_VARRUN "/var/run/"
#endif
#ifdef __sun
#define _PATH_VARRUN "/var/run/"
#endif
#define _PATH_LOGPID _PATH_VARRUN SYSLOGD_PIDNAME
#else
#define _PATH_LOGPID "/etc/" SYSLOGD_PIDNAME
#endif
#else
#ifndef _PATH_LOGPID
#if defined(FSSTND)
#define _PATH_LOGPID _PATH_VARRUN "rsyslogd.pid"
#else
#define _PATH_LOGPID "/etc/rsyslogd.pid"
#endif
#endif
#endif

#ifndef _PATH_DEV
#define _PATH_DEV	"/dev/"
#endif

#ifndef _PATH_CONSOLE
#define _PATH_CONSOLE	"/dev/console"
#endif

#ifndef _PATH_TTY
#define _PATH_TTY	"/dev/tty"
#endif

static uchar	*ConfFile = (uchar*) _PATH_LOGCONF; /* read-only after startup */
static char	*PidFile = _PATH_LOGPID; /* read-only after startup */
static uchar	*pModDir = NULL; /* read-only after startup */
char	ctty[] = _PATH_CONSOLE;	/* this is read-only; used by omfile -- TODO: remove that dependency */

static pid_t myPid;	/* our pid for use in self-generated messages, e.g. on startup */
/* mypid is read-only after the initial fork() */
static int restart = 0; /* do restart (config read) - multithread safe */

int glblHadMemShortage = 0; /* indicates if we had memory shortage some time during the run */

#define INTERNAL_NOPRI	0x10	/* the "no priority" priority */
#define TABLE_NOPRI	0	/* Value to indicate no priority in f_pmask */
#define TABLE_ALLPRI    0xFF    /* Value to indicate all priorities in f_pmask */
#define	LOG_MARK	LOG_MAKEPRI(LOG_NFACILITIES, 0)	/* mark "facility" */

/* definitions used for doNameLine to differentiate between different command types
 * (with otherwise identical code). This is a left-over from the previous config
 * system. It stays, because it is still useful. So do not wonder why it looks
 * somewhat strange (at least its name). -- rgerhards, 2007-08-01
 */
enum eDirective { DIR_TEMPLATE = 0, DIR_OUTCHANNEL = 1, DIR_ALLOWEDSENDER = 2};

/* The following global variables are used for building
 * tag and host selector lines during startup and config reload.
 * This is stored as a global variable pool because of its ease. It is
 * also fairly compatible with multi-threading as the stratup code must
 * be run in a single thread anyways. So there can be no race conditions. These
 * variables are no longer used once the configuration has been loaded (except,
 * of course, during a reload). rgerhards 2005-10-18
 */
static EHostnameCmpMode eDfltHostnameCmpMode;
static rsCStrObj *pDfltHostnameCmp;
static rsCStrObj *pDfltProgNameCmp;

static int bParseHOSTNAMEandTAG = 1; /* global config var: should the hostname and tag be
                                      * parsed inside message - rgerhards, 2006-03-13 */
static int bFinished = 0;	/* used by termination signal handler, read-only except there
				 * is either 0 or the number of the signal that requested the
 				 * termination.
				 */

/* Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 * TODO: this shall go into action object! -- rgerhards, 2008-01-29
 */
int	repeatinterval[2] = { 30, 60 };	/* # of secs before flush */

#define LIST_DELIMITER	':'		/* delimiter between two hosts */

struct	filed *Files = NULL; /* read-only after init() (but beware of sigusr1!) */

struct code {
	char	*c_name;
	int	c_val;
};

static struct code	PriNames[] = {
	{"alert",	LOG_ALERT},
	{"crit",	LOG_CRIT},
	{"debug",	LOG_DEBUG},
	{"emerg",	LOG_EMERG},
	{"err",		LOG_ERR},
	{"error",	LOG_ERR},		/* DEPRECATED */
	{"info",	LOG_INFO},
	{"none",	INTERNAL_NOPRI},	/* INTERNAL */
	{"notice",	LOG_NOTICE},
	{"panic",	LOG_EMERG},		/* DEPRECATED */
	{"warn",	LOG_WARNING},		/* DEPRECATED */
	{"warning",	LOG_WARNING},
	{"*",		TABLE_ALLPRI},
	{NULL,		-1}
};

static struct code	FacNames[] = {
	{"auth",         LOG_AUTH},
	{"authpriv",     LOG_AUTHPRIV},
	{"cron",         LOG_CRON},
	{"daemon",       LOG_DAEMON},
	{"kern",         LOG_KERN},
	{"lpr",          LOG_LPR},
	{"mail",         LOG_MAIL},
	{"mark",         LOG_MARK},		/* INTERNAL */
	{"news",         LOG_NEWS},
	{"security",     LOG_AUTH},		/* DEPRECATED */
	{"syslog",       LOG_SYSLOG},
	{"user",         LOG_USER},
	{"uucp",         LOG_UUCP},
#if defined(LOG_FTP)
	{"ftp",          LOG_FTP},
#endif
	{"local0",       LOG_LOCAL0},
	{"local1",       LOG_LOCAL1},
	{"local2",       LOG_LOCAL2},
	{"local3",       LOG_LOCAL3},
	{"local4",       LOG_LOCAL4},
	{"local5",       LOG_LOCAL5},
	{"local6",       LOG_LOCAL6},
	{"local7",       LOG_LOCAL7},
	{NULL,           -1},
};

static pid_t ppid; /* This is a quick and dirty hack used for spliting main/startup thread */

/* global variables for config file state */
static int	bDropTrailingLF = 1; /* drop trailing LF's on reception? */
int	iCompatibilityMode = 0;		/* version we should be compatible with; 0 means sysklogd. It is
					   the default, so if no -c<n> option is given, we make ourselvs
					   as compatible to sysklogd as possible. */
static int	bDebugPrintTemplateList = 1;/* output template list in debug mode? */
static int	bDebugPrintCfSysLineHandlerList = 1;/* output cfsyslinehandler list in debug mode? */
static int	bDebugPrintModuleList = 1;/* output module list in debug mode? */
int	bDropMalPTRMsgs = 0;/* Drop messages which have malicious PTR records during DNS lookup */
static uchar	cCCEscapeChar = '\\';/* character to be used to start an escape sequence for control chars */
static int 	bEscapeCCOnRcv; /* escape control characters on reception: 0 - no, 1 - yes */
static int 	bReduceRepeatMsgs; /* reduce repeated message - 0 - no, 1 - yes */
static int	bActExecWhenPrevSusp; /* execute action only when previous one was suspended? */
static int	logEveryMsg = 0;/* no repeat message processing  - read-only after startup
				 * 0 - suppress duplicate messages
				 * 1 - do NOT suppress duplicate messages
				 */
uchar *pszWorkDir = NULL;/* name of rsyslog's spool directory (without trailing slash) */
/* end global config file state variables */

static unsigned int Forwarding = 0;
char	LocalHostName[MAXHOSTNAMELEN+1];/* our hostname  - read-only after startup */
char	*LocalDomain;	/* our local domain name  - read-only after startup */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds - read-only after startup */
int      family = PF_UNSPEC;     /* protocol family (IPv4, IPv6 or both), set via cmdline */
int      send_to_all = 0;        /* send message to all IPv4/IPv6 addresses */
static int	NoFork = 0; 	/* don't fork - don't run in daemon mode - read-only after startup */
int	DisableDNS = 0; /* don't look up IP addresses of remote messages */
char	**StripDomains = NULL;/* these domains may be stripped before writing logs  - r/o after s.u., never touched by init */
char	**LocalHosts = NULL;/* these hosts are logged with their hostname  - read-only after startup, never touched by init */
int	NoHops = 1;	/* Can we bounce syslog messages through an
				   intermediate host.  Read-only after startup */
static int	bHaveMainQueue = 0;/* set to 1 if the main queue - in queueing mode - is available
				 * If the main queue is either not yet ready or not running in 
				 * queueing mode (mode DIRECT!), then this is set to 0.
				 */

extern	int errno;

/* main message queue and its configuration parameters */
static queue_t *pMsgQueue = NULL;				/* the main message queue */
static int iMainMsgQueueSize = 10000;				/* size of the main message queue above */
static int iMainMsgQHighWtrMark = 8000;				/* high water mark for disk-assisted queues */
static int iMainMsgQLowWtrMark = 2000;				/* low water mark for disk-assisted queues */
static int iMainMsgQDiscardMark = 9800;				/* begin to discard messages */
static int iMainMsgQDiscardSeverity = 4;			/* discard warning and above */
static int iMainMsgQueueNumWorkers = 1;				/* number of worker threads for the mm queue above */
static queueType_t MainMsgQueType = QUEUETYPE_FIXED_ARRAY;	/* type of the main message queue above */
static uchar *pszMainMsgQFName = NULL;				/* prefix for the main message queue file */
static size_t iMainMsgQueMaxFileSize = 1024*1024;
static int iMainMsgQPersistUpdCnt = 0;				/* persist queue info every n updates */
static int iMainMsgQtoQShutdown = 0;				/* queue shutdown */ 
static int iMainMsgQtoActShutdown = 1000;			/* action shutdown (in phase 2) */ 
static int iMainMsgQtoEnq = 2000;				/* timeout for queue enque */ 
static int iMainMsgQtoWrkShutdown = 60000;			/* timeout for worker thread shutdown */
static int iMainMsgQWrkMinMsgs = 100;				/* minimum messages per worker needed to start a new one */
static int bMainMsgQSaveOnShutdown = 1;				/* save queue on shutdown (when DA enabled)? */


/* This structure represents the files that will have log
 * copies printed.
 * RGerhards 2004-11-08: Each instance of the filed structure 
 * describes what I call an "output channel". This is important
 * to mention as we now allow database connections to be
 * present in the filed structure. If helps immensely, if we
 * think of it as the abstraction of an output channel.
 * rgerhards, 2005-10-26: The structure below provides ample
 * opportunity for non-thread-safety. Each of the variable
 * accesses must be carefully evaluated, many of them probably
 * be guarded by mutexes. But beware of deadlocks...
 * rgerhards, 2007-08-01: as you can see, the structure has shrunk pretty much. I will
 * remove some of the comments some time. It's still the structure that controls much
 * of the processing that goes on in syslogd, but it now has lots of helpers.
 */
struct filed {
	struct	filed *f_next;		/* next in linked list */
	/* filter properties */
	enum {
		FILTER_PRI = 0,		/* traditional PRI based filer */
		FILTER_PROP = 1		/* extended filter, property based */
	} f_filter_type;
	EHostnameCmpMode eHostnameCmpMode;
	rsCStrObj *pCSHostnameComp;	/* hostname to check */
	rsCStrObj *pCSProgNameComp;	/* tag to check or NULL, if not to be checked */
	union {
		u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
		struct {
			rsCStrObj *pCSPropName;
			enum {
				FIOP_NOP = 0,		/* do not use - No Operation */
				FIOP_CONTAINS  = 1,	/* contains string? */
				FIOP_ISEQUAL  = 2,	/* is (exactly) equal? */
				FIOP_STARTSWITH = 3,	/* starts with a string? */
 				FIOP_REGEX = 4		/* matches a regular expression? */
			} operation;
			rsCStrObj *pCSCompValue;	/* value to "compare" against */
			char isNegated;			/* actually a boolean ;) */
		} prop;
	} f_filterData;

	linkedList_t llActList;	/* list of configured actions */
};
typedef struct filed selector_t;	/* new type name */


/* support for simple textual representation of FIOP names
 * rgerhards, 2005-09-27
 */
static char* getFIOPName(unsigned iFIOP)
{
	char *pRet;
	switch(iFIOP) {
		case FIOP_CONTAINS:
			pRet = "contains";
			break;
		case FIOP_ISEQUAL:
			pRet = "isequal";
			break;
		case FIOP_STARTSWITH:
			pRet = "startswith";
			break;
		case FIOP_REGEX:
			pRet = "regex";
			break;
		default:
			pRet = "NOP";
			break;
	}
	return pRet;
}


/* Reset config variables to default values.
 * rgerhards, 2007-07-17
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	cCCEscapeChar = '#';
	bActExecWhenPrevSusp = 0;
	bDebugPrintTemplateList = 1;
	bDebugPrintCfSysLineHandlerList = 1;
	bDebugPrintModuleList = 1;
	bEscapeCCOnRcv = 1; /* default is to escape control characters */
	bReduceRepeatMsgs = (logEveryMsg == 1) ? 0 : 1;
	bDropMalPTRMsgs = 0;
	if(pModDir != NULL) {
		free(pModDir);
		pModDir = NULL;
	}
	if(pszWorkDir != NULL) {
		free(pszWorkDir);
		pszWorkDir = NULL;
	}
	if(pszMainMsgQFName != NULL) {
		free(pszMainMsgQFName);
		pszMainMsgQFName = NULL;
	}
	iMainMsgQueueSize = 10000;
	iMainMsgQHighWtrMark = 8000;
	iMainMsgQLowWtrMark = 2000;
	iMainMsgQDiscardMark = 9800;
	iMainMsgQDiscardSeverity = 4;
	iMainMsgQueMaxFileSize = 1024 * 1024;
	iMainMsgQueueNumWorkers = 1;
	iMainMsgQPersistUpdCnt = 0;
	iMainMsgQtoQShutdown = 0;
	iMainMsgQtoActShutdown = 1000;
	iMainMsgQtoEnq = 2000;
	iMainMsgQtoWrkShutdown = 60000;
	iMainMsgQWrkMinMsgs = 100;
	bMainMsgQSaveOnShutdown = 1;
	MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
	glbliActionResumeRetryCount = 0;

	return RS_RET_OK;
}



int option_DisallowWarning = 1;	/* complain if message from disallowed sender is received */


/* hardcoded standard templates (used for defaults) */
static uchar template_TraditionalFormat[] = "\"%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::drop-last-lf%\n\"";
static uchar template_WallFmt[] = "\"\r\n\7Message from syslogd@%HOSTNAME% at %timegenerated% ...\r\n %syslogtag%%msg%\n\r\"";
static uchar template_StdFwdFmt[] = "\"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag%%msg%\"";
static uchar template_StdUsrMsgFmt[] = "\" %syslogtag%%msg%\n\r\"";
static uchar template_StdDBFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')\",SQL";
static uchar template_StdPgSQLFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-pgsql%', '%timegenerated:::date-pgsql%', %iut%, '%syslogtag%')\",STDSQL";
/* end template */


/* up to the next comment, prototypes that should be removed by reordering */
/* Function prototypes. */
static char **crunch_list(char *list);
static void reapchild();
static void debug_switch();
static rsRetVal cfline(uchar *line, selector_t **pfCurr);
static int decode(uchar *name, struct code *codetab);
static void sighup_handler();
static void freeSelectors(void);
static rsRetVal processConfFile(uchar *pConfFile);
static rsRetVal selectorAddList(selector_t *f);
static void processImInternal(void);



/*******************************************************************
 * BEGIN CODE-LIBLOGGING                                           *
 *******************************************************************
 * Code in this section is borrowed from liblogging. This is an
 * interim solution. Once liblogging is fully integrated, this is
 * to be removed (see http://www.monitorware.com/liblogging for
 * more details. 2004-11-16 rgerhards
 *
 * Please note that the orginal liblogging code is modified so that
 * it fits into the context of the current version of syslogd.c.
 *
 * DO NOT PUT ANY OTHER CODE IN THIS BEGIN ... END BLOCK!!!!
 */

/**
 * Parse a 32 bit integer number from a string.
 *
 * \param ppsz Pointer to the Pointer to the string being parsed. It
 *             must be positioned at the first digit. Will be updated 
 *             so that on return it points to the first character AFTER
 *             the integer parsed.
 * \retval The number parsed.
 */

static int srSLMGParseInt32(char** ppsz)
{
	int i;

	i = 0;
	while(isdigit((int) **ppsz))
	{
		i = i * 10 + **ppsz - '0';
		++(*ppsz);
	}

	return i;
}


/**
 * Parse a TIMESTAMP-3339.
 * updates the parse pointer position.
 */
static int srSLMGParseTIMESTAMP3339(struct syslogTime *pTime, char** ppszTS)
{
	char *pszTS = *ppszTS;

	assert(pTime != NULL);
	assert(ppszTS != NULL);
	assert(pszTS != NULL);

	pTime->year = srSLMGParseInt32(&pszTS);

	/* We take the liberty to accept slightly malformed timestamps e.g. in 
	 * the format of 2003-9-1T1:0:0. This doesn't hurt on receiving. Of course,
	 * with the current state of affairs, we would never run into this code
	 * here because at postion 11, there is no "T" in such cases ;)
	 */
	if(*pszTS++ != '-')
		return FALSE;
	pTime->month = srSLMGParseInt32(&pszTS);
	if(pTime->month < 1 || pTime->month > 12)
		return FALSE;

	if(*pszTS++ != '-')
		return FALSE;
	pTime->day = srSLMGParseInt32(&pszTS);
	if(pTime->day < 1 || pTime->day > 31)
		return FALSE;

	if(*pszTS++ != 'T')
		return FALSE;

	pTime->hour = srSLMGParseInt32(&pszTS);
	if(pTime->hour < 0 || pTime->hour > 23)
		return FALSE;

	if(*pszTS++ != ':')
		return FALSE;
	pTime->minute = srSLMGParseInt32(&pszTS);
	if(pTime->minute < 0 || pTime->minute > 59)
		return FALSE;

	if(*pszTS++ != ':')
		return FALSE;
	pTime->second = srSLMGParseInt32(&pszTS);
	if(pTime->second < 0 || pTime->second > 60)
		return FALSE;

	/* Now let's see if we have secfrac */
	if(*pszTS == '.')
	{
		char *pszStart = ++pszTS;
		pTime->secfrac = srSLMGParseInt32(&pszTS);
		pTime->secfracPrecision = (int) (pszTS - pszStart);
	}
	else
	{
		pTime->secfracPrecision = 0;
		pTime->secfrac = 0;
	}

	/* check the timezone */
	if(*pszTS == 'Z')
	{
		pszTS++; /* eat Z */
		pTime->OffsetMode = 'Z';
		pTime->OffsetHour = 0;
		pTime->OffsetMinute = 0;
	}
	else if((*pszTS == '+') || (*pszTS == '-'))
	{
		pTime->OffsetMode = *pszTS;
		pszTS++;

		pTime->OffsetHour = srSLMGParseInt32(&pszTS);
		if(pTime->OffsetHour < 0 || pTime->OffsetHour > 23)
			return FALSE;

		if(*pszTS++ != ':')
			return FALSE;
		pTime->OffsetMinute = srSLMGParseInt32(&pszTS);
		if(pTime->OffsetMinute < 0 || pTime->OffsetMinute > 59)
			return FALSE;
	}
	else
		/* there MUST be TZ information */
		return FALSE;

	/* OK, we actually have a 3339 timestamp, so let's indicated this */
	if(*pszTS == ' ')
		++pszTS;
	else
		return FALSE;

	/* update parse pointer */
	*ppszTS = pszTS;

	return TRUE;
}


/**
 * Parse a TIMESTAMP-3164.
 * Returns TRUE on parse OK, FALSE on parse error.
 */
static int srSLMGParseTIMESTAMP3164(struct syslogTime *pTime, char* pszTS)
{
	assert(pTime != NULL);
	assert(pszTS != NULL);

	getCurrTime(pTime);	/* obtain the current year and UTC offsets! */

	/* If we look at the month (Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec),
	 * we may see the following character sequences occur:
	 *
	 * J(an/u(n/l)), Feb, Ma(r/y), A(pr/ug), Sep, Oct, Nov, Dec
	 *
	 * We will use this for parsing, as it probably is the
	 * fastest way to parse it.
	 *
	 * 2005-07-18, well sometimes it pays to be a bit more verbose, even in C...
	 * Fixed a bug that lead to invalid detection of the data. The issue was that
	 * we had an if(++pszTS == 'x') inside of some of the consturcts below. However,
	 * there were also some elseifs (doing the same ++), which than obviously did not
	 * check the orginal character but the next one. Now removed the ++ and put it
	 * into the statements below. Was a really nasty bug... I didn't detect it before
	 * june, when it first manifested. This also lead to invalid parsing of the rest
	 * of the message, as the time stamp was not detected to be correct. - rgerhards
	 */
	switch(*pszTS++)
	{
	case 'J':
		if(*pszTS == 'a') {
			++pszTS;
			if(*pszTS == 'n') {
				++pszTS;
				pTime->month = 1;
			} else
				return FALSE;
		} else if(*pszTS == 'u') {
			++pszTS;
			if(*pszTS == 'n') {
				++pszTS;
				pTime->month = 6;
			} else if(*pszTS == 'l') {
				++pszTS;
				pTime->month = 7;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'F':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'b') {
				++pszTS;
				pTime->month = 2;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'M':
		if(*pszTS == 'a') {
			++pszTS;
			if(*pszTS == 'r') {
				++pszTS;
				pTime->month = 3;
			} else if(*pszTS == 'y') {
				++pszTS;
				pTime->month = 5;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'A':
		if(*pszTS == 'p') {
			++pszTS;
			if(*pszTS == 'r') {
				++pszTS;
				pTime->month = 4;
			} else
				return FALSE;
		} else if(*pszTS == 'u') {
			++pszTS;
			if(*pszTS == 'g') {
				++pszTS;
				pTime->month = 8;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'S':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'p') {
				++pszTS;
				pTime->month = 9;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'O':
		if(*pszTS == 'c') {
			++pszTS;
			if(*pszTS == 't') {
				++pszTS;
				pTime->month = 10;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'N':
		if(*pszTS == 'o') {
			++pszTS;
			if(*pszTS == 'v') {
				++pszTS;
				pTime->month = 11;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	case 'D':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'c') {
				++pszTS;
				pTime->month = 12;
			} else
				return FALSE;
		} else
			return FALSE;
		break;
	default:
		return FALSE;
	}

	/* done month */

	if(*pszTS++ != ' ')
		return FALSE;

	/* we accept a slightly malformed timestamp when receiving. This is
	 * we accept one-digit days
	 */
	if(*pszTS == ' ')
		++pszTS;

	pTime->day = srSLMGParseInt32(&pszTS);
	if(pTime->day < 1 || pTime->day > 31)
		return FALSE;

	if(*pszTS++ != ' ')
		return FALSE;
	pTime->hour = srSLMGParseInt32(&pszTS);
	if(pTime->hour < 0 || pTime->hour > 23)
		return FALSE;

	if(*pszTS++ != ':')
		return FALSE;
	pTime->minute = srSLMGParseInt32(&pszTS);
	if(pTime->minute < 0 || pTime->minute > 59)
		return FALSE;

	if(*pszTS++ != ':')
		return FALSE;
	pTime->second = srSLMGParseInt32(&pszTS);
	if(pTime->second < 0 || pTime->second > 60)
		return FALSE;
	if(*pszTS++ != ':')

	/* OK, we actually have a 3164 timestamp, so let's indicate this
	 * and fill the rest of the properties. */
	pTime->timeType = 1;
 	pTime->secfracPrecision = 0;
	pTime->secfrac = 0;
	return TRUE;
}

/*******************************************************************
 * END CODE-LIBLOGGING                                             *
 *******************************************************************/

/**
 * Format a syslogTimestamp into format required by MySQL.
 * We are using the 14 digits format. For example 20041111122600 
 * is interpreted as '2004-11-11 12:26:00'. 
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 */
int formatTimestampToMySQL(struct syslogTime *ts, char* pDst, size_t iLenDst)
{
	/* currently we do not consider localtime/utc. This may later be
	 * added. If so, I recommend using a property replacer option
	 * and/or a global configuration option. However, we should wait
	 * on user requests for this feature before doing anything.
	 * rgerhards, 2007-06-26
	 */
	assert(ts != NULL);
	assert(pDst != NULL);

	if (iLenDst < 15) /* we need at least 14 bytes
			     14 digits for timestamp + '\n' */
		return(0); 

	return(snprintf(pDst, iLenDst, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", 
		ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second));

}

int formatTimestampToPgSQL(struct syslogTime *ts, char *pDst, size_t iLenDst)
{
       /* see note in formatTimestampToMySQL, applies here as well */
       assert(ts != NULL);
       assert(pDst != NULL);

       if (iLenDst < 21) /* we need 20 bytes + '\n' */
               return(0);

       return(snprintf(pDst, iLenDst, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
                               ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second));
}

/**
 * Format a syslogTimestamp to a RFC3339 timestamp string (as
 * specified in syslog-protocol).
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 */
int formatTimestamp3339(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	int iRet;
	char szTZ[7]; /* buffer for TZ information */

	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(iLenBuf < 20)
		return(0); /* we NEED at least 20 bytes */

	/* do TZ information first, this is easier to take care of "Z" zone in rfc3339 */
	if(ts->OffsetMode == 'Z') {
		szTZ[0] = 'Z';
		szTZ[1] = '\0';
	} else {
		snprintf(szTZ, sizeof(szTZ) / sizeof(char), "%c%2.2d:%2.2d",
			ts->OffsetMode, ts->OffsetHour, ts->OffsetMinute);
	}

	if(ts->secfracPrecision > 0)
	{	/* we now need to include fractional seconds. While doing so, we must look at
		 * the precision specified. For example, if we have millisec precision (3 digits), a
		 * secFrac value of 12 is not equivalent to ".12" but ".012". Obviously, this
		 * is a huge difference ;). To avoid this, we first create a format string with
		 * the specific precision and *then* use that format string to do the actual
		 * formating (mmmmhhh... kind of self-modifying code... ;)).
		 */
		char szFmtStr[64];
		/* be careful: there is ONE actual %d in the format string below ;) */
		snprintf(szFmtStr, sizeof(szFmtStr),
		         "%%04d-%%02d-%%02dT%%02d:%%02d:%%02d.%%0%dd%%s",
			ts->secfracPrecision);
		iRet = snprintf(pBuf, iLenBuf, szFmtStr, ts->year, ts->month, ts->day,
			        ts->hour, ts->minute, ts->second, ts->secfrac, szTZ);
	}
	else
		iRet = snprintf(pBuf, iLenBuf,
		 		"%4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d%s",
				ts->year, ts->month, ts->day,
			        ts->hour, ts->minute, ts->second, szTZ);
	return(iRet);
}

/**
 * Format a syslogTimestamp to a RFC3164 timestamp sring.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string termnator). If 0 is returend, an error occured.
 */
int formatTimestamp3164(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	static char* monthNames[13] = {"ERR", "Jan", "Feb", "Mar",
	                               "Apr", "May", "Jun", "Jul",
				       "Aug", "Sep", "Oct", "Nov", "Dec"};
	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(iLenBuf < 16)
		return(0); /* we NEED 16 bytes */
	return(snprintf(pBuf, iLenBuf, "%s %2d %2.2d:%2.2d:%2.2d",
		monthNames[ts->month], ts->day, ts->hour,
		ts->minute, ts->second
		));
}

/**
 * Format a syslogTimestamp to a text format.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string termnator). If 0 is returend, an error occured.
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int formatTimestamp(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(ts->timeType == 1) {
		return(formatTimestamp3164(ts, pBuf, iLenBuf));
	}

	if(ts->timeType == 2) {
		return(formatTimestamp3339(ts, pBuf, iLenBuf));
	}

	return(0);
}
#endif


/**
 * Get the current date/time in the best resolution the operating
 * system has to offer (well, actually at most down to the milli-
 * second level.
 *
 * The date and time is returned in separate fields as this is
 * most portable and removes the need for additional structures
 * (but I have to admit it is somewhat "bulky";)).
 *
 * Obviously, all caller-provided pointers must not be NULL...
 */
void getCurrTime(struct syslogTime *t)
{
	struct timeval tp;
	struct tm *tm;
	struct tm tmBuf;
	long lBias;

	assert(t != NULL);
	gettimeofday(&tp, NULL);
	tm = localtime_r((time_t*) &(tp.tv_sec), &tmBuf);

	t->year = tm->tm_year + 1900;
	t->month = tm->tm_mon + 1;
	t->day = tm->tm_mday;
	t->hour = tm->tm_hour;
	t->minute = tm->tm_min;
	t->second = tm->tm_sec;
	t->secfrac = tp.tv_usec;
	t->secfracPrecision = 6;

#	if __sun
		/* Solaris uses a different method of exporting the time zone.
		 * It is UTC - localtime, which is the opposite sign of mins east of GMT.
		 */
		lBias = -(daylight ? altzone : timezone);
#	else
		lBias = tm->tm_gmtoff;
#	endif
	if(lBias < 0)
	{
		t->OffsetMode = '-';
		lBias *= -1;
	}
	else
		t->OffsetMode = '+';
	t->OffsetHour = lBias / 3600;
	t->OffsetMinute = lBias % 3600;
}
/* rgerhards 2004-11-09: end of helper routines. On to the 
 * "real" code ;)
 */


static int usage(void)
{
	fprintf(stderr, "usage: rsyslogd [-46AdhqQvw] [-cversion] [-lhostlist] [-mmarkinterval] [-n] [-p path]\n" \
		" [-s domainlist] [-r[port]] [-tport[,max-sessions]] [-gport[,max-sessions]] [-f conffile] [-i pidfile] [-x]\n\n");
	fprintf(stderr, "The following options are deprecated and are provided\n"
 		        "for compatibility reasons only:\n"
			"-mmarkinterval\n\n"
			"For further information see http://www.rsyslog.com/doc\n"
	       );
	exit(1); /* "good" exit - done to terminate usage() */
}


/* function to destruct a selector_t object
 * rgerhards, 2007-08-01
 */
static rsRetVal selectorDestruct(void *pVal)
{
	selector_t *pThis = (selector_t *) pVal;

	assert(pThis != NULL);

	if(pThis->pCSHostnameComp != NULL)
		rsCStrDestruct(pThis->pCSHostnameComp);
	if(pThis->pCSProgNameComp != NULL)
		rsCStrDestruct(pThis->pCSProgNameComp);

	if(pThis->f_filter_type == FILTER_PROP) {
		if(pThis->f_filterData.prop.pCSPropName != NULL)
			rsCStrDestruct(pThis->f_filterData.prop.pCSPropName);
		if(pThis->f_filterData.prop.pCSCompValue != NULL)
			rsCStrDestruct(pThis->f_filterData.prop.pCSCompValue);
	}

	llDestroy(&pThis->llActList);
	free(pThis);
	
	return RS_RET_OK;
}


/* function to construct a selector_t object
 * rgerhards, 2007-08-01
 */
static rsRetVal selectorConstruct(selector_t **ppThis)
{
	DEFiRet;
	selector_t *pThis;

	assert(ppThis != NULL);
	
	if((pThis = (selector_t*) calloc(1, sizeof(selector_t))) == NULL) {
		glblHadMemShortage = 1;
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}
	CHKiRet(llInit(&pThis->llActList, actionDestruct, NULL, NULL));

finalize_it:
	if(iRet != RS_RET_OK) {
		if(pThis != NULL) {
			selectorDestruct(pThis);
		}
	}
	*ppThis = pThis;
	RETiRet;
}


/* rgerhards, 2005-10-24: crunch_list is called only during option processing. So
 * it is never called once rsyslogd is running (not even when HUPed). This code
 * contains some exits, but they are considered safe because they only happen
 * during startup. Anyhow, when we review the code here, we might want to
 * reconsider the exit()s.
 */
static char **crunch_list(char *list)
{
	int count, i;
	char *p, *q;
	char **result = NULL;

	p = list;
	
	/* strip off trailing delimiters */
	while (p[strlen(p)-1] == LIST_DELIMITER) {
		count--;
		p[strlen(p)-1] = '\0';
	}
	/* cut off leading delimiters */
	while (p[0] == LIST_DELIMITER) {
		count--;
		p++; 
	}
	
	/* count delimiters to calculate elements */
	for (count=i=0; p[i]; i++)
		if (p[i] == LIST_DELIMITER) count++;
	
	if ((result = (char **)malloc(sizeof(char *) * (count+2))) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}
	
	/*
	 * We now can assume that the first and last
	 * characters are different from any delimiters,
	 * so we don't have to care about this.
	 */
	count = 0;
	while ((q=strchr(p, LIST_DELIMITER))) {
		result[count] = (char *) malloc((q - p + 1) * sizeof(char));
		if (result[count] == NULL) {
			printf ("Sorry, can't get enough memory, exiting.\n");
			exit(0); /* safe exit, because only called during startup */
		}
		strncpy(result[count], p, q - p);
		result[count][q - p] = '\0';
		p = q; p++;
		count++;
	}
	if ((result[count] = \
	     (char *)malloc(sizeof(char) * strlen(p) + 1)) == NULL) {
		printf ("Sorry, can't get enough memory, exiting.\n");
		exit(0); /* safe exit, because only called during startup */
	}
	strcpy(result[count],p);
	result[++count] = NULL;

#if 0
	count=0;
	while (result[count])
		dbgprintf("#%d: %s\n", count, StripDomains[count++]);
#endif
	return result;
}


void untty(void)
#ifdef HAVE_SETSID
{
	if ( !Debug ) {
		setsid();
	}
	return;
}
#else
{
	int i;

	if ( !Debug ) {
		i = open(_PATH_TTY, O_RDWR);
		if (i >= 0) {
			(void) ioctl(i, (int) TIOCNOTTY, (char *)0);
			(void) close(i);
		}
	}
}
#endif


/* Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 * rgerhards 2004-11-08: Please note
 * that this function does only a partial decoding. At best, it splits 
 * the PRI part. No further decode happens. The rest is done in 
 * logmsg().
 * Added the iSource parameter so that we know if we have to parse
 * HOSTNAME or not. rgerhards 2004-11-16.
 * changed parameter iSource to bParseHost. For details, see comment in
 * printchopped(). rgerhards 2005-10-06
 */
rsRetVal printline(char *hname, char *msg, int bParseHost)
{
	DEFiRet;
	register char *p;
	int pri;
	msg_t *pMsg;

	/* Now it is time to create the message object (rgerhards)
	*/
	CHKiRet(MsgConstruct(&pMsg));
	MsgSetRawMsg(pMsg, msg);
	
	pMsg->bParseHOSTNAME  = bParseHost;
	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit((int) *++p))
		{
		   pri = 10 * pri + (*p - '0');
		}
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);

	/* Now we look at the HOSTNAME. That is a bit complicated...
	 * If we have a locally received message, it does NOT
	 * contain any hostname information in the message itself.
	 * As such, the HOSTNAME is the same as the system that
	 * the message was received from (that, for obvious reasons,
	 * being the local host).  rgerhards 2004-11-16
	 */
	if(bParseHost == 0)
		MsgSetHOSTNAME(pMsg, hname);
	MsgSetRcvFrom(pMsg, hname);

	/* rgerhards 2004-11-19: well, well... we've now seen that we
	 * have the "hostname problem" also with the traditional Unix
	 * message. As we like to emulate it, we need to add the hostname
	 * to it.
	 */
	if(MsgSetUxTradMsg(pMsg, p) != 0)
		ABORT_FINALIZE(RS_RET_ERR);

	logmsg(pri, pMsg, SYNC_FILE);

finalize_it:
	RETiRet;
}


/* rgerhards, 2006-11-30: I have greatly changed this function. Formerly,
 * it tried to reassemble multi-part messages, which is a legacy stock
 * sysklogd concept. In essence, that was that messages not ending with
 * \0 were glued together. As far as I can see, this is a sysklogd
 * specific feature and, from looking at the code, seems to be used
 * pretty seldom (if at all). I remove this now, not the least because it is totally
 * incompatible with upcoming IETF syslog standards. If you experience
 * strange behaviour with messages beeing split across multiple lines,
 * this function here might be the place to look at.
 *
 * Some previous history worth noting:
 * I added the "iSource" parameter. This is needed to distinguish between
 * messages that have a hostname in them (received from the internet) and
 * those that do not have (most prominently /dev/log).  rgerhards 2004-11-16
 * And now I removed the "iSource" parameter and changed it to be "bParseHost",
 * because all that it actually controls is whether the host is parsed or not.
 * For rfc3195 support, we needed to modify the algo for host parsing, so we can
 * no longer rely just on the source (rfc3195d forwarded messages arrive via
 * unix domain sockets but contain the hostname). rgerhards, 2005-10-06
 */
void printchopped(char *hname, char *msg, int len, int fd, int bParseHost)
{
	register int iMsg;
	char *pMsg;
	char *pData;
	char *pEnd;
	char tmpline[MAXLINE + 1];
#	ifdef USE_NETZIP
	char deflateBuf[MAXLINE + 1];
	uLongf iLenDefBuf;
#	endif

	assert(hname != NULL);
	assert(msg != NULL);
	assert(len >= 0);

	dbgprintf("Message length: %d, File descriptor: %d.\n", len, fd);

	/* we first check if we have a NUL character at the very end of the
	 * message. This seems to be a frequent problem with a number of senders.
	 * So I have now decided to drop these NULs. However, if they are intentional,
	 * that may cause us some problems, e.g. with syslog-sign. On the other hand,
	 * current code always has problems with intentional NULs (as it needs to escape
	 * them to prevent problems with the C string libraries), so that does not
	 * really matter. Just to be on the save side, we'll log destruction of such
	 * NULs in the debug log.
	 * rgerhards, 2007-09-14
	 */
	if(*(msg + len - 1) == '\0') {
		dbgprintf("dropped NUL at very end of message\n");
		len--;
	}

	/* then we check if we need to drop trailing LFs, which often make
	 * their way into syslog messages unintentionally. In order to remain
	 * compatible to recent IETF developments, we allow the user to
	 * turn on/off this handling.  rgerhards, 2007-07-23
	 */
	if(bDropTrailingLF && *(msg + len - 1) == '\n') {
		dbgprintf("dropped LF at very end of message (DropTrailingLF is set)\n");
		len--;
	}

	iMsg = 0;	/* initialize receiving buffer index */
	pMsg = tmpline; /* set receiving buffer pointer */
	pData = msg;	/* set source buffer pointer */
	pEnd = msg + len; /* this is one off, which is intensional */

#	ifdef USE_NETZIP
	/* we first need to check if we have a compressed record. If so,
	 * we must decompress it.
	 */
	if(len > 0 && *msg == 'z') { /* compressed data present? (do NOT change order if conditions!) */
		/* we have compressed data, so let's deflate it. We support a maximum
		 * message size of MAXLINE. If it is larger, an error message is logged
		 * and the message is dropped. We do NOT try to decompress larger messages
		 * as such might be used for denial of service. It might happen to later
		 * builds that such functionality be added as an optional, operator-configurable
		 * feature.
		 */
		int ret;
		iLenDefBuf = MAXLINE;
		ret = uncompress((uchar *) deflateBuf, &iLenDefBuf, (uchar *) msg+1, len-1);
		dbgprintf("Compressed message uncompressed with status %d, length: new %ld, old %d.\n",
		        ret, (long) iLenDefBuf, len-1);
		/* Now check if the uncompression worked. If not, there is not much we can do. In
		 * that case, we log an error message but ignore the message itself. Storing the
		 * compressed text is dangerous, as it contains control characters. So we do
		 * not do this. If someone would like to have a copy, this code here could be
		 * modified to do a hex-dump of the buffer in question. We do not include
		 * this functionality right now.
		 * rgerhards, 2006-12-07
		 */
		if(ret != Z_OK) {
			logerrorInt("Uncompression of a message failed with return code %d "
			            "- enable debug logging if you need further information. "
				    "Message ignored.", ret);
			return; /* unconditional exit, nothing left to do... */
		}
		pData = deflateBuf;
		pEnd = deflateBuf + iLenDefBuf;
	}
#	else /* ifdef USE_NETZIP */
	/* in this case, we still need to check if the message is compressed. If so, we must
	 * tell the user we can not accept it.
	 */
	if(len > 0 && *msg == 'z') {
		logerror("Received a compressed message, but rsyslogd does not have compression "
		         "support enabled. The message will be ignored.");
		return;
	}	
#	endif /* ifdef USE_NETZIP */

	while(pData < pEnd) {
		if(iMsg >= MAXLINE) {
			/* emergency, we now need to flush, no matter if
			 * we are at end of message or not...
			 */
			if(iMsg == MAXLINE) {
				*(pMsg + iMsg) = '\0'; /* space *is* reserved for this! */
				printline(hname, tmpline, bParseHost);
			} else {
				/* This case in theory never can happen. If it happens, we have
				 * a logic error. I am checking for it, because if I would not,
				 * we would address memory invalidly with the code above. I
				 * do not care much about this case, just a debug log entry
				 * (I couldn't do any more smart things anyway...).
				 * rgerhards, 2007-9-20
				 */
				dbgprintf("internal error: iMsg > MAXLINE in printchopped()\n");
			}
			return; /* in this case, we are done... nothing left we can do */
		}
		if(*pData == '\0') { /* guard against \0 characters... */
			/* changed to the sequence (somewhat) proposed in
			 * draft-ietf-syslog-protocol-19. rgerhards, 2006-11-30
			 */
			if(iMsg + 3 < MAXLINE) { /* do we have space? */
				*(pMsg + iMsg++) =  cCCEscapeChar;
				*(pMsg + iMsg++) = '0';
				*(pMsg + iMsg++) = '0';
				*(pMsg + iMsg++) = '0';
			} /* if we do not have space, we simply ignore the '\0'... */
			  /* log an error? Very questionable... rgerhards, 2006-11-30 */
			  /* decided: we do not log an error, it won't help... rger, 2007-06-21 */
			++pData;
		} else if(bEscapeCCOnRcv && iscntrl((int) *pData)) {
			/* we are configured to escape control characters. Please note
			 * that this most probably break non-western character sets like
			 * Japanese, Korean or Chinese. rgerhards, 2007-07-17
			 * Note: sysklogd logs octal values only for DEL and CCs above 127.
			 * For others, it logs ^n where n is the control char converted to an
			 * alphabet character. We like consistency and thus escape it to octal
			 * in all cases. If someone complains, we may change the mode. At least
			 * we known now what's going on.
			 * rgerhards, 2007-07-17
			 */
			if(iMsg + 3 < MAXLINE) { /* do we have space? */
				*(pMsg + iMsg++) = cCCEscapeChar;
				*(pMsg + iMsg++) = '0' + ((*pData & 0300) >> 6);
				*(pMsg + iMsg++) = '0' + ((*pData & 0070) >> 3);
				*(pMsg + iMsg++) = '0' + ((*pData & 0007));
			} /* again, if we do not have space, we ignore the char - see comment at '\0' */
			++pData;
		} else {
			*(pMsg + iMsg++) = *pData++;
		}
	}

	*(pMsg + iMsg) = '\0'; /* space *is* reserved for this! */

	/* typically, we should end up here! */
	printline(hname, tmpline, bParseHost);

	return;
}

/* rgerhards 2004-11-09: the following is a function that can be used
 * to log a message orginating from the syslogd itself. In sysklogd code,
 * this is done by simply calling logmsg(). However, logmsg() is changed in
 * rsyslog so that it takes a msg "object". So it can no longer be called
 * directly. This method here solves the need. It provides an interface that
 * allows to construct a locally-generated message. Please note that this
 * function here probably is only an interim solution and that we need to
 * think on the best way to do this.
 */
rsRetVal
logmsgInternal(int pri, char *msg, int flags)
{
	DEFiRet;
	msg_t *pMsg;

	CHKiRet(MsgConstruct(&pMsg));
	MsgSetUxTradMsg(pMsg, msg);
	MsgSetRawMsg(pMsg, msg);
	MsgSetHOSTNAME(pMsg, LocalHostName);
	MsgSetTAG(pMsg, "rsyslogd:");
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	pMsg->bParseHOSTNAME = 0;
	getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */
	flags |= INTERNAL_MSG;

	if(bHaveMainQueue == 0) { /* not yet in queued mode */
		iminternalAddMsg(pri, pMsg, flags);
	} else {
		/* we have the queue, so we can simply provide the 
		 * message to the queue engine.
		 */
		logmsg(pri, pMsg, flags);
	}
finalize_it:
	RETiRet;
}

/* This functions looks at the given message and checks if it matches the
 * provided filter condition. If so, it returns true, else it returns
 * false. This is a helper to logmsg() and meant to drive the decision
 * process if a message is to be processed or not. As I expect this
 * decision code to grow more complex over time AND logmsg() is already
 * a very lengthy function, I thought a separate function is more appropriate.
 * 2005-09-19 rgerhards
 */
int shouldProcessThisMessage(selector_t *f, msg_t *pMsg)
{
	unsigned short pbMustBeFreed;
	char *pszPropVal;
	int iRet = 0;

	assert(f != NULL);
	assert(pMsg != NULL);

	/* we first have a look at the global, BSD-style block filters (for tag
	 * and host). Only if they match, we evaluate the actual filter.
	 * rgerhards, 2005-10-18
	 */
	if(f->eHostnameCmpMode == HN_NO_COMP) {
		/* EMPTY BY INTENSION - we check this value first, because
		 * it is the one most often used, so this saves us time!
		 */
	} else if(f->eHostnameCmpMode == HN_COMP_MATCH) {
		if(rsCStrSzStrCmp(f->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '+%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(f->pCSHostnameComp), getHOSTNAME(pMsg));
			return 0;
		}
	} else { /* must be -hostname */
		if(!rsCStrSzStrCmp(f->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '-%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(f->pCSHostnameComp), getHOSTNAME(pMsg));
			return 0;
		}
	}
	
	if(f->pCSProgNameComp != NULL) {
		int bInv = 0, bEqv = 0, offset = 0;
		if(*(rsCStrGetSzStrNoNULL(f->pCSProgNameComp)) == '-') {
			if(*(rsCStrGetSzStrNoNULL(f->pCSProgNameComp) + 1) == '-')
				offset = 1;
			else {
				bInv = 1;
				offset = 1;
			}
		}
		if(!rsCStrOffsetSzStrCmp(f->pCSProgNameComp, offset, (uchar*) getProgramName(pMsg), getProgramNameLen(pMsg)))
			bEqv = 1;

		if((!bEqv && !bInv) || (bEqv && bInv)) {
			/* not equal or inverted selection, so we are already done... */
			dbgprintf("programname filter '%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(f->pCSProgNameComp), getProgramName(pMsg));
			return 0;
		}
	}
	
	/* done with the BSD-style block filters */

	if(f->f_filter_type == FILTER_PRI) {
		/* skip messages that are incorrect priority */
		if ( (f->f_filterData.f_pmask[pMsg->iFacility] == TABLE_NOPRI) || \
		    ((f->f_filterData.f_pmask[pMsg->iFacility] & (1<<pMsg->iSeverity)) == 0) )
			iRet = 0;
		else
			iRet = 1;
	} else {
		assert(f->f_filter_type == FILTER_PROP); /* assert() just in case... */
		pszPropVal = MsgGetProp(pMsg, NULL, f->f_filterData.prop.pCSPropName, &pbMustBeFreed);

		/* Now do the compares (short list currently ;)) */
		switch(f->f_filterData.prop.operation ) {
		case FIOP_CONTAINS:
			if(rsCStrLocateInSzStr(f->f_filterData.prop.pCSCompValue, (uchar*) pszPropVal) != -1)
				iRet = 1;
			break;
		case FIOP_ISEQUAL:
			if(rsCStrSzStrCmp(f->f_filterData.prop.pCSCompValue,
					  (uchar*) pszPropVal, strlen(pszPropVal)) == 0)
				iRet = 1; /* process message! */
			break;
		case FIOP_STARTSWITH:
			if(rsCStrSzStrStartsWithCStr(f->f_filterData.prop.pCSCompValue,
					  (uchar*) pszPropVal, strlen(pszPropVal)) == 0)
				iRet = 1; /* process message! */
			break;
		case FIOP_REGEX:
			if(rsCStrSzStrMatchRegex(f->f_filterData.prop.pCSCompValue,
					  (unsigned char*) pszPropVal) == 0)
				iRet = 1;
			break;
		default:
			/* here, it handles NOP (for performance reasons) */
			assert(f->f_filterData.prop.operation == FIOP_NOP);
			iRet = 1; /* as good as any other default ;) */
			break;
		}

		/* now check if the value must be negated */
		if(f->f_filterData.prop.isNegated)
			iRet = (iRet == 1) ?  0 : 1;

		if(Debug) {
			printf("Filter: check for property '%s' (value '%s') ",
			        rsCStrGetSzStrNoNULL(f->f_filterData.prop.pCSPropName),
			        pszPropVal);
			if(f->f_filterData.prop.isNegated)
				printf("NOT ");
			printf("%s '%s': %s\n",
			       getFIOPName(f->f_filterData.prop.operation),
			       rsCStrGetSzStrNoNULL(f->f_filterData.prop.pCSCompValue),
			       iRet ? "TRUE" : "FALSE");
		}

		/* cleanup */
		if(pbMustBeFreed)
			free(pszPropVal);
	}

	return(iRet);
}


/* cancellation cleanup handler - frees the action mutex
 * rgerhards, 2008-01-14
 */
static void callActionMutClean(void *arg)
{
	assert(arg != NULL);
	pthread_mutex_unlock((pthread_mutex_t*) arg);
}


/* helper to processMsg(), used to call the configured actions. It is
 * executed from within llExecFunc() of the action list.
 * rgerhards, 2007-08-02
 */
typedef struct processMsgDoActions_s {
	int bPrevWasSuspended; /* was the previous action suspended? */
	msg_t *pMsg;
} processMsgDoActions_t;
DEFFUNC_llExecFunc(processMsgDoActions)
{
	DEFiRet;
	rsRetVal iRetMod;	/* return value of module - we do not always pass that back */
	action_t *pAction = (action_t*) pData;
	processMsgDoActions_t *pDoActData = (processMsgDoActions_t*) pParam;

	assert(pAction != NULL);

	if((pAction->bExecWhenPrevSusp  == 1) && (pDoActData->bPrevWasSuspended == 0)) {
		dbgprintf("not calling action because the previous one is not suspended\n");
		ABORT_FINALIZE(RS_RET_OK);
	}

	iRetMod = actionCallAction(pAction, pDoActData->pMsg);
	if(iRetMod == RS_RET_DISCARDMSG) {
		ABORT_FINALIZE(RS_RET_DISCARDMSG);
	} else if(iRetMod == RS_RET_SUSPENDED) {
		/* indicate suspension for next module to be called */
		pDoActData->bPrevWasSuspended = 1;
	} else {
		pDoActData->bPrevWasSuspended = 0;
	}

finalize_it:
	RETiRet;
}


/* Process (consume) a received message. Calls the actions configured.
 * rgerhards, 2005-10-13
 */
static void
processMsg(msg_t *pMsg)
{
	selector_t *f;
	int bContinue;
	processMsgDoActions_t DoActData;

	BEGINfunc
	assert(pMsg != NULL);

	/* log the message to the particular outputs */

	bContinue = 1;
	for (f = Files; f != NULL && bContinue ; f = f->f_next) {
		/* first check the filters... */
		if(!shouldProcessThisMessage(f, pMsg)) {
			continue;
		}

		/* ok -- from here, we have action-specific code, nothing really selector-specific -- rger 2007-08-01 */
		DoActData.pMsg = pMsg;
		DoActData.bPrevWasSuspended = 0;
		if(llExecFunc(&f->llActList, processMsgDoActions, (void*)&DoActData) == RS_RET_DISCARDMSG)
			bContinue = 0;
	}
	ENDfunc
}


/* The consumer of dequeued messages. This function is called by the
 * queue engine on dequeueing of a message. It runs on a SEPARATE
 * THREAD.
 * NOTE: Having more than one worker requires guarding of some
 * message object structures and potentially others - need to be checked
 * before we support multiple worker threads on the message queue.
 * Please note: the message object is destructed by the queue itself!
 */
static rsRetVal
msgConsumer(void __attribute__((unused)) *notNeeded, void *pUsr)
{
	DEFiRet;
	msg_t *pMsg = (msg_t*) pUsr;

	assert(pMsg != NULL);

	processMsg(pMsg);
	MsgDestruct(&pMsg);

	RETiRet;
}


/* Helper to parseRFCSyslogMsg. This function parses a field up to
 * (and including) the SP character after it. The field contents is
 * returned in a caller-provided buffer. The parsepointer is advanced
 * to after the terminating SP. The caller must ensure that the 
 * provided buffer is large enough to hold the to be extracted value.
 * Returns 0 if everything is fine or 1 if either the field is not
 * SP-terminated or any other error occurs.
 * rger, 2005-11-24
 */
static int parseRFCField(char **pp2parse, char *pResult)
{
	char *p2parse;
	int iRet = 0;

	assert(pp2parse != NULL);
	assert(*pp2parse != NULL);
	assert(pResult != NULL);

	p2parse = *pp2parse;

	/* this is the actual parsing loop */
	while(*p2parse && *p2parse != ' ') {
		*pResult++ = *p2parse++;
	}

	if(*p2parse == ' ')
		++p2parse; /* eat SP, but only if not at end of string */
	else
		iRet = 1; /* there MUST be an SP! */
	*pResult = '\0';

	/* set the new parse pointer */
	*pp2parse = p2parse;
	return 0;
}


/* Helper to parseRFCSyslogMsg. This function parses the structured
 * data field of a message. It does NOT parse inside structured data,
 * just gets the field as whole. Parsing the single entities is left
 * to other functions. The parsepointer is advanced
 * to after the terminating SP. The caller must ensure that the 
 * provided buffer is large enough to hold the to be extracted value.
 * Returns 0 if everything is fine or 1 if either the field is not
 * SP-terminated or any other error occurs.
 * rger, 2005-11-24
 */
static int parseRFCStructuredData(char **pp2parse, char *pResult)
{
	char *p2parse;
	int bCont = 1;
	int iRet = 0;

	assert(pp2parse != NULL);
	assert(*pp2parse != NULL);
	assert(pResult != NULL);

	p2parse = *pp2parse;

	/* this is the actual parsing loop
	 * Remeber: structured data starts with [ and includes any characters
	 * until the first ] followed by a SP. There may be spaces inside
	 * structured data. There may also be \] inside the structured data, which
	 * do NOT terminate an element.
	 */
	if(*p2parse != '[')
		return 1; /* this is NOT structured data! */

	while(bCont) {
		if(*p2parse == '\0') {
			iRet = 1; /* this is not valid! */
			bCont = 0;
		} else if(*p2parse == '\\' && *(p2parse+1) == ']') {
			/* this is escaped, need to copy both */
			*pResult++ = *p2parse++;
			*pResult++ = *p2parse++;
		} else if(*p2parse == ']' && *(p2parse+1) == ' ') {
			/* found end, just need to copy the ] and eat the SP */
			*pResult++ = *p2parse;
			p2parse += 2;
			bCont = 0;
		} else {
			*pResult++ = *p2parse++;
		}
	}

	if(*p2parse == ' ')
		++p2parse; /* eat SP, but only if not at end of string */
	else
		iRet = 1; /* there MUST be an SP! */
	*pResult = '\0';

	/* set the new parse pointer */
	*pp2parse = p2parse;
	return 0;
}

/* parse a RFC-formatted syslog message. This function returns
 * 0 if processing of the message shall continue and 1 if something
 * went wrong and this messe should be ignored. This function has been
 * implemented in the effort to support syslog-protocol. Please note that
 * the name (parse *RFC*) stems from the hope that syslog-protocol will
 * some time become an RFC. Do not confuse this with informational
 * RFC 3164 (which is legacy syslog).
 *
 * currently supported format:
 *
 * <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP [SD-ID]s SP MSG
 *
 * <PRI> is already stripped when this function is entered. VERSION already
 * has been confirmed to be "1", but has NOT been stripped from the message.
 *
 * rger, 2005-11-24
 */
static int parseRFCSyslogMsg(msg_t *pMsg, int flags)
{
	char *p2parse;
	char *pBuf;
	int bContParse = 1;

	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	p2parse = (char*) pMsg->pszUxTradMsg;

	/* do a sanity check on the version and eat it */
	assert(p2parse[0] == '1' && p2parse[1] == ' ');
	p2parse += 2;

	/* Now get us some memory we can use as a work buffer while parsing.
	 * We simply allocated a buffer sufficiently large to hold all of the
	 * message, so we can not run into any troubles. I think this is
	 * more wise then to use individual buffers.
	 */
	if((pBuf = malloc(sizeof(char)* strlen(p2parse) + 1)) == NULL)
		return 1;
		
	/* IMPORTANT NOTE:
	 * Validation is not actually done below nor are any errors handled. I have
	 * NOT included this for the current proof of concept. However, it is strongly
	 * advisable to add it when this code actually goes into production.
	 * rgerhards, 2005-11-24
	 */

	/* TIMESTAMP */
	if(srSLMGParseTIMESTAMP3339(&(pMsg->tTIMESTAMP),  &p2parse) == FALSE) {
		dbgprintf("no TIMESTAMP detected!\n");
		bContParse = 0;
		flags |= ADDDATE;
	}

	if (flags & ADDDATE) {
		getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */
	}

	/* HOSTNAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetHOSTNAME(pMsg, pBuf);
	} else {
		/* we can not parse, so we get the system we
		 * received the data from.
		 */
		MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
	}

	/* APP-NAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetAPPNAME(pMsg, pBuf);
	}

	/* PROCID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetPROCID(pMsg, pBuf);
	}

	/* MSGID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetMSGID(pMsg, pBuf);
	}

	/* STRUCTURED-DATA */
	if(bContParse) {
		parseRFCStructuredData(&p2parse, pBuf);
		MsgSetStructuredData(pMsg, pBuf);
	}

	/* MSG */
	MsgSetMSG(pMsg, p2parse);

	free(pBuf);
	return 0; /* all ok */
}
/* parse a legay-formatted syslog message. This function returns
 * 0 if processing of the message shall continue and 1 if something
 * went wrong and this messe should be ignored. This function has been
 * implemented in the effort to support syslog-protocol.
 * rger, 2005-11-24
 * As of 2006-01-10, I am removing the logic to continue parsing only
 * when a valid TIMESTAMP is detected. Validity of other fields already
 * is ignored. This is due to the fact that the parser has grown smarter
 * and is now more able to understand different dialects of the syslog
 * message format. I do not expect any bad side effects of this change,
 * but I thought I log it in this comment.
 * rgerhards, 2006-01-10
 */
static int parseLegacySyslogMsg(msg_t *pMsg, int flags)
{
	char *p2parse;
	char *pBuf;
	char *pWork;
	rsCStrObj *pStrB;
	int iCnt;
	int bTAGCharDetected;

	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	p2parse = (char*) pMsg->pszUxTradMsg;

	/* Check to see if msg contains a timestamp
	 */
	if(srSLMGParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), p2parse) == TRUE)
		p2parse += 16;
	else {
		flags |= ADDDATE;
	}

	/* here we need to check if the timestamp is valid. If it is not,
	 * we can not continue to parse but must treat the rest as the 
	 * MSG part of the message (as of RFC 3164).
	 * rgerhards 2004-12-03
	 */
	if(flags & ADDDATE) {
		getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */
	}

	/* rgerhards, 2006-03-13: next, we parse the hostname and tag. But we 
	 * do this only when the user has not forbidden this. I now introduce some
	 * code that allows a user to configure rsyslogd to treat the rest of the
	 * message as MSG part completely. In this case, the hostname will be the
	 * machine that we received the message from and the tag will be empty. This
	 * is meant to be an interim solution, but for now it is in the code.
	 */

	if(bParseHOSTNAMEandTAG && !(flags & INTERNAL_MSG)) {
		/* parse HOSTNAME - but only if this is network-received!
		 * rger, 2005-11-14: we still have a problem with BSD messages. These messages
		 * do NOT include a host name. In most cases, this leads to the TAG to be treated
		 * as hostname and the first word of the message as the TAG. Clearly, this is not
		 * of advantage ;) I think I have now found a way to handle this situation: there
		 * are certain characters which are frequently used in TAG (e.g. ':'), which are
		 * *invalid* in host names. So while parsing the hostname, I check for these characters.
		 * If I find them, I set a simple flag but continue. After parsing, I check the flag.
		 * If it was set, then we most probably do not have a hostname but a TAG. Thus, I change
		 * the fields. I think this logic shall work with any type of syslog message.
		 */
		bTAGCharDetected = 0;
		if(pMsg->bParseHOSTNAME) {
			/* TODO: quick and dirty memory allocation */
			/* the memory allocated is far too much in most cases. But on the plus side,
			 * it is quite fast... - rgerhards, 2007-09-20
			 */
			if((pBuf = malloc(sizeof(char)* (strlen(p2parse) +1))) == NULL)
				return 1;
			pWork = pBuf;
			/* this is the actual parsing loop */
			while(*p2parse && *p2parse != ' ' && *p2parse != ':') {
				if(*p2parse == '[' || *p2parse == ']' || *p2parse == '/')
					bTAGCharDetected = 1;
				*pWork++ = *p2parse++;
			}
			/* we need to handle ':' seperately, because it terminates the
			 * TAG - so we also need to terminate the parser here!
			 * rgerhards, 2007-09-10 *p2parse points to a valid address here in 
			 * any case. We can reach this point only if we are at end of string,
			 * or we have a ':' or ' '. What the if below does is check if we are
			 * not at end of string and, if so, advance the parse pointer. If we 
			 * are already at end of string, *p2parse is equal to '\0', neither if
			 * will be true and the parse pointer remain as is. This is perfectly
			 * well.
			 */
			if(*p2parse == ':') {
				bTAGCharDetected = 1;
				++p2parse;
			} else if(*p2parse == ' ')
				++p2parse;
			*pWork = '\0';
			MsgAssignHOSTNAME(pMsg, pBuf);
		}
		/* check if we seem to have a TAG */
		if(bTAGCharDetected) {
			/* indeed, this smells like a TAG, so lets use it for this. We take
			 * the HOSTNAME from the sender system instead.
			 */
			dbgprintf("HOSTNAME contains invalid characters, assuming it to be a TAG.\n");
			moveHOSTNAMEtoTAG(pMsg);
			MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
		}

		/* now parse TAG - that should be present in message from
		 * all sources.
		 * This code is somewhat not compliant with RFC 3164. As of 3164,
		 * the TAG field is ended by any non-alphanumeric character. In
		 * practice, however, the TAG often contains dashes and other things,
		 * which would end the TAG. So it is not desirable. As such, we only
		 * accept colon and SP to be terminators. Even there is a slight difference:
		 * a colon is PART of the TAG, while a SP is NOT part of the tag
		 * (it is CONTENT). Finally, we allow only up to 32 characters for
		 * TAG, as it is specified in RFC 3164.
		 */
		/* The following code in general is quick & dirty - I need to get
		 * it going for a test, rgerhards 2004-11-16 */
		/* lol.. we tried to solve it, just to remind ourselfs that 32 octets
		 * is the max size ;) we need to shuffle the code again... Just for 
		 * the records: the code is currently clean, but we could optimize it! */
		if(!bTAGCharDetected) {
			uchar *pszTAG;
			if((pStrB = rsCStrConstruct()) == NULL) 
				return 1;
			rsCStrSetAllocIncrement(pStrB, 33);
			pWork = pBuf;
			iCnt = 0;
			while(*p2parse && *p2parse != ':' && *p2parse != ' ' && iCnt < 32) {
				rsCStrAppendChar(pStrB, *p2parse++);
				++iCnt;
			}
			if(*p2parse == ':') {
				++p2parse; 
				rsCStrAppendChar(pStrB, ':');
			}
			rsCStrFinish(pStrB);

			rsCStrConvSzStrAndDestruct(pStrB, &pszTAG, 1);
			if(pszTAG == NULL)
			{	/* rger, 2005-11-10: no TAG found - this implies that what
				 * we have considered to be the HOSTNAME is most probably the
				 * TAG. We consider it so probable, that we now adjust it
				 * that way. So we pick up the previously set hostname, assign
				 * it to tag and use the sender system (from IP stack) as
				 * the hostname. This situation is the standard case with
				 * stock BSD syslogd.
				 */
				dbgprintf("No TAG in message, assuming that HOSTNAME is missing.\n");
				moveHOSTNAMEtoTAG(pMsg);
				MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
			} else { /* we have a TAG, so we can happily set it ;) */
				MsgAssignTAG(pMsg, pszTAG);
			}
		} else {
			/* we have no TAG, so we ... */
			/*DO NOTHING*/;
		}
	} else {
		/* we enter this code area when the user has instructed rsyslog NOT
		 * to parse HOSTNAME and TAG - rgerhards, 2006-03-13
		 */
		if(!(flags & INTERNAL_MSG))
		{
			dbgprintf("HOSTNAME and TAG not parsed by user configuraton.\n");
			MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
		}
	}

	/* The rest is the actual MSG */
	MsgSetMSG(pMsg, p2parse);

	return 0; /* all ok */
}


/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 * rgerhards 2004-11-08: actually, this also decodes all but the PRI part.
 * rgerhards 2004-11-09: ... but only, if syslogd could properly be initialized
 *			 if not, we use emergency logging to the console and in
 *                       this case, no further decoding happens.
 * changed to no longer receive a plain message but a msg object instead.
 * rgerhards-2004-11-16: OK, we are now up to another change... This method
 * actually needs to PARSE the message. How exactly this needs to happen depends on
 * a number of things. Most importantly, it depends on the source. For example,
 * locally received messages (SOURCE_UNIXAF) do NOT have a hostname in them. So
 * we need to treat them differntly form network-received messages which have.
 * Well, actually not all network-received message really have a hostname. We
 * can just hope they do, but we can not be sure. So this method tries to find
 * whatever can be found in the message and uses that... Obviously, there is some
 * potential for misinterpretation, which we simply can not solve under the
 * circumstances given.
 */
void
logmsg(int pri, msg_t *pMsg, int flags)
{
	char *msg;
	char PRItext[20];

	BEGINfunc
	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	msg = (char*) pMsg->pszUxTradMsg;
	dbgprintf("logmsg: %s, flags %x, from '%s', msg %s\n",
	        textpri(PRItext, sizeof(PRItext) / sizeof(char), pri),
		flags, getRcvFrom(pMsg), msg);

	/* rger 2005-11-24 (happy thanksgiving!): we now need to check if we have
	 * a traditional syslog message or one formatted according to syslog-protocol.
	 * We need to apply different parsers depending on that. We use the
	 * -protocol VERSION field for the detection.
	 */
	if(msg[0] == '1' && msg[1] == ' ') {
		dbgprintf("Message has syslog-protocol format.\n");
		setProtocolVersion(pMsg, 1);
		if(parseRFCSyslogMsg(pMsg, flags) == 1) {
			MsgDestruct(&pMsg);
			return;
		}
	} else { /* we have legacy syslog */
		dbgprintf("Message has legacy syslog format.\n");
		setProtocolVersion(pMsg, 0);
		if(parseLegacySyslogMsg(pMsg, flags) == 1) {
			MsgDestruct(&pMsg);
			return;
		}
	}

	/* ---------------------- END PARSING ---------------- */

	/* rgerhards, 2005-10-13: if we consider going multi-threaded, this
	 * is probably the best point to split between a producer and a consumer
	 * thread. In general, with the first multi-threaded approach, we should
	 * NOT try to do more than have a single producer and consumer, at least
	 * if both are from the current code base. The issue is that this code
	 * was definitely not written with reentrancy in mind and uses a lot of
	 * global variables. So it is very dangerous to simply go ahead and multi
	 * thread it. However, I think there is a clear distinction between
	 * producer (where data is received) and consumer (where the actions are).
	 * It should be fairly safe to create a single thread for each and run them
	 * concurrently, thightly coupled via an in-memory queue. Even with this 
	 * limited multithraeding, benefits are immediate: the lengthy actions
	 * (database writes!) are de-coupled from the receivers, what should result
	 * in less likely message loss (loss due to receiver overrun). It also allows
	 * us to utilize 2-cpu systems, which will soon be common given the current
	 * advances in multicore CPU hardware. So this is well worth trying.
	 * Another plus of this two-thread-approach would be that it can easily be configured,
	 * so if there are compatibility issues with the threading libs, we could simply
	 * disable it (as a makefile feature).
	 * There is one important thing to keep in mind when doing this basic
	 * multithreading. The syslog/tcp message forwarder manipulates a structutre
	 * that is used by the main thread, which actually sends the data. This
	 * structure must be guarded by a mutex, else we will have race conditions and
	 * some very bad things could happen.
	 *
	 * Additional consumer threads might be added relatively easy for new receivers,
	 * e.g. if we decide to move RFC 3195 via liblogging natively into rsyslogd.
	 *
	 * To aid this functionality, I am moving the rest of the code (the actual
	 * consumer) to its own method, now called "processMsg()".
	 *
	 * rgerhards, 2005-10-25: as of now, the dual-threading code is now in place.
	 * It is an optional feature and even when enabled, rsyslogd will run single-threaded
	 * if it gets any errors during thread creation.
	 */
	
	pMsg->msgFlags = flags;
	MsgPrepareEnqueue(pMsg);
	queueEnqObj(pMsgQueue, (void*) pMsg);
	ENDfunc
}


static void
reapchild()
{
	int saved_errno = errno;
	struct sigaction sigAct;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);  /* reset signal handler -ASP */

	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}


/* helper to doFlushRptdMsgs() to flush the individual action links via llExecFunc
 * rgerhards, 2007-08-02
 */
DEFFUNC_llExecFunc(flushRptdMsgsActions)
{
	action_t *pAction = (action_t*) pData;

	assert(pAction != NULL);
	
	LockObj(pAction);
	if (pAction->f_prevcount && time(NULL) >= REPEATTIME(pAction)) {
		dbgprintf("flush %s: repeated %d times, %d sec.\n",
		    modGetStateName(pAction->pMod), pAction->f_prevcount,
		    repeatinterval[pAction->f_repeatcount]);
		actionWriteToAction(pAction);
		BACKOFF(pAction);
	}
	UnlockObj(pAction);

	return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This method flushes reapeat messages.
 */
static void
doFlushRptdMsgs(void)
{
	register selector_t *f;

	/* see if we need to flush any "message repeated n times"... 
	 * Note that this interferes with objects running on other threads.
	 * We are using appropriate locking inside the function to handle that.
	 */
	for (f = Files; f != NULL ; f = f->f_next) {
		llExecFunc(&f->llActList, flushRptdMsgsActions, NULL);
	}
}


static void debug_switch()
{
	struct sigaction sigAct;

 	dbgprintf("Switching debugging_on to %s\n", (debugging_on == 0) ? "true" : "false");
 	debugging_on = (debugging_on == 0) ? 1 : 0;
	
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = debug_switch;
	sigaction(SIGUSR1, &sigAct, NULL);
}


/*
 * Add a string to error message and send it to logerror()
 * The error message is passed to snprintf() and must be
 * correctly formatted for it (containing a single %s param).
 * rgerhards 2005-09-19
 */
void logerrorSz(char *type, char *errMsg)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), type, errMsg);
	buf[sizeof(buf)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	logerror(buf);
	return;
}

/*
 * Add an integer to error message and send it to logerror()
 * The error message is passed to snprintf() and must be
 * correctly formatted for it (containing a single %d param).
 * rgerhards 2005-09-19
 */
void logerrorInt(char *type, int errCode)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), type, errCode);
	buf[sizeof(buf)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	logerror(buf);
	return;
}

/* Print syslogd errors some place.
 */
void logerror(char *type)
{
	char buf[1024];
	char errStr[1024];

	BEGINfunc
	dbgprintf("Called logerr, msg: %s\n", type);

	if (errno == 0)
		snprintf(buf, sizeof(buf), "%s", type);
	else {
		strerror_r(errno, errStr, sizeof(errStr));
		snprintf(buf, sizeof(buf), "%s: %s", type, errStr);
	}
	buf[sizeof(buf)/sizeof(char) - 1] = '\0'; /* just to be on the safe side... */
	errno = 0;
	logmsgInternal(LOG_SYSLOG|LOG_ERR, buf, ADDDATE);
	ENDfunc
	return;
}

/* doDie() is a signal handler. If called, it sets the bFinished variable
 * to indicate the program should terminate. However, it does not terminate
 * it itself, because that causes issues with multi-threading. The actual
 * termination is then done on the main thread. This solution might introduce
 * a minimal delay, but it is much cleaner than the approach of doing everything
 * inside the signal handler.
 * rgerhards, 2005-10-26
 */
static void doDie(int sig)
{
	static int iRetries = 0; /* debug aid */
	dbgprintf("DoDie called.\n");
	dbgPrintAllDebugInfo();
	if(iRetries++ == 4) {
		dbgprintf("DoDie called 5 times - unconditional exit\n");
		exit(1);
	}
	bFinished = sig;
}


/* die() is called when the program shall end. This typically only occurs
 * during sigterm or during the initialization. 
 * As die() is intended to shutdown rsyslogd, it is
 * safe to call exit() here. Just make sure that die() itself is not called
 * at inapropriate places. As a general rule of thumb, it is a bad idea to add
 * any calls to die() in new code!
 * rgerhards, 2005-10-24
 */
static void
die(int sig)
{
	char buf[256];

	dbgprintf("exiting on signal %d\n", sig);

	/* IMPORTANT: we should close the inputs first, and THEN send our termination
	 * message. If we do it the other way around, logmsgInternal() may block on
	 * a full queue and the inputs still fill up that queue. Depending on the
	 * scheduling order, we may end up with logmsgInternal being held for a quite
	 * long time. When the inputs are terminated first, that should not happen
	 * because the queue is drained in parallel. The situation could only become
	 * an issue with extremely long running actions in a queue full environment.
	 * However, such actions are at least considered poorly written, if not
	 * outright wrong. So we do not care about this very remote problem.
	 * rgerhards, 2008-01-11
	 */

	/* close the inputs */
	dbgprintf("Terminating input threads...\n");
	thrdTerminateAll(); /* TODO: inputs only, please */

	/* and THEN send the termination log message (see long comment above) */
	if (sig) {
		(void) snprintf(buf, sizeof(buf) / sizeof(char),
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
		 "\" x-pid=\"%d\"]" " exiting on signal %d.",
		 (int) myPid, sig);
		errno = 0;
		logmsgInternal(LOG_SYSLOG|LOG_INFO, buf, ADDDATE);
	}
	
	/* drain queue (if configured so) and stop main queue worker thread pool */
	dbgprintf("Terminating main queue...\n");
	queueDestruct(&pMsgQueue);
	pMsgQueue = NULL;

	/* Free ressources and close connections. This includes flushing any remaining
	 * repeated msgs.
	 */
	dbgprintf("Terminating outputs...\n");
	freeSelectors();

	dbgprintf("all primary multi-thread sources have been terminated - now doing aux cleanup...\n");
	/* rger 2005-02-22
	 * now clean up the in-memory structures. OK, the OS
	 * would also take care of that, but if we do it
	 * ourselfs, this makes finding memory leaks a lot
	 * easier.
	 */
	tplDeleteAll();

	remove_pid(PidFile);
	if(glblHadMemShortage)
		dbgprintf("Had memory shortage at least once during the run.\n");

	/* de-init some modules */
	modExitIminternal();

	/*dbgPrintAllDebugInfo(); / * this is the last spot where this can be done - below output modules are unloaded! */
	
	/* TODO: this would also be the right place to de-init the builtin output modules. We
	 * do not currently do that, because the module interface does not allow for
	 * it. This will come some time later (it's essential with loadable modules).
	 * For the time being, this is a memory leak on exit, but as the process is
	 * terminated, we do not really bother about it.
	 * rgerhards, 2007-08-03
	 * I have added some code now, but all that mod init/de-init should be moved to
	 * init, so that modules are unloaded and reloaded on HUP to. Eventually it should go
	 * into freeSelectors() - but that needs to be seen. -- rgerhards, 2007-08-09
	 */
	modUnloadAndDestructAll();

	/* the following line cleans up CfSysLineHandlers that were not based on loadable
	 * modules. As such, they are not yet cleared.
	 */
	unregCfSysLineHdlrs();

	/* clean up auxiliary data */
	if(pModDir != NULL)
		free(pModDir);

	dbgprintf("Clean shutdown completed, bye\n");

	/* exit classes... This MUST be after the dbgprintf (because it de-inits the debug system!) */
	dbgClassExit();

	exit(0); /* "good" exit, this is the terminator function for rsyslog [die()] */
}

/*
 * Signal handler to terminate the parent process.
 * rgerhards, 2005-10-24: this is only called during forking of the
 * detached syslogd. I consider this method to be safe.
 */
static void doexit()
{
	exit(0); /* "good" exit, only during child-creation */
}


/* process a directory and include all of its files into
 * the current config file. There is no specific order of inclusion,
 * files are included in the order they are read from the directory.
 * The caller must have make sure that the provided parameter is
 * indeed a directory.
 * rgerhards, 2007-08-01
 */
static rsRetVal doIncludeDirectory(uchar *pDirName)
{
	DEFiRet;
	int iEntriesDone = 0;
	DIR *pDir;
	union {
              struct dirent d;
              char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
	} u;
	struct dirent *res;
	size_t iDirNameLen;
	size_t iFileNameLen;
	uchar szFullFileName[MAXFNAME];

	assert(pDirName != NULL);

	if((pDir = opendir((char*) pDirName)) == NULL) {
		logerror("error opening include directory");
		ABORT_FINALIZE(RS_RET_FOPEN_FAILURE);
	}

	/* prepare file name buffer */
	iDirNameLen = strlen((char*) pDirName);
	memcpy(szFullFileName, pDirName, iDirNameLen);

	/* now read the directory */
	iEntriesDone = 0;
	while(readdir_r(pDir, &u.d, &res) == 0) {
		if(res == NULL)
			break; /* this also indicates end of directory */
		if(res->d_type != DT_REG)
			continue; /* we are not interested in special files */
		if(res->d_name[0] == '.')
			continue; /* these files we are also not interested in */
		++iEntriesDone;
		/* construct filename */
		iFileNameLen = strlen(res->d_name);
		if (iFileNameLen > NAME_MAX)
			iFileNameLen = NAME_MAX;
		memcpy(szFullFileName + iDirNameLen, res->d_name, iFileNameLen);
		*(szFullFileName + iDirNameLen + iFileNameLen) = '\0';
		dbgprintf("including file '%s'\n", szFullFileName);
		processConfFile(szFullFileName);
		/* we deliberately ignore the iRet of processConfFile() - this is because
		 * failure to process one file does not mean all files will fail. By ignoring,
		 * we retry with the next file, which is the best thing we can do. -- rgerhards, 2007-08-01
		 */
	}

	if(iEntriesDone == 0) {
		/* I just make it a debug output, because I can think of a lot of cases where it
		 * makes sense not to have any files. E.g. a system maintainer may place a $Include
		 * into the config file just in case, when additional modules be installed. When none
		 * are installed, the directory will be empty, which is fine. -- rgerhards 2007-08-01
		 */
		dbgprintf("warning: the include directory contained no files - this may be ok.\n");
	}

finalize_it:
	if(pDir != NULL)
		closedir(pDir);

	RETiRet;
}


/* process a $include config line. That type of line requires
 * inclusion of another file.
 * rgerhards, 2007-08-01
 */
static rsRetVal doIncludeLine(uchar **pp, __attribute__((unused)) void* pVal)
{
	DEFiRet;
	char pattern[MAXFNAME];
	uchar *cfgFile;
	glob_t cfgFiles;
	size_t i = 0;
	struct stat fileInfo;

	assert(pp != NULL);
	assert(*pp != NULL);

	if(getSubString(pp, (char*) pattern, sizeof(pattern) / sizeof(char), ' ')  != 0) {
		logerror("could not extract group name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* Use GLOB_MARK to append a trailing slash for directories.
	 * Required by doIncludeDirectory().
	 */
	glob(pattern, GLOB_MARK, NULL, &cfgFiles);

	for(i = 0; i < cfgFiles.gl_pathc; i++) {
		cfgFile = (uchar*) cfgFiles.gl_pathv[i];

		if(stat((char*) cfgFile, &fileInfo) != 0) 
			continue; /* continue with the next file if we can't stat() the file */

		if(S_ISREG(fileInfo.st_mode)) { /* config file */
			dbgprintf("requested to include config file '%s'\n", cfgFile);
			iRet = processConfFile(cfgFile);
		} else if(S_ISDIR(fileInfo.st_mode)) { /* config directory */
			dbgprintf("requested to include directory '%s'\n", cfgFile);
			iRet = doIncludeDirectory(cfgFile);
		} else { /* TODO: shall we handle symlinks or not? */
			dbgprintf("warning: unable to process IncludeConfig directive '%s'\n", cfgFile);
		}
	}

	globfree(&cfgFiles);

finalize_it:
	RETiRet;
}


/* process a $ModLoad config line.
 * As of now, it is a dummy, that will later evolve into the
 * loader for plug-ins.
 * rgerhards, 2007-07-21
 * varmojfekoj added support for dynamically loadable modules on 2007-08-13
 * rgerhards, 2007-09-25: please note that the non-threadsafe function dlerror() is
 * called below. This is ok because modules are currently only loaded during
 * configuration file processing, which is executed on a single thread. Should we
 * change that design at any stage (what is unlikely), we need to find a
 * replacement.
 */
static rsRetVal doModLoad(uchar **pp, __attribute__((unused)) void* pVal)
{
	DEFiRet;
	uchar szName[512];
        uchar szPath[512];
        uchar errMsg[1024];
	uchar *pModName;
        void *pModHdlr, *pModInit;

	assert(pp != NULL);
	assert(*pp != NULL);

	if(getSubString(pp, (char*) szName, sizeof(szName) / sizeof(uchar), ' ')  != 0) {
		logerror("could not extract module name");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* this below is a quick and dirty hack to provide compatibility with the
	 * $ModLoad MySQL forward compatibility statement. TODO: clean this up
	 * For the time being, it is clean enough, it just needs to be done
	 * differently when we have a full design for loadable plug-ins. For the
	 * time being, we just mangle the names a bit.
	 * rgerhards, 2007-08-14
	 */
	if(!strcmp((char*) szName, "MySQL"))
		pModName = (uchar*) "ommysql.so";
	else
		pModName = szName;

	dbgprintf("Requested to load module '%s'\n", szName);

	if(*pModName == '/') {
		*szPath = '\0';	/* we do not need to append the path - its already in the module name */
	} else {
		strncpy((char *) szPath, (pModDir == NULL) ? _PATH_MODDIR : (char*) pModDir, sizeof(szPath));
	}
	strncat((char *) szPath, (char *) pModName, sizeof(szPath) - strlen((char*) szPath) - 1);
	if(!(pModHdlr = dlopen((char *) szPath, RTLD_NOW))) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', dlopen: %s\n", szPath, dlerror());
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		logerror((char *) errMsg);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if(!(pModInit = dlsym(pModHdlr, "modInit"))) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', dlsym: %s\n", szPath, dlerror());
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		logerror((char *) errMsg);
		dlclose(pModHdlr);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	if((iRet = doModInit(pModInit, (uchar*) pModName, pModHdlr)) != RS_RET_OK) {
		snprintf((char *) errMsg, sizeof(errMsg), "could not load module '%s', rsyslog error %d\n", szPath, iRet);
		errMsg[sizeof(errMsg)/sizeof(uchar) - 1] = '\0';
		logerror((char *) errMsg);
		dlclose(pModHdlr);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	skipWhiteSpace(pp); /* skip over any whitespace */

finalize_it:
	RETiRet;
}

/* parse and interpret a $-config line that starts with
 * a name (this is common code). It is parsed to the name
 * and then the proper sub-function is called to handle
 * the actual directive.
 * rgerhards 2004-11-17
 * rgerhards 2005-06-21: previously only for templates, now 
 *    generalized.
 */
static rsRetVal doNameLine(uchar **pp, void* pVal)
{
	DEFiRet;
	uchar *p;
	enum eDirective eDir;
	char szName[128];

	assert(pp != NULL);
	p = *pp;
	assert(p != NULL);

	eDir = (enum eDirective) pVal;	/* this time, it actually is NOT a pointer! */

	if(getSubString(&p, szName, sizeof(szName) / sizeof(char), ',')  != 0) {
		logerror("Invalid config line: could not extract name - line ignored");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}
	if(*p == ',')
		++p; /* comma was eaten */
	
	/* we got the name - now we pass name & the rest of the string
	 * to the subfunction. It makes no sense to do further
	 * parsing here, as this is in close interaction with the
	 * respective subsystem. rgerhards 2004-11-17
	 */
	
	switch(eDir) {
		case DIR_TEMPLATE: 
			tplAddLine(szName, &p);
			break;
		case DIR_OUTCHANNEL: 
			ochAddLine(szName, &p);
			break;
		case DIR_ALLOWEDSENDER: 
			addAllowedSenderLine(szName, &p);
			break;
		default:/* we do this to avoid compiler warning - not all
			 * enum values call this function, so an incomplete list
			 * is quite ok (but then we should not run into this code,
			 * so at least we log a debug warning).
			 */
			dbgprintf("INTERNAL ERROR: doNameLine() called with invalid eDir %d.\n",
				eDir);
			break;
	}

	*pp = p;

finalize_it:
	RETiRet;
}


/* set the action resume interval
 */
static rsRetVal setActionResumeInterval(void __attribute__((unused)) *pVal, int iNewVal)
{
	return actionSetGlobalResumeInterval(iNewVal);
}


/* set the processes umask (upon configuration request)
 */
static rsRetVal setUmask(void __attribute__((unused)) *pVal, int iUmask)
{
	umask(iUmask);
	dbgprintf("umask set to 0%3.3o.\n", iUmask);

	return RS_RET_OK;
}


/* Parse and interpret a system-directive in the config line
 * A system directive is one that starts with a "$" sign. It offers
 * extended configuration parameters.
 * 2004-11-17 rgerhards
 */
rsRetVal cfsysline(uchar *p)
{
	DEFiRet;
	uchar szCmd[64];
	uchar errMsg[128];	/* for dynamic error messages */

	assert(p != NULL);
	errno = 0;
	if(getSubString(&p, (char*) szCmd, sizeof(szCmd) / sizeof(uchar), ' ')  != 0) {
		logerror("Invalid $-configline - could not extract command - line ignored\n");
		ABORT_FINALIZE(RS_RET_NOT_FOUND);
	}

	/* we now try and see if we can find the command in the registered
	 * list of cfsysline handlers. -- rgerhards, 2007-07-31
	 */
	CHKiRet(processCfSysLineCommand(szCmd, &p));

	/* now check if we have some extra characters left on the line - that
	 * should not be the case. Whitespace is OK, but everything else should
	 * trigger a warning (that may be an indication of undesired behaviour).
	 * An exception, of course, are comments (starting with '#').
	 * rgerhards, 2007-07-04
	 */
	skipWhiteSpace(&p);

	if(*p && *p != '#') { /* we have a non-whitespace, so let's complain */
		snprintf((char*) errMsg, sizeof(errMsg)/sizeof(uchar),
		         "error: extra characters in config line ignored: '%s'", p);
		errno = 0;
		logerror((char*) errMsg);
	}

finalize_it:
	RETiRet;
}


/* helper to freeSelectors(), used with llExecFunc() to flush 
 * pending output.  -- rgerhards, 2007-08-02
 * We do not need to lock the action object here as the processing
 * queue is already empty and no other threads are running when
 * we call this function. -- rgerhards, 2007-12-12
 */
DEFFUNC_llExecFunc(freeSelectorsActions)
{
	action_t *pAction = (action_t*) pData;

	assert(pAction != NULL);

	/* flush any pending output */
	if(pAction->f_prevcount) {
		actionWriteToAction(pAction);
	}

	return RS_RET_OK; /* never fails ;) */
}


/*  Close all open log files and free selector descriptor array.
 */
static void freeSelectors(void)
{
	selector_t *f;
	selector_t *fPrev;

	if(Files != NULL) {
		dbgprintf("Freeing log structures.\n");

		for(f = Files ; f != NULL ; f = f->f_next) {
			llExecFunc(&f->llActList, freeSelectorsActions, NULL);
		}

		/* actions flushed and ready for destruction - so do that... */
		f = Files;
		while (f != NULL) {
			fPrev = f;
			f = f->f_next;
			selectorDestruct(fPrev);
		}

		/* Reflect the deletion of the selectors linked list. */
		Files = NULL;
		bHaveMainQueue = 0;
	}
}


/* helper to dbPrintInitInfo, to print out all actions via
 * the llExecFunc() facility.
 * rgerhards, 2007-08-02
 */
DEFFUNC_llExecFunc(dbgPrintInitInfoAction)
{
	DEFiRet;
	iRet = actionDbgPrint((action_t*) pData);
	printf("\n");

	RETiRet;
}

/* print debug information as part of init(). This pretty much
 * outputs the whole config of rsyslogd. I've moved this code
 * out of init() to clean it somewhat up.
 * rgerhards, 2007-07-31
 */
static void dbgPrintInitInfo(void)
{
	register selector_t *f;
	int iSelNbr = 1;
	int i;

	dbgprintf("\nActive selectors:\n");
	for (f = Files; f != NULL ; f = f->f_next) {
		dbgprintf("Selector %d:\n", iSelNbr++);
		if(f->pCSProgNameComp != NULL)
			dbgprintf("tag: '%s'\n", rsCStrGetSzStrNoNULL(f->pCSProgNameComp));
		if(f->eHostnameCmpMode != HN_NO_COMP)
			dbgprintf("hostname: %s '%s'\n",
				f->eHostnameCmpMode == HN_COMP_MATCH ?
					"only" : "allbut",
				rsCStrGetSzStrNoNULL(f->pCSHostnameComp));
		if(f->f_filter_type == FILTER_PRI) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_filterData.f_pmask[i] == TABLE_NOPRI)
					dbgprintf(" X ");
				else
					dbgprintf("%2X ", f->f_filterData.f_pmask[i]);
		} else {
			dbgprintf("PROPERTY-BASED Filter:\n");
			dbgprintf("\tProperty.: '%s'\n",
			       rsCStrGetSzStrNoNULL(f->f_filterData.prop.pCSPropName));
			dbgprintf("\tOperation: ");
			if(f->f_filterData.prop.isNegated)
				dbgprintf("NOT ");
			dbgprintf("'%s'\n", getFIOPName(f->f_filterData.prop.operation));
			dbgprintf("\tValue....: '%s'\n",
			       rsCStrGetSzStrNoNULL(f->f_filterData.prop.pCSCompValue));
			dbgprintf("\tAction...: ");
		}

		dbgprintf("\nActions:\n");
		llExecFunc(&f->llActList, dbgPrintInitInfoAction, NULL); /* actions */

		dbgprintf("\n");
	}
	dbgprintf("\n");
	if(bDebugPrintTemplateList)
		tplPrintList();
	if(bDebugPrintModuleList)
		modPrintList();
	ochPrintList();

	if(bDebugPrintCfSysLineHandlerList)
		dbgPrintCfSysLineHandlers();

	dbgprintf("Messages with malicious PTR DNS Records are %sdropped.\n",
		bDropMalPTRMsgs	? "" : "not ");

	dbgprintf("Control characters are %sreplaced upon reception.\n",
			bEscapeCCOnRcv? "" : "not ");

	if(bEscapeCCOnRcv)
		dbgprintf("Control character escape sequence prefix is '%c'.\n",
			cCCEscapeChar);

	dbgprintf("Main queue size %d messages.\n", iMainMsgQueueSize);
	dbgprintf("Main queue worker threads: %d, Perists every %d updates.\n",
		  iMainMsgQueueNumWorkers, iMainMsgQPersistUpdCnt);
	dbgprintf("Main queue timeouts: shutdown: %d, action completion shutdown: %d, enq: %d\n",
		   iMainMsgQtoQShutdown, iMainMsgQtoActShutdown, iMainMsgQtoEnq);
	dbgprintf("Main queue watermarks: high: %d, low: %d, discard: %d, discard-severity: %d\n",
		   iMainMsgQHighWtrMark, iMainMsgQLowWtrMark, iMainMsgQDiscardMark, iMainMsgQDiscardSeverity);
	/* TODO: add
	iActionRetryCount = 0;
	iActionRetryInterval = 30000;
	static int iMainMsgQtoWrkShutdown = 60000;
	static int iMainMsgQtoWrkMinMsgs = 100;	
	static int iMainMsgQbSaveOnShutdown = 1;
	setQPROP(queueSettoWrkShutdown, "$MainMsgQueueTimeoutWorkerThreadShutdown", 5000);
	setQPROP(queueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", 100);
	setQPROP(queueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", 1);
	 */
	dbgprintf("Work Directory: '%s'.\n", pszWorkDir);
}


/* process a configuration file
 * started with code from init() by rgerhards on 2007-07-31
 */
static rsRetVal processConfFile(uchar *pConfFile)
{
	DEFiRet;
	int iLnNbr = 0;
	FILE *cf;
	selector_t *fCurr = NULL;
	uchar *p;
#ifdef CONT_LINE
	uchar cbuf[BUFSIZ];
	uchar *cline;
#else
	uchar cline[BUFSIZ];
#endif
	assert(pConfFile != NULL);

	if((cf = fopen((char*)pConfFile, "r")) == NULL) {
		ABORT_FINALIZE(RS_RET_FOPEN_FAILURE);
	}

	/* Now process the file.
	 */
#if CONT_LINE
	cline = cbuf;
	while (fgets((char*)cline, sizeof(cbuf) - (cline - cbuf), cf) != NULL) {
#else
	while (fgets(cline, sizeof(cline), cf) != NULL) {
#endif
		++iLnNbr;
		/* drop LF - TODO: make it better, replace fgets(), but its clean as it is */
		if(cline[strlen((char*)cline)-1] == '\n') {
			cline[strlen((char*)cline) -1] = '\0';
		}
		/* check for end-of-section, comments, strip off trailing
		 * spaces and newline character.
		 */
		p = cline;
		skipWhiteSpace(&p);
		if (*p == '\0' || *p == '#')
			continue;

#if CONT_LINE
		strcpy((char*)cline, (char*)p);
#endif
		for (p = (uchar*) strchr((char*)cline, '\0'); isspace((int) *--p););
#if CONT_LINE
		if (*p == '\\') {
			if ((p - cbuf) > BUFSIZ - 30) {
				/* Oops the buffer is full - what now? */
				cline = cbuf;
			} else {
				*p = 0;
				cline = p;
				continue;
			}
		}  else
			cline = cbuf;
#endif
		*++p = '\0'; /* TODO: check this */

		/* we now have the complete line, and are positioned at the first non-whitespace
		 * character. So let's process it
		 */
#if CONT_LINE
		if(cfline(cbuf, &fCurr) != RS_RET_OK) {
#else
		if(cfline((uchar*)cline, &fCurr) != RS_RET_OK) {
#endif
			/* we log a message, but otherwise ignore the error. After all, the next
			 * line can be correct.  -- rgerhards, 2007-08-02
			 */
			uchar szErrLoc[MAXFNAME + 64];
			dbgprintf("config line NOT successfully processed\n");
			snprintf((char*)szErrLoc, sizeof(szErrLoc) / sizeof(uchar),
				 "%s, line %d", pConfFile, iLnNbr);
			logerrorSz("the last error occured in %s", (char*)szErrLoc);
		}
	}

	/* we probably have one selector left to be added - so let's do that now */
	CHKiRet(selectorAddList(fCurr));

	/* close the configuration file */
	(void) fclose(cf);

finalize_it:
	if(iRet != RS_RET_OK) {
		char errStr[1024];
		if(fCurr != NULL)
			selectorDestruct(fCurr);

		strerror_r(errno, errStr, sizeof(errStr));
		dbgprintf("error %d processing config file '%s'; os error (if any): %s\n",
			iRet, pConfFile, errStr);
	}
	RETiRet;
}


/* Start the input modules. This function will probably undergo big changes
 * while we implement the input module interface. For now, it does the most
 * important thing to get at least my poor initial input modules up and
 * running. Almost no config option is taken.
 * rgerhards, 2007-12-14
 */
static rsRetVal
startInputModules(void)
{
	DEFiRet;
	modInfo_t *pMod;

	/* loop through all modules and activate them (brr...) */
	pMod = modGetNxtType(NULL, eMOD_IN);
	while(pMod != NULL) {
		if((iRet = pMod->mod.im.willRun()) == RS_RET_OK) {
			/* activate here */
			thrdCreate(pMod->mod.im.runInput, pMod->mod.im.afterRun);
		} else {
			dbgprintf("module %lx will not run, iRet %d\n", (unsigned long) pMod, iRet);
		}
	pMod = modGetNxtType(pMod, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* INIT -- Initialize syslogd from configuration table
 * init() is called at initial startup AND each time syslogd is HUPed
 */
static void
init(void)
{
	DEFiRet;
#ifdef CONT_LINE
	char cbuf[BUFSIZ];
#else
	char cline[BUFSIZ];
#endif
	char bufStartUpMsg[512];
	struct sigaction sigAct;

	thrdTerminateAll(); /* stop all running input threads - TODO: reconsider location! */

	/* initialize some static variables */
	pDfltHostnameCmp = NULL;
	pDfltProgNameCmp = NULL;
	eDfltHostnameCmpMode = HN_NO_COMP;
	Forwarding = 0;

	dbgprintf("rsyslog %s.\n", VERSION);
	dbgprintf("Called init.\n");

	/* delete the message queue, which also flushes all messages left over */
	if(pMsgQueue != NULL) {
		dbgprintf("deleting main message queue\n");
		queueDestruct(&pMsgQueue); /* delete pThis here! */
		pMsgQueue = NULL;
	}

	/*  Close all open log files and free log descriptor array. This also frees
	 *  all output-modules instance data.
	 */
	freeSelectors();

	/* Unload all non-static modules */
	dbgprintf("Unloading non-static modules.\n");
	modUnloadAndDestructDynamic();

	dbgprintf("Clearing templates.\n");
	tplDeleteNew();

	/* re-setting values to defaults (where applicable) */
	/* TODO: once we have loadable modules, we must re-visit this code. The reason is
	 * that config variables are not re-set, because the module is not yet loaded. On
	 * the other hand, that doesn't matter, because the module got unloaded and is then
	 * re-loaded, so the variables should be re-set via that way. In any case, we should
	 * think about the whole situation when we implement loadable plugins.
	 * rgerhards, 2007-07-31
	 */
	cfsysline((uchar*)"ResetConfigVariables");

	/* open the configuration file */
	if((iRet = processConfFile(ConfFile)) != RS_RET_OK) {
		/* rgerhards: this code is executed to set defaults when the
		 * config file could not be opened. We might think about
		 * abandoning the run in this case - but this, too, is not
		 * very clever... So we stick with what we have.
		 * We ignore any errors while doing this - we would be lost anyhow...
		 */
		selector_t *f = NULL;
		char szTTYNameBuf[_POSIX_TTY_NAME_MAX+1]; /* +1 for NULL character */
		dbgprintf("primary config file could not be opened - using emergency definitions.\n");
		cfline((uchar*)"*.ERR\t" _PATH_CONSOLE, &f);
		cfline((uchar*)"*.PANIC\t*", &f);
		if(ttyname_r(0, szTTYNameBuf, sizeof(szTTYNameBuf)) == 0) {
			snprintf(cbuf,sizeof(cbuf), "*.*\t%s", szTTYNameBuf);
			cfline((uchar*)cbuf, &f);
		}
		selectorAddList(f);
	}

	/* we are now done with reading the configuration. This is the right time to
	 * free some objects that were just needed for loading it. rgerhards 2005-10-19
	 */
	if(pDfltHostnameCmp != NULL) {
		rsCStrDestruct(pDfltHostnameCmp);
		pDfltHostnameCmp = NULL;
	}

	if(pDfltProgNameCmp != NULL) {
		rsCStrDestruct(pDfltProgNameCmp);
		pDfltProgNameCmp = NULL;
	}

	/* some checks */
	if(iMainMsgQueueNumWorkers < 1) {
		logerror("$MainMsgQueueNumWorkers must be at least 1! Set to 1.\n");
		iMainMsgQueueNumWorkers = 1;
	}

	if(MainMsgQueType == QUEUETYPE_DISK) {
		errno = 0;	/* for logerror! */
		if(pszWorkDir == NULL) {
			logerror("No $WorkDirectory specified - can not run main message queue in 'disk' mode. "
				 "Using 'FixedArray' instead.\n");
			MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
		if(pszMainMsgQFName == NULL) {
			logerror("No $MainMsgQueueFileName specified - can not run main message queue in "
				 "'disk' mode. Using 'FixedArray' instead.\n");
			MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
	}

	/* switch the message object to threaded operation, if necessary */
	if(MainMsgQueType == QUEUETYPE_DIRECT || iMainMsgQueueNumWorkers > 1) {
		MsgEnableThreadSafety();
	}

	/* create message queue */
	CHKiRet_Hdlr(queueConstruct(&pMsgQueue, MainMsgQueType, iMainMsgQueueNumWorkers, iMainMsgQueueSize, msgConsumer)) {
		/* no queue is fatal, we need to give up in that case... */
		fprintf(stderr, "fatal error %d: could not create message queue - rsyslogd can not run!\n", iRet);
		exit(1);
	}
	/* ... set some properties ... */
#	define setQPROP(func, directive, data) \
	CHKiRet_Hdlr(func(pMsgQueue, data)) { \
		logerrorInt("Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}
#	define setQPROPstr(func, directive, data) \
	CHKiRet_Hdlr(func(pMsgQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
		logerrorInt("Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}

	setQPROP(queueSetMaxFileSize, "$MainMsgQueueFileSize", iMainMsgQueMaxFileSize);
	setQPROPstr(queueSetFilePrefix, "$MainMsgQueueFileName", pszMainMsgQFName);
	setQPROP(queueSetiPersistUpdCnt, "$MainMsgQueueCheckpointInterval", iMainMsgQPersistUpdCnt);
	setQPROP(queueSettoQShutdown, "$MainMsgQueueTimeoutShutdown", iMainMsgQtoQShutdown );
	setQPROP(queueSettoActShutdown, "$MainMsgQueueTimeoutActionCompletion", iMainMsgQtoActShutdown);
	setQPROP(queueSettoWrkShutdown, "$MainMsgQueueTimeoutWorkerThreadShutdown", iMainMsgQtoWrkShutdown);
	setQPROP(queueSettoEnq, "$MainMsgQueueTimeoutEnqueue", iMainMsgQtoEnq);
	setQPROP(queueSetiHighWtrMrk, "$MainMsgQueueHighWaterMark", iMainMsgQHighWtrMark);
	setQPROP(queueSetiLowWtrMrk, "$MainMsgQueueLowWaterMark", iMainMsgQLowWtrMark);
	setQPROP(queueSetiDiscardMrk, "$MainMsgQueueDiscardMark", iMainMsgQDiscardMark);
	setQPROP(queueSetiDiscardSeverity, "$MainMsgQueueDiscardSeverity", iMainMsgQDiscardSeverity);
	setQPROP(queueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", iMainMsgQWrkMinMsgs);
	setQPROP(queueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", bMainMsgQSaveOnShutdown);

#	undef setQPROP
#	undef setQPROPstr

	/* ... and finally start the queue! */
	CHKiRet_Hdlr(queueStart(pMsgQueue)) {
		/* no queue is fatal, we need to give up in that case... */
		fprintf(stderr, "fatal error %d: could not start message queue - rsyslogd can not run!\n", iRet);
		exit(1);
	}

	bHaveMainQueue = (MainMsgQueType == QUEUETYPE_DIRECT) ? 0 : 1;
	dbgprintf("Main processing queue is initialized and running\n");

	/* the output part and the queue is now ready to run. So it is a good time
	 * to start the inputs. Please note that the net code above should be
	 * shuffled to down here once we have everything in input modules.
	 * rgerhards, 2007-12-14
	 */
	startInputModules();

	if(Debug) {
		dbgPrintInitInfo();
	}

	/* we now generate the startup message. It now includes everything to
	 * identify this instance. -- rgerhards, 2005-08-17
	 */
	snprintf(bufStartUpMsg, sizeof(bufStartUpMsg)/sizeof(char), 
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
		 "\" x-pid=\"%d\"] restart",
		 (int) myPid);
	logmsgInternal(LOG_SYSLOG|LOG_INFO, bufStartUpMsg, ADDDATE);

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);

	dbgprintf(" (re)started.\n");
	ENDfunc
}


/* Helper to cfline() and its helpers. Parses a template name
 * from an "action" line. Must be called with the Line pointer
 * pointing to the first character after the semicolon.
 * rgerhards 2004-11-19
 * changed function to work with OMSR. -- rgerhards, 2007-07-27
 * the default template is to be used when no template is specified.
 */
rsRetVal cflineParseTemplateName(uchar** pp, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts, uchar *dfltTplName)
{
	uchar *p;
	uchar *tplName;
	DEFiRet;
	rsCStrObj *pStrB;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pOMSR != NULL);

	p =*pp;
	/* a template must follow - search it and complain, if not found
	 */
	skipWhiteSpace(&p);
	if(*p == ';')
		++p; /* eat it */
	else if(*p != '\0' && *p != '#') {
		logerror("invalid character in selector line - ';template' expected");
		iRet = RS_RET_ERR;
		goto finalize_it;
	}

	skipWhiteSpace(&p); /* go to begin of template name */

	if(*p == '\0') {
		/* no template specified, use the default */
		/* TODO: check NULL ptr */
		tplName = (uchar*) strdup((char*)dfltTplName);
	} else {
		/* template specified, pick it up */
		if((pStrB = rsCStrConstruct()) == NULL) {
			glblHadMemShortage = 1;
			iRet = RS_RET_OUT_OF_MEMORY;
			goto finalize_it;
		}

		/* now copy the string */
		while(*p && *p != '#' && !isspace((int) *p)) {
			CHKiRet(rsCStrAppendChar(pStrB, *p));
			++p;
		}
		CHKiRet(rsCStrFinish(pStrB));
		CHKiRet(rsCStrConvSzStrAndDestruct(pStrB, &tplName, 0));
	}

	iRet = OMSRsetEntry(pOMSR, iEntry, tplName, iTplOpts);
	if(iRet != RS_RET_OK) goto finalize_it;

finalize_it:
	*pp = p;

	RETiRet;
}

/* Helper to cfline(). Parses a file name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template.
 * rgerhards 2004-11-18
 * parameter pFileName must point to a buffer large enough
 * to hold the largest possible filename.
 * rgerhards, 2007-07-25
 * updated to include OMSR pointer -- rgerhards, 2007-07-27
 */
rsRetVal cflineParseFileName(uchar* p, uchar *pFileName, omodStringRequest_t *pOMSR, int iEntry, int iTplOpts)
{
	register uchar *pName;
	int i;
	DEFiRet;

	assert(pOMSR != NULL);

	pName = pFileName;
	i = 1; /* we start at 1 so that we reseve space for the '\0'! */
	while(*p && *p != ';' && i < MAXFNAME) {
		*pName++ = *p++;
		++i;
	}
	*pName = '\0';

	iRet = cflineParseTemplateName(&p, pOMSR, iEntry, iTplOpts, (uchar*) " TradFmt");

	RETiRet;
}


/*
 * Helper to cfline(). This function takes the filter part of a traditional, PRI
 * based line and decodes the PRIs given in the selector line. It processed the
 * line up to the beginning of the action part. A pointer to that beginnig is
 * passed back to the caller.
 * rgerhards 2005-09-15
 */
static rsRetVal cflineProcessTradPRIFilter(uchar **pline, register selector_t *f)
{
	uchar *p;
	register uchar *q;
	register int i, i2;
	uchar *bp;
	int pri;
	int singlpri = 0;
	int ignorepri = 0;
	uchar buf[MAXLINE];
	uchar xbuf[200];

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(f != NULL);

	dbgprintf(" - traditional PRI filter\n");
	errno = 0;	/* keep strerror_r() stuff out of logerror messages */

	f->f_filter_type = FILTER_PRI;
	/* Note: file structure is pre-initialized to zero because it was
	 * created with calloc()!
	 */
	for (i = 0; i <= LOG_NFACILITIES; i++) {
		f->f_filterData.f_pmask[i] = TABLE_NOPRI;
	}

	/* scan through the list of selectors */
	for (p = *pline; *p && *p != '\t' && *p != ' ';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t ,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(",;", *q))
			q++;

		/* decode priority name */
		if ( *buf == '!' ) {
			ignorepri = 1;
			for (bp=buf; *(bp+1); bp++)
				*bp=*(bp+1);
			*bp='\0';
		}
		else {
			ignorepri = 0;
		}
		if ( *buf == '=' )
		{
			singlpri = 1;
			pri = decode(&buf[1], PriNames);
		}
		else {
		        singlpri = 0;
			pri = decode(buf, PriNames);
		}

		if (pri < 0) {
			snprintf((char*) xbuf, sizeof(xbuf), "unknown priority name \"%s\"", buf);
			logerror((char*) xbuf);
			return RS_RET_ERR;
		}

		/* scan facilities */
		while (*p && !strchr("\t .;", *p)) {
			for (bp = buf; *p && !strchr("\t ,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*') {
				for (i = 0; i <= LOG_NFACILITIES; i++) {
					if ( pri == INTERNAL_NOPRI ) {
						if ( ignorepri )
							f->f_filterData.f_pmask[i] = TABLE_ALLPRI;
						else
							f->f_filterData.f_pmask[i] = TABLE_NOPRI;
					}
					else if ( singlpri ) {
						if ( ignorepri )
				  			f->f_filterData.f_pmask[i] &= ~(1<<pri);
						else
				  			f->f_filterData.f_pmask[i] |= (1<<pri);
					}
					else
					{
						if ( pri == TABLE_ALLPRI ) {
							if ( ignorepri )
								f->f_filterData.f_pmask[i] = TABLE_NOPRI;
							else
								f->f_filterData.f_pmask[i] = TABLE_ALLPRI;
						}
						else
						{
							if ( ignorepri )
								for (i2= 0; i2 <= pri; ++i2)
									f->f_filterData.f_pmask[i] &= ~(1<<i2);
							else
								for (i2= 0; i2 <= pri; ++i2)
									f->f_filterData.f_pmask[i] |= (1<<i2);
						}
					}
				}
			} else {
				i = decode(buf, FacNames);
				if (i < 0) {

					snprintf((char*) xbuf, sizeof(xbuf), "unknown facility name \"%s\"", buf);
					logerror((char*) xbuf);
					return RS_RET_ERR;
				}

				if ( pri == INTERNAL_NOPRI ) {
					if ( ignorepri )
						f->f_filterData.f_pmask[i >> 3] = TABLE_ALLPRI;
					else
						f->f_filterData.f_pmask[i >> 3] = TABLE_NOPRI;
				} else if ( singlpri ) {
					if ( ignorepri )
						f->f_filterData.f_pmask[i >> 3] &= ~(1<<pri);
					else
						f->f_filterData.f_pmask[i >> 3] |= (1<<pri);
				} else {
					if ( pri == TABLE_ALLPRI ) {
						if ( ignorepri )
							f->f_filterData.f_pmask[i >> 3] = TABLE_NOPRI;
						else
							f->f_filterData.f_pmask[i >> 3] = TABLE_ALLPRI;
					} else {
						if ( ignorepri )
							for (i2= 0; i2 <= pri; ++i2)
								f->f_filterData.f_pmask[i >> 3] &= ~(1<<i2);
						else
							for (i2= 0; i2 <= pri; ++i2)
								f->f_filterData.f_pmask[i >> 3] |= (1<<i2);
					}
				}
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	*pline = p;
	return RS_RET_OK;
}


/*
 * Helper to cfline(). This function takes the filter part of a property
 * based filter and decodes it. It processes the line up to the beginning
 * of the action part. A pointer to that beginnig is passed back to the caller.
 * rgerhards 2005-09-15
 */
static rsRetVal cflineProcessPropFilter(uchar **pline, register selector_t *f)
{
	rsParsObj *pPars;
	rsCStrObj *pCSCompOp;
	rsRetVal iRet;
	int iOffset; /* for compare operations */

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(f != NULL);

	dbgprintf(" - property-based filter\n");
	errno = 0;	/* keep strerror_r() stuff out of logerror messages */

	f->f_filter_type = FILTER_PROP;

	/* create parser object starting with line string without leading colon */
	if((iRet = rsParsConstructFromSz(&pPars, (*pline)+1)) != RS_RET_OK) {
		logerrorInt("Error %d constructing parser object - ignoring selector", iRet);
		return(iRet);
	}

	/* read property */
	iRet = parsDelimCStr(pPars, &f->f_filterData.prop.pCSPropName, ',', 1, 1);
	if(iRet != RS_RET_OK) {
		logerrorInt("error %d parsing filter property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* read operation */
	iRet = parsDelimCStr(pPars, &pCSCompOp, ',', 1, 1);
	if(iRet != RS_RET_OK) {
		logerrorInt("error %d compare operation property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* we now first check if the condition is to be negated. To do so, we first
	 * must make sure we have at least one char in the param and then check the
	 * first one.
	 * rgerhards, 2005-09-26
	 */
	if(rsCStrLen(pCSCompOp) > 0) {
		if(*rsCStrGetBufBeg(pCSCompOp) == '!') {
			f->f_filterData.prop.isNegated = 1;
			iOffset = 1; /* ignore '!' */
		} else {
			f->f_filterData.prop.isNegated = 0;
			iOffset = 0;
		}
	} else {
		f->f_filterData.prop.isNegated = 0;
		iOffset = 0;
	}

	if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "contains", 8)) {
		f->f_filterData.prop.operation = FIOP_CONTAINS;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "isequal", 7)) {
		f->f_filterData.prop.operation = FIOP_ISEQUAL;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (uchar*) "startswith", 10)) {
		f->f_filterData.prop.operation = FIOP_STARTSWITH;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, (unsigned char*) "regex", 5)) {
		f->f_filterData.prop.operation = FIOP_REGEX;
	} else {
		logerrorSz("error: invalid compare operation '%s' - ignoring selector",
		           (char*) rsCStrGetSzStrNoNULL(pCSCompOp));
	}
	rsCStrDestruct (pCSCompOp); /* no longer needed */

	/* read compare value */
	iRet = parsQuotedCStr(pPars, &f->f_filterData.prop.pCSCompValue);
	if(iRet != RS_RET_OK) {
		logerrorInt("error %d compare value property - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* skip to action part */
	if((iRet = parsSkipWhitespace(pPars)) != RS_RET_OK) {
		logerrorInt("error %d skipping to action part - ignoring selector", iRet);
		rsParsDestruct(pPars);
		return(iRet);
	}

	/* cleanup */
	*pline = *pline + rsParsGetParsePointer(pPars) + 1;
		/* we are adding one for the skipped initial ":" */

	return rsParsDestruct(pPars);
}


/*
 * Helper to cfline(). This function interprets a BSD host selector line
 * from the config file ("+/-hostname"). It stores it for further reference.
 * rgerhards 2005-10-19
 */
static rsRetVal cflineProcessHostSelector(uchar **pline)
{
	rsRetVal iRet;

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(**pline == '-' || **pline == '+');

	dbgprintf(" - host selector line\n");

	/* check include/exclude setting */
	if(**pline == '+') {
		eDfltHostnameCmpMode = HN_COMP_MATCH;
	} else { /* we do not check for '-', it must be, else we wouldn't be here */
		eDfltHostnameCmpMode = HN_COMP_NOMATCH;
	}
	(*pline)++;	/* eat + or - */

	/* the below is somewhat of a quick hack, but it is efficient (this is
	 * why it is in here. "+*" resets the tag selector with BSD syslog. We mimic
	 * this, too. As it is easy to check that condition, we do not fire up a
	 * parser process, just make sure we do not address beyond our space.
	 * Order of conditions in the if-statement is vital! rgerhards 2005-10-18
	 */
	if(**pline != '\0' && **pline == '*' && *(*pline+1) == '\0') {
		dbgprintf("resetting BSD-like hostname filter\n");
		eDfltHostnameCmpMode = HN_NO_COMP;
		if(pDfltHostnameCmp != NULL) {
			if((iRet = rsCStrSetSzStr(pDfltHostnameCmp, NULL)) != RS_RET_OK)
				return(iRet);
		}
	} else {
		dbgprintf("setting BSD-like hostname filter to '%s'\n", *pline);
		if(pDfltHostnameCmp == NULL) {
			/* create string for parser */
			if((iRet = rsCStrConstructFromszStr(&pDfltHostnameCmp, *pline)) != RS_RET_OK)
				return(iRet);
		} else { /* string objects exists, just update... */
			if((iRet = rsCStrSetSzStr(pDfltHostnameCmp, *pline)) != RS_RET_OK)
				return(iRet);
		}
	}
	return RS_RET_OK;
}


/*
 * Helper to cfline(). This function interprets a BSD tag selector line
 * from the config file ("!tagname"). It stores it for further reference.
 * rgerhards 2005-10-18
 */
static rsRetVal cflineProcessTagSelector(uchar **pline)
{
	rsRetVal iRet;

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(**pline == '!');

	dbgprintf(" - programname selector line\n");

	(*pline)++;	/* eat '!' */

	/* the below is somewhat of a quick hack, but it is efficient (this is
	 * why it is in here. "!*" resets the tag selector with BSD syslog. We mimic
	 * this, too. As it is easy to check that condition, we do not fire up a
	 * parser process, just make sure we do not address beyond our space.
	 * Order of conditions in the if-statement is vital! rgerhards 2005-10-18
	 */
	if(**pline != '\0' && **pline == '*' && *(*pline+1) == '\0') {
		dbgprintf("resetting programname filter\n");
		if(pDfltProgNameCmp != NULL) {
			if((iRet = rsCStrSetSzStr(pDfltProgNameCmp, NULL)) != RS_RET_OK)
				return(iRet);
		}
	} else {
		dbgprintf("setting programname filter to '%s'\n", *pline);
		if(pDfltProgNameCmp == NULL) {
			/* create string for parser */
			if((iRet = rsCStrConstructFromszStr(&pDfltProgNameCmp, *pline)) != RS_RET_OK)
				return(iRet);
		} else { /* string objects exists, just update... */
			if((iRet = rsCStrSetSzStr(pDfltProgNameCmp, *pline)) != RS_RET_OK)
				return(iRet);
		}
	}
	return RS_RET_OK;
}


/* add an Action to the current selector
 * The pOMSR is freed, as it is not needed after this function.
 * Note: this function pulls global data that specifies action config state.
 * rgerhards, 2007-07-27
 */
rsRetVal addAction(action_t **ppAction, modInfo_t *pMod, void *pModData, omodStringRequest_t *pOMSR, int bSuspended)
{
	DEFiRet;
	int i;
	int iTplOpts;
	uchar *pTplName;
	action_t *pAction;
	char errMsg[512];

	assert(ppAction != NULL);
	assert(pMod != NULL);
	assert(pOMSR != NULL);
	dbgprintf("Module %s processed this config line.\n", modGetName(pMod));

	CHKiRet(actionConstruct(&pAction)); /* create action object first */
	pAction->pMod = pMod;
	pAction->pModData = pModData;
	pAction->bExecWhenPrevSusp = bActExecWhenPrevSusp;

	/* check if we can obtain the template pointers - TODO: move to separat function? */
	pAction->iNumTpls = OMSRgetEntryCount(pOMSR);
	assert(pAction->iNumTpls >= 0); /* only debug check because this "can not happen" */
	/* please note: iNumTpls may validly be zero. This is the case if the module
	 * does not request any templates. This sounds unlikely, but an actual example is
	 * the discard action, which does not require a string. -- rgerhards, 2007-07-30
	 */
	if(pAction->iNumTpls > 0) {
		/* we first need to create the template pointer array */
		if((pAction->ppTpl = calloc(pAction->iNumTpls, sizeof(struct template *))) == NULL) {
			glblHadMemShortage = 1;
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
		/* and now the array for doAction() message pointers */
		if((pAction->ppMsgs = calloc(pAction->iNumTpls, sizeof(uchar *))) == NULL) {
			glblHadMemShortage = 1;
			ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
		}
	}
	
	for(i = 0 ; i < pAction->iNumTpls ; ++i) {
		CHKiRet(OMSRgetEntry(pOMSR, i, &pTplName, &iTplOpts));
		/* Ok, we got everything, so it now is time to look up the
		 * template (Hint: templates MUST be defined before they are
		 * used!)
		 */
		if((pAction->ppTpl[i] = tplFind((char*)pTplName, strlen((char*)pTplName))) == NULL) {
			snprintf(errMsg, sizeof(errMsg) / sizeof(char),
				 " Could not find template '%s' - action disabled\n",
				 pTplName);
			errno = 0;
			logerror(errMsg);
			ABORT_FINALIZE(RS_RET_NOT_FOUND);
		}
		/* check required template options */
		if(   (iTplOpts & OMSR_RQD_TPL_OPT_SQL)
		   && (pAction->ppTpl[i]->optFormatForSQL == 0)) {
			errno = 0;
			logerror("Action disabled. To use this action, you have to specify "
				"the SQL or stdSQL option in your template!\n");
			ABORT_FINALIZE(RS_RET_RQD_TPLOPT_MISSING);
		}

		dbgprintf("template: '%s' assigned\n", pTplName);
	}

	pAction->pMod = pMod;
	pAction->pModData = pModData;
	/* now check if the module is compatible with select features */
	if(pMod->isCompatibleWithFeature(sFEATURERepeatedMsgReduction) == RS_RET_OK)
		pAction->f_ReduceRepeated = bReduceRepeatMsgs;
	else {
		dbgprintf("module is incompatible with RepeatedMsgReduction - turned off\n");
		pAction->f_ReduceRepeated = 0;
	}
	pAction->bEnabled = 1; /* action is enabled */

	if(bSuspended)
		actionSuspend(pAction);

	CHKiRet(actionConstructFinalize(pAction));
	
	/* TODO: if we exit here, we have a memory leak... */

	*ppAction = pAction; /* finally store the action pointer */

finalize_it:
	if(iRet == RS_RET_OK)
		iRet = OMSRdestruct(pOMSR);
	else {
		/* do not overwrite error state! */
		OMSRdestruct(pOMSR);
		if(pAction != NULL)
			actionDestruct(pAction);
	}

	RETiRet;
}


/* read the filter part of a configuration line and store the filter
 * in the supplied selector_t
 * rgerhards, 2007-08-01
 */
static rsRetVal cflineDoFilter(uchar **pp, selector_t *f)
{
	DEFiRet;

	assert(pp != NULL);
	assert(f != NULL);

	/* check which filter we need to pull... */
	switch(**pp) {
		case ':':
			iRet = cflineProcessPropFilter(pp, f);
			break;
		default:
			iRet = cflineProcessTradPRIFilter(pp, f);
			break;
	}

	/* we now check if there are some global (BSD-style) filter conditions
	 * and, if so, we copy them over. rgerhards, 2005-10-18
	 */
	if(pDfltProgNameCmp != NULL)
		if((iRet = rsCStrConstructFromCStr(&(f->pCSProgNameComp), pDfltProgNameCmp)) != RS_RET_OK)
			return(iRet);

	if(eDfltHostnameCmpMode != HN_NO_COMP) {
		f->eHostnameCmpMode = eDfltHostnameCmpMode;
		if((iRet = rsCStrConstructFromCStr(&(f->pCSHostnameComp), pDfltHostnameCmp)) != RS_RET_OK)
			return(iRet);
	}

	RETiRet;
}


/* process the action part of a selector line
 * rgerhards, 2007-08-01
 */
static rsRetVal cflineDoAction(uchar **p, action_t **ppAction)
{
	DEFiRet;
	modInfo_t *pMod;
	omodStringRequest_t *pOMSR;
	action_t *pAction;
	void *pModData;

	assert(p != NULL);
	assert(ppAction != NULL);

	/* loop through all modules and see if one picks up the line */
	pMod = modGetNxtType(NULL, eMOD_OUT);
	while(pMod != NULL) {
		iRet = pMod->mod.om.parseSelectorAct(p, &pModData, &pOMSR);
		dbgprintf("tried selector action for %s: %d\n", modGetName(pMod), iRet);
		if(iRet == RS_RET_OK || iRet == RS_RET_SUSPENDED) {
			if((iRet = addAction(&pAction, pMod, pModData, pOMSR, (iRet == RS_RET_SUSPENDED)? 1 : 0)) == RS_RET_OK) {
				/* now check if the module is compatible with select features */
				if(pMod->isCompatibleWithFeature(sFEATURERepeatedMsgReduction) == RS_RET_OK)
					pAction->f_ReduceRepeated = bReduceRepeatMsgs;
				else {
					dbgprintf("module is incompatible with RepeatedMsgReduction - turned off\n");
					pAction->f_ReduceRepeated = 0;
				}
				pAction->bEnabled = 1; /* action is enabled */
			}
			break;
		}
		else if(iRet != RS_RET_CONFLINE_UNPROCESSED) {
			/* In this case, the module would have handled the config
			 * line, but some error occured while doing so. This error should
			 * already by reported by the module. We do not try any other
			 * modules on this line, because we found the right one.
			 * rgerhards, 2007-07-24
			 */
			dbgprintf("error %d parsing config line\n", (int) iRet);
			break;
		}
		pMod = modGetNxtType(pMod, eMOD_OUT);
	}

	*ppAction = pAction;
	RETiRet;
}


/* helper to selectorAddListCheckActions()
 * This is the fucntion to be executed by llExecFunc
 */
DEFFUNC_llExecFunc(selectorAddListCheckActionsChecker)
{
	DEFiRet;
	action_t *pAction = (action_t *) pData;

	assert(pAction != NULL);

	if(pAction->pMod->needUDPSocket(pAction->pModData) == RS_RET_TRUE) {
		Forwarding++;
	}

	RETiRet;
}

/* loop through a list of actions and perform necessary checks and
 * housekeeping. This function must only be called when the owning
 * selector_t looks valid and is not likely to be discarded. However,
 * if we do not return RS_RET_OK, the caller MUST discard the
 * owning selector_t. -- rgerhards, 2007-08-02
*/
static rsRetVal selectorAddListCheckActions(selector_t *f)
{
	DEFiRet;

	assert(f != NULL);

	CHKiRet(llExecFunc(&f->llActList, selectorAddListCheckActionsChecker, NULL));

finalize_it:
	RETiRet;
}


/* add a completely-processed selector (after config line parsing) to
 * the linked list of selectors. We now need to check
 * if it has any actions associated and, if so, link it to the linked
 * list. If it has nothing associated with it, we can simply discard
 * it.
 * We have one special case during initialization: then, the current
 * selector is NULL, which means we do not need to care about it at
 * all.  -- rgerhards, 2007-08-01
 */
static rsRetVal selectorAddList(selector_t *f)
{
	DEFiRet;
	int iActionCnt;

	static selector_t *nextp = NULL; /* TODO: make this go away (see comment below) */

	if(f != NULL) {
		CHKiRet(llGetNumElts(&f->llActList, &iActionCnt));
		if(iActionCnt == 0) {
			logerror("warning: selector line without actions will be discarded");
			selectorDestruct(f);
		} else {
			if((iRet = selectorAddListCheckActions(f)) != RS_RET_OK) {
				logerror("selector line will be discarded due to error in action(s)");
				selectorDestruct(f);
				goto finalize_it;
			}
			/* successfully created an entry */
			dbgprintf("selector line successfully processed\n");
			/* TODO: we should use the linked list class for the selector list, else we need to add globals
			 * ... well nextp could be added temporarily...
			 * Thanks to varmojfekoj for having the idea to just use "Files" to make this
			 * code work. I had actually forgotten to fix the code here before moving to 1.18.0.
			 * And, of course, I also did not migrate the selector_t structure to the linked list class.
			 * However, that should still be one of the very next things to happen.
			 * rgerhards, 2007-08-06
			 */
			if(Files == NULL) {
				Files = f;
			} else {
				nextp->f_next = f;
			}
			nextp = f;
		}
	}

finalize_it:
	RETiRet;
}


/* Process a configuration file line in traditional "filter selector" format
 */
static rsRetVal cflineClassic(uchar *p, selector_t **pfCurr)
{
	DEFiRet;
	action_t *pAction;
	selector_t *fCurr;

	assert(pfCurr != NULL);

	fCurr = *pfCurr;

	/* lines starting with '&' have no new filters and just add
	 * new actions to the currently processed selector.
	 */
	if(*p == '&') {
		++p; /* eat '&' */
		skipWhiteSpace(&p); /* on to command */
	} else {
		/* we are finished with the current selector. So we now need to check
		 * if it has any actions associated and, if so, link it to the linked
		 * list. If it has nothing associated with it, we can simply discard
		 * it. In any case, we create a fresh selector for our new filter.
		 * We have one special case during initialization: then, the current
		 * selector is NULL, which means we do not need to care about it at
		 * all.  -- rgerhards, 2007-08-01
		 */
		CHKiRet(selectorAddList(fCurr));
		CHKiRet(selectorConstruct(&fCurr)); /* create "fresh" selector */
		CHKiRet(cflineDoFilter(&p, fCurr)); /* pull filters */
	}

	CHKiRet(cflineDoAction(&p, &pAction));
	CHKiRet(llAppend(&fCurr->llActList,  NULL, (void*) pAction));

finalize_it:
	*pfCurr = fCurr;
	RETiRet;
}


/* process a configuration line
 * I re-did this functon because it was desperately time to do so
 * rgerhards, 2007-08-01
 */
static rsRetVal cfline(uchar *line, selector_t **pfCurr)
{
	DEFiRet;

	assert(line != NULL);

	dbgprintf("cfline: '%s'\n", line);

	/* check type of line and call respective processing */
	switch(*line) {
		case '!':
			iRet = cflineProcessTagSelector(&line);
			break;
		case '+':
		case '-':
			iRet = cflineProcessHostSelector(&line);
			break;
		case '$':
			++line; /* eat '$' */
			iRet = cfsysline(line);
			break;
		default:
			iRet = cflineClassic(line, pfCurr);
			break;
	}

	RETiRet;
}


/* set the main message queue mode
 * rgerhards, 2008-01-03
 */
static rsRetVal setMainMsgQueType(void __attribute__((unused)) *pVal, uchar *pszType)
{
	DEFiRet;

	if (!strcasecmp((char *) pszType, "fixedarray")) {
		MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		dbgprintf("main message queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		MainMsgQueType = QUEUETYPE_LINKEDLIST;
		dbgprintf("main message queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		MainMsgQueType = QUEUETYPE_DISK;
		dbgprintf("main message queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		MainMsgQueType = QUEUETYPE_DIRECT;
		dbgprintf("main message queue type set to DIRECT (no queueing at all)\n");
	} else {
		logerrorSz("unknown mainmessagequeuetype parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	free(pszType); /* no longer needed */

	RETiRet;
}


/*  Decode a symbolic name to a numeric value
 */
int decode(uchar *name, struct code *codetab)
{
	register struct code *c;
	register uchar *p;
	uchar buf[80];

	assert(name != NULL);
	assert(codetab != NULL);

	dbgprintf("symbolic name: %s", name);
	if (isdigit((int) *name))
	{
		dbgprintf("\n");
		return (atoi((char*) name));
	}
	strncpy((char*) buf, (char*) name, 79);
	for (p = buf; *p; p++)
		if (isupper((int) *p))
			*p = tolower((int) *p);
	for (c = codetab; c->c_name; c++)
		if (!strcmp((char*) buf, (char*) c->c_name))
		{
			dbgprintf(" ==> %d\n", c->c_val);
			return (c->c_val);
		}
	return (-1);
}



/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tell the main loop to go through a restart.
 */
void sighup_handler()
{
	struct sigaction sigAct;
	
	restart = 1;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);

	return;
}


/**
 * getSubString
 *
 * Copy a string byte by byte until the occurrence  
 * of a given separator.
 *
 * \param ppSrc		Pointer to a pointer of the source array of characters. If a
			separator detected the Pointer points to the next char after the
			separator. Except if the end of the string is dedected ('\n'). 
			Then it points to the terminator char. 
 * \param pDst		Pointer to the destination array of characters. Here the substing
			will be stored.
 * \param DstSize	Maximum numbers of characters to store.
 * \param cSep		Separator char.
 * \ret int		Returns 0 if no error occured.
 */
int getSubString(uchar **ppSrc,  char *pDst, size_t DstSize, char cSep)
{
	uchar *pSrc = *ppSrc;
	int iErr = 0; /* 0 = no error, >0 = error */
	while(*pSrc != cSep && *pSrc != '\n' && *pSrc != '\0' && DstSize>1) {
		*pDst++ = *(pSrc)++;
		DstSize--;
	}
	/* check if the Dst buffer was to small */
	if (*pSrc != cSep && *pSrc != '\n' && *pSrc != '\0')
	{ 
		dbgprintf("in getSubString, error Src buffer > Dst buffer\n");
		iErr = 1;
	}	
	if (*pSrc == '\0' || *pSrc == '\n')
		/* this line was missing, causing ppSrc to be invalid when it
		 * was returned in case of end-of-string. rgerhards 2005-07-29
		 */
		*ppSrc = pSrc;
	else
		*ppSrc = pSrc+1;
	*pDst = '\0';
	return iErr;
}


/* this function pulls all internal messages from the buffer
 * and puts them into the processing engine.
 * We can only do limited error handling, as this would not
 * really help us. TODO: add error messages?
 * rgerhards, 2007-08-03
 */
static void processImInternal(void)
{
	int iPri;
	int iFlags;
	msg_t *pMsg;

	while(iminternalRemoveMsg(&iPri, &pMsg, &iFlags) == RS_RET_OK) {
		logmsg(iPri, pMsg, iFlags);
	}
}


/* This is the main processing loop. It is called after successful initialization.
 * When it returns, the syslogd terminates.
 * Its sole function is to provide some housekeeping things. The real work is done
 * by the other threads spawned.
 */
static void
mainloop(void)
{
	struct timeval tvSelectTimeout;

	BEGINfunc
	while(!bFinished){
		/* first check if we have any internal messages queued and spit them out */
		/* TODO: do we need this any longer? I doubt it, but let's care about it
 		 * later -- rgerhards, 2007-12-21
 		 */
		processImInternal();

		/* this is now just a wait */
		tvSelectTimeout.tv_sec = TIMERINTVL;
		tvSelectTimeout.tv_usec = 0;
		select(1, NULL, NULL, NULL, &tvSelectTimeout);
		if(bFinished)
			break;	/* exit as quickly as possible - see long comment below */

		/* If we received a HUP signal, we call doFlushRptdMsgs() a bit early. This
 		 * doesn't matter, because doFlushRptdMsgs() checks timestamps. What may happen,
 		 * however, is that the too-early call may lead to a bit too-late output
 		 * of "last message repeated n times" messages. But that is quite acceptable.
 		 * rgerhards, 2007-12-21
		 * ... and just to explain, we flush here because that is exactly what the mainloop
		 * shall do - provide a periodic interval in which not-yet-flushed messages will
		 * be flushed. Be careful, there is a potential race condition: doFlushRptdMsgs()
		 * needs to aquire a lock on the action objects. If, however, long-running consumers
		 * cause the main queue worker threads to lock them for a long time, we may receive
		 * a starvation condition, resulting in the mainloop being held on lock for an extended
		 * period of time. That, in turn, could lead to unresponsiveness to termination
		 * requests. It is especially important that the bFinished flag is checked before
		 * doFlushRptdMsgs() is called (I know because I ran into that situation). I am
		 * not yet sure if the remaining probability window of a termination-related
		 * problem is large enough to justify changing the code - I would consider it
		 * extremely unlikely that the problem ever occurs in practice. Fixing it would
		 * require not only a lot of effort but would cost considerable performance. So
		 * for the time being, I think the remaining risk can be accepted.
		 * rgerhards, 2008-01-10
 		 */
		doFlushRptdMsgs();

		if(restart) {
			dbgprintf("\nReceived SIGHUP, reloading rsyslogd.\n");
			/* main queue is stopped as part of init() */
			init();
			restart = 0;
			continue;
		}
	}
	ENDfunc
}

/* If user is not root, prints warnings or even exits 
 * TODO: check all dynafiles for write permission
 * ... but it is probably better to wait here until we have
 * a module interface - rgerhards, 2007-07-23
 */
static void checkPermissions()
{
#if 0
	/* TODO: this function must either be redone or removed - now with the input modules,
	 * there is no such simple check we can do. What we can check, however, is if there is
	 * any input module active and terminate, if not. -- rgerhards, 2007-12-26
	 */
	/* we are not root */
	if (geteuid() != 0)
	{
		fputs("WARNING: Local messages will not be logged! If you want to log them, run rsyslog as root.\n",stderr); 
#ifdef SYSLOG_INET	
		/* udp enabled and port number less than or equal to 1024 */
		if ( AcceptRemote && (atoi(LogPort) <= 1024) )
			fprintf(stderr, "WARNING: Will not listen on UDP port %s. Use port number higher than 1024 or run rsyslog as root!\n", LogPort);
		
		/* tcp enabled and port number less or equal to 1024 */
		if( bEnableTCP   && (atoi(TCPLstnPort) <= 1024) )
			fprintf(stderr, "WARNING: Will not listen on TCP port %s. Use port number higher than 1024 or run rsyslog as root!\n", TCPLstnPort);

		/* Neither explicit high UDP port nor explicit high TCP port.
                 * It is useless to run anymore */
		if( !(AcceptRemote && (atoi(LogPort) > 1024)) && !( bEnableTCP && (atoi(TCPLstnPort) > 1024)) )
		{
#endif
			fprintf(stderr, "ERROR: Nothing to log, no reason to run. Please run rsyslog as root.\n");
			exit(EXIT_FAILURE);
#ifdef SYSLOG_INET
		}
#endif
	}
#endif
}


/* load build-in modules
 * very first version begun on 2007-07-23 by rgerhards
 */
static rsRetVal loadBuildInModules(void)
{
	DEFiRet;

	if((iRet = doModInit(modInitFile, (uchar*) "builtin-file", NULL)) != RS_RET_OK) {
		RETiRet;
	}
#ifdef SYSLOG_INET
	if((iRet = doModInit(modInitFwd, (uchar*) "builtin-fwd", NULL)) != RS_RET_OK) {
		RETiRet;
	}
#endif
	if((iRet = doModInit(modInitShell, (uchar*) "builtin-shell", NULL)) != RS_RET_OK) {
		RETiRet;
	}
	if((iRet = doModInit(modInitDiscard, (uchar*) "builtin-discard", NULL)) != RS_RET_OK) {
		RETiRet;
	}

	/* dirty, but this must be for the time being: the usrmsg module must always be
	 * loaded as last module. This is because it processes any time of action selector.
	 * If we load it before other modules, these others will never have a chance of
	 * working with the config file. We may change that implementation so that a user name
	 * must start with an alnum, that would definitely help (but would it break backwards
	 * compatibility?). * rgerhards, 2007-07-23
	 * User names now must begin with:
	 *   [a-zA-Z0-9_.]
	 */
	if((iRet = doModInit(modInitUsrMsg, (uchar*) "builtin-usrmsg", NULL)) != RS_RET_OK)
		RETiRet;

	/* ok, initialization of the command handler probably does not 100% belong right in
	 * this space here. However, with the current design, this is actually quite a good
	 * place to put it. We might decide to shuffle it around later, but for the time
	 * being, the code has found its home here. A not-just-sideeffect of this decision
	 * is that rsyslog will terminate if we can not register our built-in config commands.
	 * This, I think, is the right thing to do. -- rgerhards, 2007-07-31
	 */
	CHKiRet(regCfSysLineHdlr((uchar *)"workdirectory", 0, eCmdHdlrGetWord, NULL, &pszWorkDir, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeretrycount", 0, eCmdHdlrInt, NULL, &glbliActionResumeRetryCount, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuefilename", 0, eCmdHdlrGetWord, NULL, &pszMainMsgQFName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesize", 0, eCmdHdlrInt, NULL, &iMainMsgQueueSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuehighwatermark", 0, eCmdHdlrInt, NULL, &iMainMsgQHighWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuelowwatermark", 0, eCmdHdlrInt, NULL, &iMainMsgQLowWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardmark", 0, eCmdHdlrInt, NULL, &iMainMsgQDiscardMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardseverity", 0, eCmdHdlrInt, NULL, &iMainMsgQDiscardSeverity, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuecheckpointinterval", 0, eCmdHdlrInt, NULL, &iMainMsgQPersistUpdCnt, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetype", 0, eCmdHdlrGetWord, setMainMsgQueType, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreads", 0, eCmdHdlrInt, NULL, &iMainMsgQueueNumWorkers, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutshutdown", 0, eCmdHdlrInt, NULL, &iMainMsgQtoQShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutactioncompletion", 0, eCmdHdlrInt, NULL, &iMainMsgQtoActShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutenqueue", 0, eCmdHdlrInt, NULL, &iMainMsgQtoEnq, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutworkerthreadshutdown", 0, eCmdHdlrInt, NULL, &iMainMsgQtoWrkShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreadminimummessages", 0, eCmdHdlrInt, NULL, &iMainMsgQWrkMinMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuemaxfilesize", 0, eCmdHdlrSize, NULL, &iMainMsgQueMaxFileSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesaveonshutdown", 0, eCmdHdlrBinary, NULL, &bMainMsgQSaveOnShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"repeatedmsgreduction", 0, eCmdHdlrBinary, NULL, &bReduceRepeatMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlywhenpreviousissuspended", 0, eCmdHdlrBinary, NULL, &bActExecWhenPrevSusp, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeinterval", 0, eCmdHdlrInt, setActionResumeInterval, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"controlcharacterescapeprefix", 0, eCmdHdlrGetChar, NULL, &cCCEscapeChar, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"escapecontrolcharactersonreceive", 0, eCmdHdlrBinary, NULL, &bEscapeCCOnRcv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"dropmsgswithmaliciousdnsptrrecords", 0, eCmdHdlrBinary, NULL, &bDropMalPTRMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"droptrailinglfonreception", 0, eCmdHdlrBinary, NULL, &bDropTrailingLF, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"template", 0, eCmdHdlrCustomHandler, doNameLine, (void*)DIR_TEMPLATE, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"outchannel", 0, eCmdHdlrCustomHandler, doNameLine, (void*)DIR_OUTCHANNEL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"allowedsender", 0, eCmdHdlrCustomHandler, doNameLine, (void*)DIR_ALLOWEDSENDER, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"modload", 0, eCmdHdlrCustomHandler, doModLoad, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"includeconfig", 0, eCmdHdlrCustomHandler, doIncludeLine, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"umask", 0, eCmdHdlrFileCreateMode, setUmask, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprinttemplatelist", 0, eCmdHdlrBinary, NULL, &bDebugPrintTemplateList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintmodulelist", 0, eCmdHdlrBinary, NULL, &bDebugPrintModuleList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintcfsyslinehandlerlist", 0, eCmdHdlrBinary,
		 NULL, &bDebugPrintCfSysLineHandlerList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"moddir", 0, eCmdHdlrGetWord, NULL, &pModDir, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));

	/* now add other modules handlers (we should work on that to be able to do it in ClassInit(), but so far
	 * that is not possible). -- rgerhards, 2008-01-28
	 */
	CHKiRet(actionAddCfSysLineHdrl());

finalize_it:
	RETiRet;
}


/* print version and compile-time setting information.
 */
static void printVersion(void)
{
	printf("rsyslogd %s, ", VERSION);
	printf("compiled with:\n");
#ifdef FEATURE_REGEXP
	printf("\tFEATURE_REGEXP:\t\t\t\tYes\n");
#else
	printf("\tFEATURE_REGEXP:\t\t\t\tNo\n");
#endif
#ifndef	NOLARGEFILE
	printf("\tFEATURE_LARGEFILE:\t\t\tYes\n");
#else
	printf("\tFEATURE_LARGEFILE:\t\t\tNo\n");
#endif
#ifdef	USE_NETZIP
	printf("\tFEATURE_NETZIP (message compression):\tYes\n");
#else
	printf("\tFEATURE_NETZIP (message compression):\tNo\n");
#endif
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
	printf("\tGSSAPI Kerberos 5 support:\t\tYes\n");
#else
	printf("\tGSSAPI Kerberos 5 support:\t\tNo\n");
#endif
#ifndef	NDEBUG
	printf("\tFEATURE_DEBUG (debug build, slow code):\tYes\n");
#else
	printf("\tFEATURE_DEBUG (debug build, slow code):\tNo\n");
#endif
#ifdef	RTINST
	printf("\tRuntime Instrumentation (slow code):\tYes\n");
#else
	printf("\tRuntime Instrumentation (slow code):\tNo\n");
#endif
	printf("\nSee http://www.rsyslog.com for more information.\n");
}


/* This function is called after initial initalization. It is used to
 * move code out of the too-long main() function.
 * rgerhards, 2007-10-17
 */
static void mainThread()
{
	DEFiRet;
	uchar *pTmp;

	/* doing some core initializations */
	if((iRet = modInitIminternal()) != RS_RET_OK) {
		fprintf(stderr, "fatal error: could not initialize errbuf object (error code %d).\n",
			iRet);
		exit(1); /* "good" exit, leaving at init for fatal error */
	}

	if((iRet = loadBuildInModules()) != RS_RET_OK) {
		fprintf(stderr, "fatal error: could not activate built-in modules. Error code %d.\n",
			iRet);
		exit(1); /* "good" exit, leaving at init for fatal error */
	}

	/* Note: signals MUST be processed by the thread this code is running in. The reason
	 * is that we need to interrupt the select() system call. -- rgerhards, 2007-10-17
	 */

	/* initialize the default templates
	 * we use template names with a SP in front - these 
	 * can NOT be generated via the configuration file
	 */
	pTmp = template_TraditionalFormat;
	tplAddLine(" TradFmt", &pTmp);
	pTmp = template_WallFmt;
	tplAddLine(" WallFmt", &pTmp);
	pTmp = template_StdFwdFmt;
	tplAddLine(" StdFwdFmt", &pTmp);
	pTmp = template_StdUsrMsgFmt;
	tplAddLine(" StdUsrMsgFmt", &pTmp);
	pTmp = template_StdDBFmt;
	tplAddLine(" StdDBFmt", &pTmp);
        pTmp = template_StdPgSQLFmt;
        tplLastStaticInit(tplAddLine(" StdPgSQLFmt", &pTmp));

	dbgprintf("Starting.\n");
	init();
	if(Debug) {
		dbgprintf("Debugging enabled, SIGUSR1 to turn off debugging.\n");
		debugging_on = 1;
	}
	/* Send a signal to the parent so it can terminate.
	 */
	if (myPid != ppid)
		kill (ppid, SIGTERM);

	/* END OF INTIALIZATION
	 * ... but keep in mind that we might do a restart and thus init() might
	 * be called again. If that happens, we must shut down the worker thread,
	 * do the init() and then restart things.
	 * rgerhards, 2005-10-24
	 */

	mainloop();
	ENDfunc
}


/* Method to initialize all global classes.
 * rgerhards, 2008-01-04
 */
static rsRetVal InitGlobalClasses(void)
{
	DEFiRet;

	CHKiRet(objClassInit()); /* *THIS* *MUST* always be the first class initilizere called! */
	CHKiRet(MsgClassInit());
	CHKiRet(strmClassInit());
	CHKiRet(wtiClassInit());
	CHKiRet(wtpClassInit());
	CHKiRet(queueClassInit());

finalize_it:
	RETiRet;
}



/* This is the main entry point into rsyslogd. Over time, we should try to
 * modularize it a bit more...
 */
int realMain(int argc, char **argv)
{	
	DEFiRet;

	register int i;
	register char *p;
	int num_fds;
	int ch;
	struct hostent *hent;
	extern int optind;
	extern char *optarg;
	struct sigaction sigAct;

#ifdef	MTRACE
	mtrace(); /* this is a debug aid for leak detection - either remove
	           * or put in conditional compilation. 2005-01-18 RGerhards */
#endif

	CHKiRet(InitGlobalClasses());

	ppid = getpid();

	if(chdir ("/") != 0)
		fprintf(stderr, "Can not do 'cd /' - still trying to run\n");

	/* END core initializations */

	while ((ch = getopt(argc, argv, "46Ac:dehi:f:g:l:m:nqQr::s:t:u:vwx")) != EOF) {
		switch((char)ch) {
                case '4':
	                family = PF_INET;
                        break;
                case '6':
                        family = PF_INET6;
                        break;
                case 'A':
                        send_to_all++;
                        break;
		case 'c':		/* compatibility mode */
			iCompatibilityMode = atoi(optarg);
			break;
		case 'd':		/* debug */
			Debug = 1;
			break;
		case 'e':		/* log every message (no repeat message supression) */
			logEveryMsg = 1;
			break;
		case 'f':		/* configuration file */
			ConfFile = (uchar*) optarg;
			break;
		case 'g':		/* enable tcp gssapi logging */
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
			if (!bEnableTCP)
				configureTCPListen(optarg);
			bEnableTCP |= ALLOWEDMETHOD_GSS;
#else
			fprintf(stderr, "rsyslogd: -g not valid - not compiled with gssapi support");
#endif
			break;
		case 'h':
			NoHops = 0;
			break;
		case 'i':		/* pid file name */
			PidFile = optarg;
			break;
		case 'l':
			if (LocalHosts) {
				fprintf (stderr, "rsyslogd: Only one -l argument allowed," \
					"the first one is taken.\n");
			} else {
				LocalHosts = crunch_list(optarg);
			}
			break;
		case 'm':		/* mark interval */
			if(iCompatibilityMode < 3)
				MarkInterval = atoi(optarg) * 60;
			else
				fprintf(stderr,
					"-m option only supported in compatibility modes 0 to 2 - ignored\n");
			break;
		case 'n':		/* don't fork */
			NoFork = 1;
			break;
		case 'q':               /* add hostname if DNS resolving has failed */
		        ACLAddHostnameOnFail = 1;
		        break;
		case 'Q':               /* dont resolve hostnames in ACL to IPs */
		        ACLDontResolve = 1;
		        break;
		case 'r':		/* accept remote messages */
#ifdef SYSLOG_INET
#if 0
			AcceptRemote = 1;
			if(optarg == NULL)
				LogPort = "0";
			else
				LogPort = optarg;
#endif
#else
			fprintf(stderr, "rsyslogd: -r not valid - not compiled with network support");
#endif
			break;
		case 's':
			if (StripDomains) {
				fprintf (stderr, "rsyslogd: Only one -s argument allowed," \
					"the first one is taken.\n");
			} else {
				StripDomains = crunch_list(optarg);
			}
			break;
		case 't':		/* enable tcp logging */
#ifdef SYSLOG_INET
			if (!bEnableTCP)
				configureTCPListen(optarg);
			bEnableTCP |= ALLOWEDMETHOD_TCP;
#else
			fprintf(stderr, "rsyslogd: -t not valid - not compiled with network support");
#endif
			break;
		case 'u':		/* misc user settings */
			if(atoi(optarg) == 1)
				bParseHOSTNAMEandTAG = 0;
			break;
		case 'v':
			printVersion();
			exit(0); /* exit for -v option - so this is a "good one" */
		case 'w':		/* disable disallowed host warnigs */
			option_DisallowWarning = 0;
			break;
		case 'x':		/* disable dns for remote messages */
			DisableDNS = 1;
			break;
		case '?':              
		default:
			usage();
		}
	}

	if ((argc -= optind))
		usage();

	/* TODO: this should go away at a reasonable stage of v3 development.
	 * rgerhards, 2007-12-19
	 */
	if(iCompatibilityMode < 3) {
		fprintf(stderr, "Warning: compatibility modes < 3 are currently NOT supported - continuing...\n");
	}

	checkPermissions();
	thrdInit();

	if ( !(Debug || NoFork) )
	{
		dbgprintf("Checking pidfile.\n");
		if (!check_pid(PidFile))
		{
			memset(&sigAct, 0, sizeof (sigAct));
			sigemptyset(&sigAct.sa_mask);
			sigAct.sa_handler = doexit;
			sigaction(SIGTERM, &sigAct, NULL);

			if (fork()) {
				/*
				 * Parent process
				 */
				sleep(300);
				/*
				 * Not reached unless something major went wrong.  5
				 * minutes should be a fair amount of time to wait.
				 * Please note that this procedure is important since
				 * the father must not exit before syslogd isn't
				 * initialized or the klogd won't be able to flush its
				 * logs.  -Joey
				 */
				exit(1); /* "good" exit - after forking, not diasabling anything */
			}
			num_fds = getdtablesize();
			for (i= 0; i < num_fds; i++)
				(void) close(i);
			untty();
		}
		else
		{
			fputs(" Already running.\n", stderr);
			exit(1); /* "good" exit, done if syslogd is already running */
		}
	}
	else
		debugging_on = 1;

	dbgprintf("Compatibility Mode: %d\n", iCompatibilityMode);

	/* tuck my process id away */
	dbgprintf("Writing pidfile %s.\n", PidFile);
	if (!check_pid(PidFile))
	{
		if (!write_pid(PidFile))
		{
			fputs("Can't write pid.\n", stderr);
			exit(1); /* exit during startup - questionable */
		}
	}
	else
	{
		fputs("Pidfile (and pid) already exist.\n", stderr);
		exit(1); /* exit during startup - questionable */
	}
	myPid = getpid(); 	/* save our pid for further testing (also used for messages) */


	gethostname(LocalHostName, sizeof(LocalHostName));
	if ( (p = strchr(LocalHostName, '.')) ) {
		*p++ = '\0';
		LocalDomain = p;
	}
	else
	{
		LocalDomain = "";

		/* It's not clearly defined whether gethostname()
		 * should return the simple hostname or the fqdn. A
		 * good piece of software should be aware of both and
		 * we want to distribute good software.  Joey
		 *
		 * Good software also always checks its return values...
		 * If syslogd starts up before DNS is up & /etc/hosts
		 * doesn't have LocalHostName listed, gethostbyname will
		 * return NULL. 
		 */
		/* TODO: gethostbyname() is not thread-safe, but replacing it is
		 * not urgent as we do not run on multiple threads here. rgerhards, 2007-09-25
		 */
		hent = gethostbyname(LocalHostName);
		if(hent) {
			snprintf(LocalHostName, sizeof(LocalHostName), "%s", hent->h_name);
				
			if ( (p = strchr(LocalHostName, '.')) )
			{
				*p++ = '\0';
				LocalDomain = p;
			}
		}
	}

	/* Convert to lower case to recognize the correct domain laterly
	 */
	for (p = (char *)LocalDomain; *p ; p++)
		if (isupper((int) *p))
			*p = (char)tolower((int)*p);

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);

	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGSEGV, &sigAct, NULL);
	sigAct.sa_handler = doDie;
	sigaction(SIGTERM, &sigAct, NULL);
	sigAct.sa_handler = Debug ? doDie : SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigaction(SIGQUIT, &sigAct, NULL);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);
	sigAct.sa_handler = Debug ? debug_switch : SIG_IGN;
	/* TODO: use signal 2 */
	/*sigaction(SIGUSR1, &sigAct, NULL);*/
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);
	sigaction(SIGXFSZ, &sigAct, NULL); /* do not abort if 2gig file limit is hit */

	mainThread();

	/* do any de-init's that need to be done AFTER this comment */

	die(bFinished);
	
	thrdExit();

finalize_it:
	if(iRet != RS_RET_OK)
		fprintf(stderr, "rsyslogd run failed with error %d.\n", iRet);

	ENDfunc
	return 0;
}


/* This is the main entry point into rsyslogd. This must be a function in its own
 * right in order to intialize the debug system in a portable way (otherwise we would
 * need to have a statement before variable definitions.
 * rgerhards, 20080-01-28
 */
int main(int argc, char **argv)
{	
	dbgClassInit();
	return realMain(argc, argv);
}


/*
 * vi:set ai:
 */
