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
 * Please note that while rsyslog started from the sysklogd code base,
 * it nowadays has almost nothing left in common with it. Allmost all
 * parts of the code have been rewritten.
 *
 * This Project was intiated and is maintained by
 * Rainer Gerhards <rgerhards@hq.adiscon.com>.
 *
 * For further information, please see http://www.rsyslog.com
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

#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define TIMERINTVL	30		/* interval for checking flush, mark */

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
#include <assert.h>

#ifdef OS_SOLARIS
#	include <errno.h>
#	include <fcntl.h>
#	include <stropts.h>
#	include <sys/termios.h>
#	include <sys/types.h>
#else
#	include <libgen.h>
#	include <sys/errno.h>
#endif

#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <grp.h>

#if HAVE_SYS_TIMESPEC_H
# include <sys/timespec.h>
#endif

#if HAVE_SYS_STAT_H
#	include <sys/stat.h>
#endif

#include <signal.h>

#if HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef USE_NETZIP
#include <zlib.h>
#endif

#include <netdb.h>

#include "pidfile.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "msg.h"
#include "modules.h"
#include "action.h"
#include "iminternal.h"
#include "cfsysline.h"
#include "omshell.h"
#include "omusrmsg.h"
#include "omfwd.h"
#include "omfile.h"
#include "omdiscard.h"
#include "threads.h"
#include "wti.h"
#include "queue.h"
#include "stream.h"
#include "conf.h"
#include "errmsg.h"
#include "datetime.h"
#include "parser.h"
//#include "sysvar.h"
#include "batch.h"
#include "unicode-helper.h"
#include "ruleset.h"
#include "rule.h"
#include "net.h"
#include "vm.h"
#include "prop.h"

/* definitions for objects we access */
DEFobjCurrIf(obj)
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(conf)
DEFobjCurrIf(expr)
DEFobjCurrIf(module)
DEFobjCurrIf(errmsg)
DEFobjCurrIf(rule)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(prop)
DEFobjCurrIf(net) /* TODO: make go away! */


/* forward definitions */
static rsRetVal GlobalClassExit(void);


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
#	undef _PATH_LOGPID
#	if defined(FSSTND)
#		ifdef OS_BSD
#			define _PATH_VARRUN "/var/run/"
#		endif
#		if defined(__sun) || defined(__hpux)
#			define _PATH_VARRUN "/var/run/"
#		endif
#		define _PATH_LOGPID _PATH_VARRUN SYSLOGD_PIDNAME
#	else
#		define _PATH_LOGPID "/etc/" SYSLOGD_PIDNAME
#	endif
#else
#	ifndef _PATH_LOGPID
#		if defined(__sun) || defined(__hpux)
#			define _PATH_VARRUN "/var/run/"
#		endif
#		if defined(FSSTND)
#			define _PATH_LOGPID _PATH_VARRUN "rsyslogd.pid"
#		else
#			define _PATH_LOGPID "/etc/rsyslogd.pid"
#		endif
#	endif
#endif

#ifndef _PATH_TTY
#	define _PATH_TTY	"/dev/tty"
#endif

static prop_t *pInternalInputName = NULL;	/* there is only one global inputName for all internally-generated messages */
static prop_t *pLocalHostIP = NULL;		/* there is only one global IP for all internally-generated messages */
static uchar	*ConfFile = (uchar*) _PATH_LOGCONF; /* read-only after startup */
static char	*PidFile = _PATH_LOGPID; /* read-only after startup */

static pid_t myPid;	/* our pid for use in self-generated messages, e.g. on startup */
/* mypid is read-only after the initial fork() */
static int bHadHUP = 0; /* did we have a HUP? */

static int bParseHOSTNAMEandTAG = 1; /* global config var: should the hostname and tag be
                                      * parsed inside message - rgerhards, 2006-03-13 */
static int bFinished = 0;	/* used by termination signal handler, read-only except there
				 * is either 0 or the number of the signal that requested the
 				 * termination.
				 */
static int iConfigVerify = 0;	/* is this just a config verify run? */

/* Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 * TODO: this shall go into action object! -- rgerhards, 2008-01-29
 */
int	repeatinterval[2] = { 30, 60 };	/* # of secs before flush */

#define LIST_DELIMITER	':'		/* delimiter between two hosts */

static pid_t ppid; /* This is a quick and dirty hack used for spliting main/startup thread */

typedef struct legacyOptsLL_s {
	uchar *line;
	struct legacyOptsLL_s *next;
} legacyOptsLL_t;
legacyOptsLL_t *pLegacyOptsLL = NULL;

/* global variables for config file state */
int	bDropTrailingLF = 1; /* drop trailing LF's on reception? */
int	iCompatibilityMode = 0;		/* version we should be compatible with; 0 means sysklogd. It is
					   the default, so if no -c<n> option is given, we make ourselvs
					   as compatible to sysklogd as possible. */
#define DFLT_bLogStatusMsgs 1
static int	bLogStatusMsgs = DFLT_bLogStatusMsgs;	/* log rsyslog start/stop/HUP messages? */
static int	bDebugPrintTemplateList = 1;/* output template list in debug mode? */
static int	bDebugPrintCfSysLineHandlerList = 1;/* output cfsyslinehandler list in debug mode? */
static int	bDebugPrintModuleList = 1;/* output module list in debug mode? */
uchar	cCCEscapeChar = '\\';/* character to be used to start an escape sequence for control chars */
int 	bEscapeCCOnRcv = 1; /* escape control characters on reception: 0 - no, 1 - yes */
static int	bErrMsgToStderr = 1; /* print error messages to stderr (in addition to everything else)? */
int 	bReduceRepeatMsgs; /* reduce repeated message - 0 - no, 1 - yes */
int 	bAbortOnUncleanConfig = 0; /* abort run (rather than starting with partial config) if there was any issue in conf */
int	bActExecWhenPrevSusp; /* execute action only when previous one was suspended? */
int	iActExecOnceInterval = 0; /* execute action once every nn seconds */
/* end global config file state variables */

int	MarkInterval = 20 * 60;	/* interval between marks in seconds - read-only after startup */
int      send_to_all = 0;        /* send message to all IPv4/IPv6 addresses */
static int	NoFork = 0; 	/* don't fork - don't run in daemon mode - read-only after startup */
static int	bHaveMainQueue = 0;/* set to 1 if the main queue - in queueing mode - is available
				 * If the main queue is either not yet ready or not running in 
				 * queueing mode (mode DIRECT!), then this is set to 0.
				 */
static int uidDropPriv = 0;	/* user-id to which priveleges should be dropped to (AFTER init()!) */
static int gidDropPriv = 0;	/* group-id to which priveleges should be dropped to (AFTER init()!) */

extern	int errno;

static uchar *pszConfDAGFile = NULL;				/* name of config DAG file, non-NULL means generate one */
/* main message queue and its configuration parameters */
static qqueue_t *pMsgQueue = NULL;				/* the main message queue */
static int iMainMsgQueueSize = 10000;				/* size of the main message queue above */
static int iMainMsgQHighWtrMark = 8000;				/* high water mark for disk-assisted queues */
static int iMainMsgQLowWtrMark = 2000;				/* low water mark for disk-assisted queues */
static int iMainMsgQDiscardMark = 9800;				/* begin to discard messages */
static int iMainMsgQDiscardSeverity = 8;			/* by default, discard nothing to prevent unintentional loss */
static int iMainMsgQueueNumWorkers = 1;				/* number of worker threads for the mm queue above */
static queueType_t MainMsgQueType = QUEUETYPE_FIXED_ARRAY;	/* type of the main message queue above */
static uchar *pszMainMsgQFName = NULL;				/* prefix for the main message queue file */
static int64 iMainMsgQueMaxFileSize = 1024*1024;
static int iMainMsgQPersistUpdCnt = 0;				/* persist queue info every n updates */
static int bMainMsgQSyncQeueFiles = 0;				/* sync queue files on every write? */
static int iMainMsgQtoQShutdown = 1500;				/* queue shutdown (ms) */ 
static int iMainMsgQtoActShutdown = 1000;			/* action shutdown (in phase 2) */ 
static int iMainMsgQtoEnq = 2000;				/* timeout for queue enque */ 
static int iMainMsgQtoWrkShutdown = 60000;			/* timeout for worker thread shutdown */
static int iMainMsgQWrkMinMsgs = 100;				/* minimum messages per worker needed to start a new one */
static int iMainMsgQDeqSlowdown = 0;				/* dequeue slowdown (simple rate limiting) */
static int64 iMainMsgQueMaxDiskSpace = 0;			/* max disk space allocated 0 ==> unlimited */
static int iMainMsgQueDeqBatchSize = 32;			/* dequeue batch size */
static int bMainMsgQSaveOnShutdown = 1;				/* save queue on shutdown (when DA enabled)? */
static int iMainMsgQueueDeqtWinFromHr = 0;			/* hour begin of time frame when queue is to be dequeued */
static int iMainMsgQueueDeqtWinToHr = 25;			/* hour begin of time frame when queue is to be dequeued */


/* support for simple textual representation of FIOP names
 * rgerhards, 2005-09-27
 */
char*
getFIOPName(unsigned iFIOP)
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
	bLogStatusMsgs = DFLT_bLogStatusMsgs;
	bActExecWhenPrevSusp = 0;
	iActExecOnceInterval = 0;
	bDebugPrintTemplateList = 1;
	bDebugPrintCfSysLineHandlerList = 1;
	bDebugPrintModuleList = 1;
	bEscapeCCOnRcv = 1; /* default is to escape control characters */
	bReduceRepeatMsgs = 0;
	bAbortOnUncleanConfig = 0;
	free(pszMainMsgQFName);
	pszMainMsgQFName = NULL;
	iMainMsgQueueSize = 10000;
	iMainMsgQHighWtrMark = 8000;
	iMainMsgQLowWtrMark = 2000;
	iMainMsgQDiscardMark = 9800;
	iMainMsgQDiscardSeverity = 8;
	iMainMsgQueMaxFileSize = 1024 * 1024;
	iMainMsgQueueNumWorkers = 1;
	iMainMsgQPersistUpdCnt = 0;
	bMainMsgQSyncQeueFiles = 0;
	iMainMsgQtoQShutdown = 1500;
	iMainMsgQtoActShutdown = 1000;
	iMainMsgQtoEnq = 2000;
	iMainMsgQtoWrkShutdown = 60000;
	iMainMsgQWrkMinMsgs = 100;
	iMainMsgQDeqSlowdown = 0;
	bMainMsgQSaveOnShutdown = 1;
	MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
	iMainMsgQueMaxDiskSpace = 0;
	iMainMsgQueDeqBatchSize = 32;
	glbliActionResumeRetryCount = 0;

	return RS_RET_OK;
}


/* hardcoded standard templates (used for defaults) */
static uchar template_DebugFormat[] = "\"Debug line with all properties:\nFROMHOST: '%FROMHOST%', fromhost-ip: '%fromhost-ip%', HOSTNAME: '%HOSTNAME%', PRI: %PRI%,\nsyslogtag '%syslogtag%', programname: '%programname%', APP-NAME: '%APP-NAME%', PROCID: '%PROCID%', MSGID: '%MSGID%',\nTIMESTAMP: '%TIMESTAMP%', STRUCTURED-DATA: '%STRUCTURED-DATA%',\nmsg: '%msg%'\nescaped msg: '%msg:::drop-cc%'\nrawmsg: '%rawmsg%'\n\n\"";
static uchar template_SyslogProtocol23Format[] = "\"<%PRI%>1 %TIMESTAMP:::date-rfc3339% %HOSTNAME% %APP-NAME% %PROCID% %MSGID% %STRUCTURED-DATA% %msg%\n\"";
static uchar template_TraditionalFileFormat[] = "\"%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n\"";
static uchar template_FileFormat[] = "\"%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n\"";
static uchar template_WallFmt[] = "\"\r\n\7Message from syslogd@%HOSTNAME% at %timegenerated% ...\r\n %syslogtag%%msg%\n\r\"";
static uchar template_ForwardFormat[] = "\"<%PRI%>%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%\"";
static uchar template_TraditionalForwardFormat[] = "\"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag:1:32%%msg:::sp-if-no-1st-sp%%msg%\"";
static uchar template_StdUsrMsgFmt[] = "\" %syslogtag%%msg%\n\r\"";
static uchar template_StdDBFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')\",SQL";
static uchar template_StdPgSQLFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-pgsql%', '%timegenerated:::date-pgsql%', %iut%, '%syslogtag%')\",STDSQL";
/* end template */


/* up to the next comment, prototypes that should be removed by reordering */
/* Function prototypes. */
static char **crunch_list(char *list);
static void reapchild();
static void debug_switch();
static void sighup_handler();
static void processImInternal(void);


static int usage(void)
{
	fprintf(stderr, "usage: rsyslogd [-c<version>] [-46AdnqQvwx] [-l<hostlist>] [-s<domainlist>]\n"
			"                [-f<conffile>] [-i<pidfile>] [-N<level>] [-M<module load path>]\n"
			"                [-u<number>]\n"
			"To run rsyslogd in native mode, use \"rsyslogd -c3 <other options>\"\n\n"
			"For further information see http://www.rsyslog.com/doc\n");
	exit(1); /* "good" exit - done to terminate usage() */
}


/* ------------------------------ some support functions for imdiag ------------------------------ *
 * This is a bit dirty, but the only way to do it, at least with reasonable effort.
 * rgerhards, 2009-05-25
 */

/* return back the approximate current number of messages in the main message queue
 */
rsRetVal
diagGetMainMsgQSize(int *piSize)
{
	DEFiRet;
	assert(piSize != NULL);
	*piSize = pMsgQueue->iQueueSize;
	RETiRet;
}


/* ------------------------------ end support functions for imdiag  ------------------------------ */


/* rgerhards, 2005-10-24: crunch_list is called only during option processing. So
 * it is never called once rsyslogd is running. This code
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
		DBGPRINTF("#%d: %s\n", count, StripDomains[count++]);
#endif
	return result;
}


void untty(void)
#ifdef HAVE_SETSID
{
	if(!Debug) {
		setsid();
	}
	return;
}
#else
{
	int i;

	if(!Debug) {
		i = open(_PATH_TTY, O_RDWR|O_CLOEXEC);
		if (i >= 0) {
#			if !defined(__hpux)
				(void) ioctl(i, (int) TIOCNOTTY, NULL);
#			else
				/* TODO: we need to implement something for HP UX! -- rgerhards, 2008-03-04 */
				/* actually, HP UX should have setsid, so the code directly above should
				 * trigger. So the actual question is why it doesn't do that...
				 */
#			endif
			close(i);
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
 * rgerhards: 2008-03-06: added "flags" to allow an input module to specify
 * flags, most importantly to request ignoring the messages' timestamp.
 *
 * rgerhards, 2008-03-19:
 * I added an additional calling parameter to permit specifying the flow
 * control capability of the source.
 *
 * rgerhards, 2008-05-16:
 * I added an additional calling parameter (hnameIP) to enable specifying the IP
 * of a remote host.
 *
 * rgerhards, 2008-09-11:
 * Interface change: added new parameter "InputName", permits the input to provide 
 * a string that identifies it. May be NULL, but must be a valid char* pointer if
 * non-NULL.
 *
 * rgerhards, 2008-10-06:
 * Interface change: added new parameter "stTime", which enables the caller to provide
 * a timestamp that is to be used as timegenerated instead of the current system time.
 * This is meant to facilitate performance optimization. Some inputs support such modes.
 * If stTime is NULL, the current system time is used.
 *
 * rgerhards, 2008-10-09:
 * interface change: bParseHostname removed, now in flags
 */
static inline rsRetVal printline(uchar *hname, uchar *hnameIP, uchar *msg, int flags, flowControl_t flowCtlType,
	prop_t *pInputName, struct syslogTime *stTime, time_t ttGenTime)
{
	DEFiRet;
	register uchar *p;
	int pri;
	msg_t *pMsg;
	prop_t *propFromHost = NULL;
	prop_t *propFromHostIP = NULL;

	/* Now it is time to create the message object (rgerhards) */
	if(stTime == NULL) {
		CHKiRet(msgConstruct(&pMsg));
	} else {
		CHKiRet(msgConstructWithTime(&pMsg, stTime, ttGenTime));
	}
	if(pInputName != NULL)
		MsgSetInputName(pMsg, pInputName);
	MsgSetFlowControlType(pMsg, flowCtlType);
	MsgSetRawMsgWOSize(pMsg, (char*)msg);
	
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
	if((pMsg->msgFlags & PARSE_HOSTNAME) == 0)
		MsgSetHOSTNAME(pMsg, hname, ustrlen(hname));
	MsgSetRcvFromStr(pMsg, hname, ustrlen(hname), &propFromHost);
	CHKiRet(MsgSetRcvFromIPStr(pMsg, hnameIP, ustrlen(hnameIP), &propFromHostIP));
	MsgSetAfterPRIOffs(pMsg, p - msg);
	prop.Destruct(&propFromHost);
	prop.Destruct(&propFromHostIP);

	logmsg(pMsg, flags);

finalize_it:
	RETiRet;
}


/* This takes a received message that must be decoded and submits it to
 * the main message queue. The function calls the necessary parser.
 *
 * rgerhards, 2006-11-30: I have greatly changed this function. Formerly,
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
 *
 * rgerhards, 2008-02-18:
 * This function was previously called "printchopped"() and has been renamed
 * as part of the effort to create a clean internal message submission interface.
 * It also has been adopted to our usual calling interface, but currently does
 * not provide any useful return states. But we now have the hook and things can
 * improve in the future. <-- TODO!
 *
 * rgerhards, 2008-03-19:
 * I added an additional calling parameter to permit specifying the flow
 * control capability of the source.
 *
 * rgerhards, 2008-05-16:
 * I added an additional calling parameter (hnameIP) to enable specifying the IP
 * of a remote host.
 *
 * rgerhards, 2008-09-11:
 * Interface change: added new parameter "InputName", permits the input to provide 
 * a string that identifies it. May be NULL, but must be a valid char* pointer if
 * non-NULL.
 *
 * rgerhards, 2008-10-06:
 * Interface change: added new parameter "stTime", which enables the caller to provide
 * a timestamp that is to be used as timegenerated instead of the current system time.
 * This is meant to facilitate performance optimization. Some inputs support such modes.
 * If stTime is NULL, the current system time is used.
 *
 * rgerhards, 2008-10-09:
 * interface change: bParseHostname removed, now in flags
 */
rsRetVal
parseAndSubmitMessage(uchar *hname, uchar *hnameIP, uchar *msg, int len, int flags, flowControl_t flowCtlType,
	prop_t *pInputName, struct syslogTime *stTime, time_t ttGenTime)
{
	DEFiRet;
	register int iMsg;
	uchar *pMsg;
	uchar *pData;
	uchar *pEnd;
	int iMaxLine;
	uchar *tmpline = NULL;
#	ifdef USE_NETZIP
	uchar *deflateBuf = NULL;
	uLongf iLenDefBuf;
#	endif

	assert(hname != NULL);
	assert(hnameIP != NULL);
	assert(msg != NULL);
	assert(len >= 0);

	/* we first allocate work buffers large enough to hold the configured maximum
	 * size of a message. Over time, we should change this to a more optimal way, i.e.
	 * by calling the function with the actual length of the message to be parsed.
	 * rgerhards, 2008-09-02
	 *
	 * TODO: optimize buffer handling */
	iMaxLine = glbl.GetMaxLine();
	CHKmalloc(tmpline = malloc(sizeof(uchar) * (iMaxLine + 1)));

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
		DBGPRINTF("dropped NUL at very end of message\n");
		len--;
	}

	/* then we check if we need to drop trailing LFs, which often make
	 * their way into syslog messages unintentionally. In order to remain
	 * compatible to recent IETF developments, we allow the user to
	 * turn on/off this handling.  rgerhards, 2007-07-23
	 */
	if(bDropTrailingLF && *(msg + len - 1) == '\n') {
		DBGPRINTF("dropped LF at very end of message (DropTrailingLF is set)\n");
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
		 * message size of iMaxLine. If it is larger, an error message is logged
		 * and the message is dropped. We do NOT try to decompress larger messages
		 * as such might be used for denial of service. It might happen to later
		 * builds that such functionality be added as an optional, operator-configurable
		 * feature.
		 */
		int ret;
		iLenDefBuf = iMaxLine;
		CHKmalloc(deflateBuf = malloc(sizeof(uchar) * (iMaxLine + 1)));
		ret = uncompress((uchar *) deflateBuf, &iLenDefBuf, (uchar *) msg+1, len-1);
		DBGPRINTF("Compressed message uncompressed with status %d, length: new %ld, old %d.\n",
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
			errmsg.LogError(0, NO_ERRCODE, "Uncompression of a message failed with return code %d "
			            "- enable debug logging if you need further information. "
				    "Message ignored.", ret);
			FINALIZE; /* unconditional exit, nothing left to do... */
		}
		pData = deflateBuf;
		pEnd = deflateBuf + iLenDefBuf;
	}
#	else /* ifdef USE_NETZIP */
	/* in this case, we still need to check if the message is compressed. If so, we must
	 * tell the user we can not accept it.
	 */
	if(len > 0 && *msg == 'z') {
		errmsg.LogError(0, NO_ERRCODE, "Received a compressed message, but rsyslogd does not have compression "
		         "support enabled. The message will be ignored.");
		FINALIZE;
	}	
#	endif /* ifdef USE_NETZIP */

	while(pData < pEnd) {
		if(iMsg >= iMaxLine) {
			/* emergency, we now need to flush, no matter if
			 * we are at end of message or not...
			 */
			if(iMsg == iMaxLine) {
				*(pMsg + iMsg) = '\0'; /* space *is* reserved for this! */
				printline(hname, hnameIP, tmpline, flags, flowCtlType, pInputName, stTime, ttGenTime);
			} else {
				/* This case in theory never can happen. If it happens, we have
				 * a logic error. I am checking for it, because if I would not,
				 * we would address memory invalidly with the code above. I
				 * do not care much about this case, just a debug log entry
				 * (I couldn't do any more smart things anyway...).
				 * rgerhards, 2007-9-20
				 */
				DBGPRINTF("internal error: iMsg > max msg size in printchopped()\n");
			}
			FINALIZE; /* in this case, we are done... nothing left we can do */
		}
		if(*pData == '\0') { /* guard against \0 characters... */
			/* changed to the sequence (somewhat) proposed in
			 * draft-ietf-syslog-protocol-19. rgerhards, 2006-11-30
			 */
			if(iMsg + 3 < iMaxLine) { /* do we have space? */
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
			if(iMsg + 3 < iMaxLine) { /* do we have space? */
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
	printline(hname, hnameIP, tmpline, flags, flowCtlType, pInputName, stTime, ttGenTime);

finalize_it:
	if(tmpline != NULL)
		free(tmpline);
#	ifdef USE_NETZIP
	if(deflateBuf != NULL)
		free(deflateBuf);
#	endif
	RETiRet;
}


/* this is a special function used to submit an error message. This
 * function is also passed to the runtime library as the generic error
 * message handler. -- rgerhards, 2008-04-17
 */
rsRetVal
submitErrMsg(int iErr, uchar *msg)
{
	DEFiRet;
	iRet = logmsgInternal(iErr, LOG_SYSLOG|LOG_ERR, msg, 0);
	RETiRet;
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
logmsgInternal(int iErr, int pri, uchar *msg, int flags)
{
	uchar pszTag[33];
	msg_t *pMsg;
	DEFiRet;

	CHKiRet(msgConstruct(&pMsg));
	MsgSetInputName(pMsg, pInternalInputName);
	MsgSetRawMsgWOSize(pMsg, (char*)msg);
	MsgSetHOSTNAME(pMsg, glbl.GetLocalHostName(), ustrlen(glbl.GetLocalHostName()));
	MsgSetRcvFrom(pMsg, glbl.GetLocalHostNameProp());
	MsgSetRcvFromIP(pMsg, pLocalHostIP);
	/* check if we have an error code associated and, if so,
	 * adjust the tag. -- rgerhards, 2008-06-27
	 */
	if(iErr == NO_ERRCODE) {
		MsgSetTAG(pMsg, UCHAR_CONSTANT("rsyslogd:"), sizeof("rsyslogd:") - 1);
	} else {
		size_t len = snprintf((char*)pszTag, sizeof(pszTag), "rsyslogd%d:", iErr);
		pszTag[32] = '\0'; /* just to make sure... */
		MsgSetTAG(pMsg, pszTag, len);
	}
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	pMsg->bParseHOSTNAME = 0;
	flags |= INTERNAL_MSG;

	/* we now check if we should print internal messages out to stderr. This was
	 * suggested by HKS as a way to help people troubleshoot rsyslog configuration
	 * (by running it interactively. This makes an awful lot of sense, so I add
	 * it here. -- rgerhards, 2008-07-28
	 * Note that error messages can not be disable during a config verify. This
	 * permits us to process unmodified config files which otherwise contain a
	 * supressor statement.
	 */
	if(((Debug || NoFork) && bErrMsgToStderr) || iConfigVerify) {
		if(LOG_PRI(pri) == LOG_ERR)
			fprintf(stderr, "rsyslogd: %s\n", msg);
	}

	if(bHaveMainQueue == 0) { /* not yet in queued mode */
		iminternalAddMsg(pri, pMsg, flags);
	} else {
		/* we have the queue, so we can simply provide the 
		 * message to the queue engine.
		 */
		logmsg(pMsg, flags);
	}
finalize_it:
	RETiRet;
}


/* The consumer of dequeued messages. This function is called by the
 * queue engine on dequeueing of a message. It runs on a SEPARATE
 * THREAD. It receives an array of pointers, which it must iterate
 * over. We do not do any further batching, as this is of no benefit
 * for the main queue.
 */
static rsRetVal
msgConsumer(void __attribute__((unused)) *notNeeded, batch_t *pBatch)
{
	int i;
	msg_t *pMsg;
	DEFiRet;

	assert(pBatch != NULL);

	for(i = 0 ; i < pBatch->nElem ; i++) {
		pMsg = (msg_t*) pBatch->pElem[i].pUsrp;
		DBGPRINTF("msgConsumer processes msg %d/%d\n", i, pBatch->nElem);
		if((pMsg->msgFlags & NEEDS_PARSING) != 0) {
			parseMsg(pMsg);
		}
		ruleset.ProcessMsg(pMsg);
	}

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
static int parseRFCField(uchar **pp2parse, uchar *pResult)
{
	uchar *p2parse;
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
static int parseRFCStructuredData(uchar **pp2parse, uchar *pResult)
{
	uchar *p2parse;
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

	if(*p2parse == '-') { /* empty structured data? */
		*pResult++ = '-';
		++p2parse;
	} else {
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

/* parse a RFC5424-formatted syslog message. This function returns
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
int parseRFCSyslogMsg(msg_t *pMsg, int flags)
{
	uchar *p2parse;
	uchar *pBuf;
	int bContParse = 1;

	BEGINfunc
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */

	/* do a sanity check on the version and eat it */
	assert(p2parse[0] == '1' && p2parse[1] == ' ');
	p2parse += 2;

	/* Now get us some memory we can use as a work buffer while parsing.
	 * We simply allocated a buffer sufficiently large to hold all of the
	 * message, so we can not run into any troubles. I think this is
	 * more wise then to use individual buffers.
	 */
	if((pBuf = malloc(sizeof(uchar) * ustrlen(p2parse) + 1)) == NULL)
		return 1;
		
	/* IMPORTANT NOTE:
	 * Validation is not actually done below nor are any errors handled. I have
	 * NOT included this for the current proof of concept. However, it is strongly
	 * advisable to add it when this code actually goes into production.
	 * rgerhards, 2005-11-24
	 */

	/* TIMESTAMP */
	if(datetime.ParseTIMESTAMP3339(&(pMsg->tTIMESTAMP),  &p2parse) == RS_RET_OK) {
		if(flags & IGNDATE) {
			/* we need to ignore the msg data, so simply copy over reception date */
			memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
		}
	} else {
		DBGPRINTF("no TIMESTAMP detected!\n");
		bContParse = 0;
	}

	/* HOSTNAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetHOSTNAME(pMsg, pBuf, ustrlen(pBuf));
	}

	/* APP-NAME */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetAPPNAME(pMsg, (char*)pBuf);
	}

	/* PROCID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetPROCID(pMsg, (char*)pBuf);
	}

	/* MSGID */
	if(bContParse) {
		parseRFCField(&p2parse, pBuf);
		MsgSetMSGID(pMsg, (char*)pBuf);
	}

	/* STRUCTURED-DATA */
	if(bContParse) {
		parseRFCStructuredData(&p2parse, pBuf);
		MsgSetStructuredData(pMsg, (char*)pBuf);
	}

	/* MSG */
	MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);

	free(pBuf);
	ENDfunc
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
int parseLegacySyslogMsg(msg_t *pMsg, int flags)
{
	uchar *p2parse;
	int bTAGCharDetected;
	int i;	/* general index for parsing */
	uchar bufParseTAG[CONF_TAG_MAXSIZE];
	uchar bufParseHOSTNAME[CONF_TAG_HOSTNAME];
	BEGINfunc

	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);
	p2parse = pMsg->pszRawMsg + pMsg->offAfterPRI; /* point to start of text, after PRI */

	/* Check to see if msg contains a timestamp. We start by assuming
	 * that the message timestamp is the time of reciption (which we 
	 * generated ourselfs and then try to actually find one inside the
	 * message. There we go from high-to low precison and are done
	 * when we find a matching one. -- rgerhards, 2008-09-16
	 */
	if(datetime.ParseTIMESTAMP3339(&(pMsg->tTIMESTAMP), &p2parse) == RS_RET_OK) {
		/* we are done - parse pointer is moved by ParseTIMESTAMP3339 */;
	} else if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse) == RS_RET_OK) {
		/* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
	} else if(*p2parse == ' ') { /* try to see if it is slighly malformed - HP procurve seems to do that sometimes */
		++p2parse;	/* move over space */
		if(datetime.ParseTIMESTAMP3164(&(pMsg->tTIMESTAMP), &p2parse) == RS_RET_OK) {
			/* indeed, we got it! */
			/* we are done - parse pointer is moved by ParseTIMESTAMP3164 */;
		} else {/* parse pointer needs to be restored, as we moved it off-by-one
			 * for this try.
			 */
			--p2parse;
		}
	}

	if(flags & IGNDATE) {
		/* we need to ignore the msg data, so simply copy over reception date */
		memcpy(&pMsg->tTIMESTAMP, &pMsg->tRcvdAt, sizeof(struct syslogTime));
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
		 * rgerhards, 2009-06-23: and I now have extended this logic to every character
		 * that is not a valid hostname.
		 */
		bTAGCharDetected = 0;
		if(flags & PARSE_HOSTNAME) {
			i = 0;
			while((isalnum(p2parse[i]) || p2parse[i] == '.' || p2parse[i] == '.'
				|| p2parse[i] == '_' || p2parse[i] == '-') && i < CONF_TAG_MAXSIZE) {
				bufParseHOSTNAME[i] = p2parse[i];
				++i;
			}

			if(i > 0 && p2parse[i] == ' ' && isalnum(p2parse[i-1])) {
				/* we got a hostname! */
				p2parse += i + 1; /* "eat" it (including SP delimiter) */
				bufParseHOSTNAME[i] = '\0';
				MsgSetHOSTNAME(pMsg, bufParseHOSTNAME, i);
			}
		}

		/* now parse TAG - that should be present in message from all sources.
		 * This code is somewhat not compliant with RFC 3164. As of 3164,
		 * the TAG field is ended by any non-alphanumeric character. In
		 * practice, however, the TAG often contains dashes and other things,
		 * which would end the TAG. So it is not desirable. As such, we only
		 * accept colon and SP to be terminators. Even there is a slight difference:
		 * a colon is PART of the TAG, while a SP is NOT part of the tag
		 * (it is CONTENT). Starting 2008-04-04, we have removed the 32 character
		 * size limit (from RFC3164) on the tag. This had bad effects on existing
		 * envrionments, as sysklogd didn't obey it either (probably another bug
		 * in RFC3164...). We now receive the full size, but will modify the
		 * outputs so that only 32 characters max are used by default.
		 */
		i = 0;
		while(*p2parse && *p2parse != ':' && *p2parse != ' ' && i < CONF_TAG_MAXSIZE) {
			bufParseTAG[i++] = *p2parse++;
		}
		if(*p2parse == ':') {
			++p2parse; 
			bufParseTAG[i++] = ':';
		}

		/* no TAG can only be detected if the message immediatly ends, in which case an empty TAG
		 * is considered OK. So we do not need to check for empty TAG. -- rgerhards, 2009-06-23
		 */
		bufParseTAG[i] = '\0';	/* terminate string */
		MsgSetTAG(pMsg, bufParseTAG, i);
	} else {/* we enter this code area when the user has instructed rsyslog NOT
		 * to parse HOSTNAME and TAG - rgerhards, 2006-03-13
		 */
		if(!(flags & INTERNAL_MSG)) {
			DBGPRINTF("HOSTNAME and TAG not parsed by user configuraton.\n");
		}
	}

	/* The rest is the actual MSG */
	MsgSetMSGoffs(pMsg, p2parse - pMsg->pszRawMsg);

	ENDfunc
	return 0; /* all ok */
}


/* submit a message to the main message queue.   This is primarily
 * a hook to prevent the need for callers to know about the main message queue
 * (which may change in the future as we will probably have multiple rule
 * sets and thus queues...).
 * rgerhards, 2008-02-13
 */
rsRetVal
submitMsg(msg_t *pMsg)
{
	DEFiRet;

	ISOBJ_TYPE_assert(pMsg, msg);
	
	MsgPrepareEnqueue(pMsg);
	qqueueEnqObj(pMsgQueue, pMsg->flowCtlType, (void*) pMsg);

	RETiRet;
}


/* submit multiple messages at once, very similar to submitMsg, just
 * for multi_submit_t.
 * rgerhards, 2009-06-16
 */
rsRetVal
multiSubmitMsg(multi_submit_t *pMultiSub)
{
	int i;
	DEFiRet;
	assert(pMultiSub != NULL);

	for(i = 0 ; i < pMultiSub->nElem ; ++i) {
		MsgPrepareEnqueue(pMultiSub->ppMsgs[i]);
	}

	iRet = qqueueMultiEnqObj(pMsgQueue, pMultiSub);
	pMultiSub->nElem = 0;

	RETiRet;
}


/* Log a message to the appropriate log files, users, etc. based on
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
logmsg(msg_t *pMsg, int flags)
{
	char *msg;

	BEGINfunc
	assert(pMsg != NULL);
	assert(pMsg->pszRawMsg != NULL);

	msg = (char*) pMsg->pszRawMsg + pMsg->offAfterPRI;  /* point to start of text, after PRI */
	DBGPRINTF("logmsg: flags %x, from '%s', msg %s\n", flags, getRcvFrom(pMsg), msg);

	/* rger 2005-11-24 (happy thanksgiving!): we now need to check if we have
	 * a traditional syslog message or one formatted according to syslog-protocol.
	 * We need to apply different parsers depending on that. We use the
	 * -protocol VERSION field for the detection.
	 */
	if(msg[0] == '1' && msg[1] == ' ') {
		DBGPRINTF("Message has syslog-protocol format.\n");
		setProtocolVersion(pMsg, 1);
		if(parseRFCSyslogMsg(pMsg, flags) == 1) {
			msgDestruct(&pMsg);
			return;
		}
	} else { /* we have legacy syslog */
		DBGPRINTF("Message has legacy syslog format.\n");
		setProtocolVersion(pMsg, 0);
		if(parseLegacySyslogMsg(pMsg, flags) == 1) {
			msgDestruct(&pMsg);
			return;
		}
	}

	/* ---------------------- END PARSING ---------------- */
	
	/* now submit the message to the main queue - then we are done */
	pMsg->msgFlags = flags;
	MsgPrepareEnqueue(pMsg);
	qqueueEnqObj(pMsgQueue, pMsg->flowCtlType, (void*) pMsg);
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
	
	BEGINfunc
	LockObj(pAction);
	/* TODO: time() performance: the call below could be moved to
	 * the beginn of the llExec(). This makes it slightly less correct, but
	 * in an acceptable way. -- rgerhards, 2008-09-16
	 */
	if (pAction->f_prevcount && time(NULL) >= REPEATTIME(pAction)) {
		DBGPRINTF("flush %s: repeated %d times, %d sec.\n",
		    module.GetStateName(pAction->pMod), pAction->f_prevcount,
		    repeatinterval[pAction->f_repeatcount]);
		actionWriteToAction(pAction);
		BACKOFF(pAction);
	}
	UnlockObj(pAction);

	ENDfunc
	return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This method flushes repeat messages.
 */
static void
doFlushRptdMsgs(void)
{
	ruleset.IterateAllActions(flushRptdMsgsActions, NULL);
}


static void debug_switch()
{
	struct sigaction sigAct;

	if(debugging_on == 0) {
		debugging_on = 1;
		DBGPRINTF("Switching debugging_on to true\n");
	} else {
		DBGPRINTF("Switching debugging_on to false\n");
		debugging_on = 0;
	}
	
	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = debug_switch;
	sigaction(SIGUSR1, &sigAct, NULL);
}


void legacyOptsEnq(uchar *line)
{
	legacyOptsLL_t *pNew;

	pNew = malloc(sizeof(legacyOptsLL_t));
	if(line == NULL)
		pNew->line = NULL;
	else
		pNew->line = (uchar *) strdup((char *) line);
	pNew->next = NULL;

	if(pLegacyOptsLL == NULL)
		pLegacyOptsLL = pNew;
	else {
		legacyOptsLL_t *pThis = pLegacyOptsLL;

		while(pThis->next != NULL)
			pThis = pThis->next;
		pThis->next = pNew;
	}
}


void legacyOptsFree(void)
{
	legacyOptsLL_t *pThis = pLegacyOptsLL, *pNext;

	while(pThis != NULL) {
		if(pThis->line != NULL)
			free(pThis->line);
		pNext = pThis->next;
		free(pThis);
		pThis = pNext;
	}
}


void legacyOptsHook(void)
{
	legacyOptsLL_t *pThis = pLegacyOptsLL;

	while(pThis != NULL) {
		if(pThis->line != NULL) {
			errno = 0;
			errmsg.LogError(0, NO_ERRCODE, "Warning: backward compatibility layer added to following "
				        "directive to rsyslog.conf: %s", pThis->line);
			conf.cfsysline(pThis->line);
		}
		pThis = pThis->next;
	}
}


void legacyOptsParseTCP(char ch, char *arg)
{
	register int i;
	register char *pArg = arg;
	static char conflict = '\0';

	if((conflict == 'g' && ch == 't') || (conflict == 't' && ch == 'g')) {
		fprintf(stderr, "rsyslogd: If you want to use both -g and -t, use directives instead, -%c ignored.\n", ch);
		return;
	} else
		conflict = ch;

	/* extract port */
	i = 0;
	while(isdigit((int) *pArg))
		i = i * 10 + *pArg++ - '0';

	/* number of sessions */
	if(*pArg == '\0' || *pArg == ',') {
		if(ch == 't')
			legacyOptsEnq((uchar *) "ModLoad imtcp");
		else if(ch == 'g')
			legacyOptsEnq((uchar *) "ModLoad imgssapi");

		if(i >= 0 && i <= 65535) {
			uchar line[30];

			if(ch == 't') {
				snprintf((char *) line, sizeof(line), "InputTCPServerRun %d", i);
			} else if(ch == 'g') {
				snprintf((char *) line, sizeof(line), "InputGSSServerRun %d", i);
			}
			legacyOptsEnq(line);
		} else {
			if(ch == 't') {
				fprintf(stderr, "rsyslogd: Invalid TCP listen port %d - changed to 514.\n", i);
				legacyOptsEnq((uchar *) "InputTCPServerRun 514");
			} else if(ch == 'g') {
				fprintf(stderr, "rsyslogd: Invalid GSS listen port %d - changed to 514.\n", i);
				legacyOptsEnq((uchar *) "InputGSSServerRun 514");
			}
		}

		if(*pArg == ',') {
			++pArg;
			while(isspace((int) *pArg))
				++pArg;
			i = 0;
			while(isdigit((int) *pArg)) {
				i = i * 10 + *pArg++ - '0';
			}
			if(i > 0) {
				uchar line[30];

				snprintf((char *) line, sizeof(line), "InputTCPMaxSessions %d", i);
				legacyOptsEnq(line);
			} else {
				if(ch == 't') {
					fprintf(stderr,	"rsyslogd: TCP session max configured "
						"to %d [-t %s] - changing to 1.\n", i, arg);
					legacyOptsEnq((uchar *) "InputTCPMaxSessions 1");
				} else if (ch == 'g') {
					fprintf(stderr,	"rsyslogd: GSS session max configured "
						"to %d [-g %s] - changing to 1.\n", i, arg);
					legacyOptsEnq((uchar *) "InputTCPMaxSessions 1");
				}
			}
		}
	} else
		fprintf(stderr, "rsyslogd: Invalid -t %s command line option.\n", arg);
}


/* doDie() is a signal handler. If called, it sets the bFinished variable
 * to indicate the program should terminate. However, it does not terminate
 * it itself, because that causes issues with multi-threading. The actual
 * termination is then done on the main thread. This solution might introduce
 * a minimal delay, but it is much cleaner than the approach of doing everything
 * inside the signal handler.
 * rgerhards, 2005-10-26
 * Note: we do not call DBGPRINTF() as this may cause us to block in case something
 * with the threading is wrong.
 */
static void doDie(int sig)
{
#	define MSG1 "DoDie called.\n"
#	define MSG2 "DoDie called 5 times - unconditional exit\n"
	static int iRetries = 0; /* debug aid */
	dbgprintf(MSG1);
	if(Debug)
		write(1, MSG1, sizeof(MSG1) - 1);
	if(iRetries++ == 4) {
		if(Debug)
			write(1, MSG2, sizeof(MSG2) - 1);
		abort();
	}
	bFinished = sig;
#	undef MSG1
#	undef MSG2
}


/* This function frees all dynamically allocated memory for program termination.
 * It must be called only immediately before exit(). It is primarily an aid
 * for memory debuggers, which prevents cluttered outupt.
 * rgerhards, 2008-03-20
 */
static void
freeAllDynMemForTermination(void)
{
	free(pszMainMsgQFName);
	free(pModDir);
	free(pszConfDAGFile);
}


/* Finalize and destruct all actions.
 */
static inline void
destructAllActions(void)
{
	ruleset.DestructAllActions();
	bHaveMainQueue = 0; // flag that internal messages need to be temporarily stored
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

	DBGPRINTF("exiting on signal %d\n", sig);

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
	DBGPRINTF("Terminating input threads...\n");
	glbl.SetGlobalInputTermination();
	thrdTerminateAll();

	/* and THEN send the termination log message (see long comment above) */
	if(sig && bLogStatusMsgs) {
		(void) snprintf(buf, sizeof(buf) / sizeof(char),
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
		 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"]" " exiting on signal %d.",
		 (int) myPid, sig);
		errno = 0;
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)buf, 0);
	}
	
	/* drain queue (if configured so) and stop main queue worker thread pool */
	DBGPRINTF("Terminating main queue...\n");
	qqueueDestruct(&pMsgQueue);
	pMsgQueue = NULL;

	/* Free ressources and close connections. This includes flushing any remaining
	 * repeated msgs.
	 */
	DBGPRINTF("Terminating outputs...\n");
	destructAllActions();

	DBGPRINTF("all primary multi-thread sources have been terminated - now doing aux cleanup...\n");
	/* rger 2005-02-22
	 * now clean up the in-memory structures. OK, the OS
	 * would also take care of that, but if we do it
	 * ourselfs, this makes finding memory leaks a lot
	 * easier.
	 */
	tplDeleteAll();

	remove_pid(PidFile);

	/* de-init some modules */
	modExitIminternal();

	/*dbgPrintAllDebugInfo(); / * this is the last spot where this can be done - below output modules are unloaded! */

	/* the following line cleans up CfSysLineHandlers that were not based on loadable
	 * modules. As such, they are not yet cleared.
	 */
	unregCfSysLineHdlrs();

	legacyOptsFree();

	/* destruct our global properties */
	if(pInternalInputName != NULL)
		prop.Destruct(&pInternalInputName);
	if(pLocalHostIP != NULL)
		prop.Destruct(&pLocalHostIP);

	/* terminate the remaining classes */
	GlobalClassExit();

	module.UnloadAndDestructAll(eMOD_LINK_ALL);

	DBGPRINTF("Clean shutdown completed, bye\n");
	/* dbgClassExit MUST be the last one, because it de-inits the debug system */
	dbgClassExit();

	/* free all remaining memory blocks - this is not absolutely necessary, but helps
	 * us keep memory debugger logs clean and this is in aid in developing. It doesn't
	 * cost much time, so we do it always. -- rgerhards, 2008-03-20
	 */
	freeAllDynMemForTermination();
	/* NO CODE HERE - feeelAllDynMemForTermination() must be the last thing before exit()! */
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


/* set the maximum message size */
static rsRetVal setMaxMsgSize(void __attribute__((unused)) *pVal, int iNewVal)
{
	return glbl.SetMaxLine(iNewVal);
}


/* set the action resume interval */
static rsRetVal setActionResumeInterval(void __attribute__((unused)) *pVal, int iNewVal)
{
	return actionSetGlobalResumeInterval(iNewVal);
}


/* set the processes max number ob files (upon configuration request)
 * 2009-04-14 rgerhards
 */
static rsRetVal setMaxFiles(void __attribute__((unused)) *pVal, int iFiles)
{
	struct rlimit maxFiles;
	char errStr[1024];
	DEFiRet;

	maxFiles.rlim_cur = iFiles;
	maxFiles.rlim_max = iFiles;

	if(setrlimit(RLIMIT_NOFILE, &maxFiles) < 0) {
		/* NOTE: under valgrind, we seem to be unable to extend the size! */
		rs_strerror_r(errno, errStr, sizeof(errStr));
		errmsg.LogError(0, RS_RET_ERR_RLIM_NOFILE, "could not set process file limit to %d: %s [kernel max %ld]",
				iFiles, errStr, (long) maxFiles.rlim_max);
		ABORT_FINALIZE(RS_RET_ERR_RLIM_NOFILE);
	}
	DBGPRINTF("Max number of files set to %d [kernel max %ld].\n", iFiles, (long) maxFiles.rlim_max);

finalize_it:
	RETiRet;
}


/* set the processes umask (upon configuration request) */
static rsRetVal setUmask(void __attribute__((unused)) *pVal, int iUmask)
{
	umask(iUmask);
	DBGPRINTF("umask set to 0%3.3o.\n", iUmask);

	return RS_RET_OK;
}


/* drop to specified group 
 * if something goes wrong, the function never returns
 * Note that such an abort can cause damage to on-disk structures, so we should
 * re-design the "interface" in the long term. -- rgerhards, 2008-11-26
 */
static void doDropPrivGid(int iGid)
{
	int res;
	uchar szBuf[1024];

	res = setgroups(0, NULL); /* remove all supplementary group IDs */
	if(res) {
		perror("could not remove supplemental group IDs");
		exit(1);
	}
	DBGPRINTF("setgroups(0, NULL): %d\n", res);
	res = setgid(iGid);
	if(res) {
		/* if we can not set the userid, this is fatal, so let's unconditionally abort */
		perror("could not set requested group id");
		exit(1);
	}
	DBGPRINTF("setgid(%d): %d\n", iGid, res);
	snprintf((char*)szBuf, sizeof(szBuf)/sizeof(uchar), "rsyslogd's groupid changed to %d", iGid);
	logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, szBuf, 0);
}


/* drop to specified user 
 * if something goes wrong, the function never returns
 * Note that such an abort can cause damage to on-disk structures, so we should
 * re-design the "interface" in the long term. -- rgerhards, 2008-11-19
 */
static void doDropPrivUid(int iUid)
{
	int res;
	uchar szBuf[1024];

	res = setuid(iUid);
	if(res) {
		/* if we can not set the userid, this is fatal, so let's unconditionally abort */
		perror("could not set requested userid");
		exit(1);
	}
	DBGPRINTF("setuid(%d): %d\n", iUid, res);
	snprintf((char*)szBuf, sizeof(szBuf)/sizeof(uchar), "rsyslogd's userid changed to %d", iUid);
	logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, szBuf, 0);
}


/* helper to generateConfigDAG, to print out all actions via
 * the llExecFunc() facility.
 * rgerhards, 2007-08-02
 */
struct dag_info {
	FILE *fp;	/* output file */
	int iActUnit;	/* current action unit number */
	int iAct;	/* current action in unit */
	int bDiscarded;	/* message discarded (config error) */
	};
DEFFUNC_llExecFunc(generateConfigDAGAction)
{
	action_t *pAction;
	uchar *pszModName;
	uchar *pszVertexName;
	struct dag_info *pDagInfo;
	DEFiRet;

	pDagInfo = (struct dag_info*) pParam;
	pAction = (action_t*) pData;

	pszModName = module.GetStateName(pAction->pMod);

	/* vertex */
	if(pAction->pszName == NULL) {
		if(!strcmp((char*)pszModName, "builtin-discard"))
			pszVertexName = (uchar*)"discard";
		else
			pszVertexName = pszModName;
	} else {
		pszVertexName = pAction->pszName;
	}

	fprintf(pDagInfo->fp, "\tact%d_%d\t\t[label=\"%s\"%s%s]\n",
		pDagInfo->iActUnit, pDagInfo->iAct, pszVertexName,
		pDagInfo->bDiscarded ? " style=dotted color=red" : "",
		(pAction->pQueue->qType == QUEUETYPE_DIRECT) ? "" : " shape=hexagon"
		);

	/* edge */
	if(pDagInfo->iAct == 0) {
	} else {
		fprintf(pDagInfo->fp, "\tact%d_%d -> act%d_%d[%s%s]\n",
			pDagInfo->iActUnit, pDagInfo->iAct - 1,
			pDagInfo->iActUnit, pDagInfo->iAct,
			pDagInfo->bDiscarded ? " style=dotted color=red" : "",
			pAction->bExecWhenPrevSusp ? " label=\"only if\\nsuspended\"" : "" );
	}

	/* check for discard */
	if(!strcmp((char*) pszModName, "builtin-discard")) {
		fprintf(pDagInfo->fp, "\tact%d_%d\t\t[shape=box]\n",
			pDagInfo->iActUnit, pDagInfo->iAct);
		pDagInfo->bDiscarded = 1;
	}


	++pDagInfo->iAct;

	RETiRet;
}


/* create config DAG
 * This functions takes a rsyslog config and produces a .dot file for use
 * with graphviz (http://www.graphviz.org). This is done in an effort to
 * document, and also potentially troubleshoot, configurations. Plus, I
 * consider it a nice feature to explain some concepts. Note that the
 * current version only produces a graph with relatively little information.
 * This is a foundation that may be later expanded (if it turns out to be
 * useful enough).
 * rgerhards, 2009-05-11
 */
static rsRetVal
generateConfigDAG(uchar *pszDAGFile)
{
	//rule_t *f;
	FILE *fp;
	int iActUnit = 1;
	//int bHasFilter = 0;	/* filter associated with this action unit? */
	//int bHadFilter;
	//int i;
	struct dag_info dagInfo;
	//char *pszFilterName;
	char szConnectingNode[64];
	DEFiRet;

	assert(pszDAGFile != NULL);
	
	if((fp = fopen((char*) pszDAGFile, "w")) == NULL) {
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)
			"configuraton graph output file could not be opened, none generated", 0);
		ABORT_FINALIZE(RS_RET_FILENAME_INVALID);
	}

	dagInfo.fp = fp;

	/* from here on, we assume writes go well. This here is a really
	 * unimportant utility function and if something goes wrong, it has
	 * almost no effect. So let's not overdo this...
	 */
	fprintf(fp, "# graph created by rsyslog " VERSION "\n\n"
	 	    "# use the dot tool from http://www.graphviz.org to visualize!\n"
		    "digraph rsyslogConfig {\n"
		    "\tinputs [shape=tripleoctagon]\n"
		    "\tinputs -> act0_0\n"
		    "\tact0_0 [label=\"main\\nqueue\" shape=hexagon]\n"
		    /*"\tmainq -> act1_0\n"*/
		    );
	strcpy(szConnectingNode, "act0_0");
	dagInfo.bDiscarded = 0;

/* TODO: re-enable! */
#if 0
	for(f = Files; f != NULL ; f = f->f_next) {
		/* BSD-Style filters are currently ignored */
		bHadFilter = bHasFilter;
		if(f->f_filter_type == FILTER_PRI) {
			bHasFilter = 0;
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_filterData.f_pmask[i] != 0xff) {
					bHasFilter = 1;
					break;
				}
		} else {
			bHasFilter = 1;
		}

		/* we know we have a filter, so it can be false */
		switch(f->f_filter_type) {
			case FILTER_PRI:
				pszFilterName = "pri filter";
				break;
			case FILTER_PROP:
				pszFilterName = "property filter";
				break;
			case FILTER_EXPR:
				pszFilterName = "script filter";
				break;
		}

		/* write action unit node */
		if(bHasFilter) {
			fprintf(fp, "\t%s -> act%d_end\t[label=\"%s:\\nfalse\"]\n",
				szConnectingNode, iActUnit, pszFilterName);
			fprintf(fp, "\t%s -> act%d_0\t[label=\"%s:\\ntrue\"]\n",
				szConnectingNode, iActUnit, pszFilterName);
			fprintf(fp, "\tact%d_end\t\t\t\t[shape=point]\n", iActUnit);
			snprintf(szConnectingNode, sizeof(szConnectingNode), "act%d_end", iActUnit);
		} else {
			fprintf(fp, "\t%s -> act%d_0\t[label=\"no filter\"]\n",
				szConnectingNode, iActUnit);
			snprintf(szConnectingNode, sizeof(szConnectingNode), "act%d_0", iActUnit);
		}

		/* draw individual nodes */
		dagInfo.iActUnit = iActUnit;
		dagInfo.iAct = 0;
		dagInfo.bDiscarded = 0;
		llExecFunc(&f->llActList, generateConfigDAGAction, &dagInfo); /* actions */

		/* finish up */
		if(bHasFilter && !dagInfo.bDiscarded) {
			fprintf(fp, "\tact%d_%d -> %s\n",
				iActUnit, dagInfo.iAct - 1, szConnectingNode);
		}

		++iActUnit;
	}
#endif

	fprintf(fp, "\t%s -> act%d_0\n", szConnectingNode, iActUnit);
	fprintf(fp, "\tact%d_0\t\t[label=discard shape=box]\n"
		    "}\n", iActUnit);
	fclose(fp);

finalize_it:
	RETiRet;
}


/* print debug information as part of init(). This pretty much
 * outputs the whole config of rsyslogd. I've moved this code
 * out of init() to clean it somewhat up.
 * rgerhards, 2007-07-31
 */
static void dbgPrintInitInfo(void)
{
	ruleset.DebugPrintAll();
	DBGPRINTF("\n");
	if(bDebugPrintTemplateList)
		tplPrintList();
	if(bDebugPrintModuleList)
		module.PrintList();
	ochPrintList();

	if(bDebugPrintCfSysLineHandlerList)
		dbgPrintCfSysLineHandlers();

	DBGPRINTF("Messages with malicious PTR DNS Records are %sdropped.\n",
		  glbl.GetDropMalPTRMsgs() ? "" : "not ");

	DBGPRINTF("Control characters are %sreplaced upon reception.\n",
		  bEscapeCCOnRcv? "" : "not ");

	if(bEscapeCCOnRcv)
		DBGPRINTF("Control character escape sequence prefix is '%c'.\n",
			cCCEscapeChar);

	DBGPRINTF("Main queue size %d messages.\n", iMainMsgQueueSize);
	DBGPRINTF("Main queue worker threads: %d, wThread shutdown: %d, Perists every %d updates.\n",
		  iMainMsgQueueNumWorkers, iMainMsgQtoWrkShutdown, iMainMsgQPersistUpdCnt);
	DBGPRINTF("Main queue timeouts: shutdown: %d, action completion shutdown: %d, enq: %d\n",
		   iMainMsgQtoQShutdown, iMainMsgQtoActShutdown, iMainMsgQtoEnq);
	DBGPRINTF("Main queue watermarks: high: %d, low: %d, discard: %d, discard-severity: %d\n",
		   iMainMsgQHighWtrMark, iMainMsgQLowWtrMark, iMainMsgQDiscardMark, iMainMsgQDiscardSeverity);
	DBGPRINTF("Main queue save on shutdown %d, max disk space allowed %lld\n",
		   bMainMsgQSaveOnShutdown, iMainMsgQueMaxDiskSpace);
	/* TODO: add
	iActionRetryCount = 0;
	iActionRetryInterval = 30000;
	static int iMainMsgQtoWrkMinMsgs = 100;	
	static int iMainMsgQbSaveOnShutdown = 1;
	iMainMsgQueMaxDiskSpace = 0;
	setQPROP(qqueueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", 100);
	setQPROP(qqueueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", 1);
	 */
	DBGPRINTF("Work Directory: '%s'.\n", glbl.GetWorkDir());
}


/* Actually run the input modules.  This happens after privileges are dropped,
 * if that is requested.
 */
static rsRetVal
runInputModules(void)
{
	modInfo_t *pMod;
	int bNeedsCancel;

	BEGINfunc
	/* loop through all modules and activate them (brr...) */
	pMod = module.GetNxtType(NULL, eMOD_IN);
	while(pMod != NULL) {
		if(pMod->mod.im.bCanRun) {
			/* activate here */
			bNeedsCancel = (pMod->isCompatibleWithFeature(sFEATURENonCancelInputTermination) == RS_RET_OK) ?
				       0 : 1;
			thrdCreate(pMod->mod.im.runInput, pMod->mod.im.afterRun, bNeedsCancel);
		}
	pMod = module.GetNxtType(pMod, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
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
	pMod = module.GetNxtType(NULL, eMOD_IN);
	while(pMod != NULL) {
		iRet = pMod->mod.im.willRun();
		pMod->mod.im.bCanRun = (iRet == RS_RET_OK);
		if(!pMod->mod.im.bCanRun) {
			DBGPRINTF("module %lx will not run, iRet %d\n", (unsigned long) pMod, iRet);
		}
	pMod = module.GetNxtType(pMod, eMOD_IN);
	}

	ENDfunc
	return RS_RET_OK; /* intentional: we do not care about module errors */
}


/* INIT -- Initialize syslogd
 * Note that if iConfigVerify is set, only the config file is verified but nothing
 * else happens. -- rgerhards, 2008-07-28
 */
static rsRetVal
init(void)
{
	rsRetVal localRet;
	int iNbrActions;
	int bHadConfigErr = 0;
	ruleset_t *pRuleset;
	char cbuf[BUFSIZ];
	char bufStartUpMsg[512];
	struct sigaction sigAct;
	DEFiRet;

	/* initialize some static variables */
	pDfltHostnameCmp = NULL;
	pDfltProgNameCmp = NULL;
	eDfltHostnameCmpMode = HN_NO_COMP;

	DBGPRINTF("rsyslog %s - called init()\n", VERSION);

	/* construct the default ruleset */
	ruleset.Construct(&pRuleset);
	ruleset.SetName(pRuleset, UCHAR_CONSTANT("RSYSLOG_DefaultRuleset"));
	ruleset.ConstructFinalize(pRuleset);

	/* open the configuration file */
	localRet = conf.processConfFile(ConfFile);
	CHKiRet(conf.GetNbrActActions(&iNbrActions));

	if(localRet != RS_RET_OK) {
		errmsg.LogError(0, localRet, "CONFIG ERROR: could not interpret master config file '%s'.", ConfFile);
		bHadConfigErr = 1;
	} else if(iNbrActions == 0) {
		errmsg.LogError(0, RS_RET_NO_ACTIONS, "CONFIG ERROR: there are no active actions configured. Inputs will "
			 "run, but no output whatsoever is created.");
		bHadConfigErr = 1;
	}

	if((localRet != RS_RET_OK && localRet != RS_RET_NONFATAL_CONFIG_ERR) || iNbrActions == 0) {
		/* rgerhards: this code is executed to set defaults when the
		 * config file could not be opened. We might think about
		 * abandoning the run in this case - but this, too, is not
		 * very clever... So we stick with what we have.
		 * We ignore any errors while doing this - we would be lost anyhow...
		 */
		errmsg.LogError(0, NO_ERRCODE, "EMERGENCY CONFIGURATION ACTIVATED - fix rsyslog config file!");

		/* note: we previously used _POSIY_TTY_NAME_MAX+1, but this turned out to be
		 * too low on linux... :-S   -- rgerhards, 2008-07-28
		 */
		char szTTYNameBuf[128];
		rule_t *pRule = NULL; /* initialization to NULL is *vitally* important! */
		conf.cfline(UCHAR_CONSTANT("*.ERR\t" _PATH_CONSOLE), &pRule);
		conf.cfline(UCHAR_CONSTANT("syslog.*\t" _PATH_CONSOLE), &pRule);
		conf.cfline(UCHAR_CONSTANT("*.PANIC\t*"), &pRule);
		conf.cfline(UCHAR_CONSTANT("syslog.*\troot"), &pRule);
		if(ttyname_r(0, szTTYNameBuf, sizeof(szTTYNameBuf)) == 0) {
			snprintf(cbuf,sizeof(cbuf), "*.*\t%s", szTTYNameBuf);
			conf.cfline((uchar*)cbuf, &pRule);
		} else {
			DBGPRINTF("error %d obtaining controlling terminal, not using that emergency rule\n", errno);
		}
		ruleset.AddRule(ruleset.GetCurrent(), &pRule);
	}

	legacyOptsHook();

	/* we are now done with reading the configuration. This is the right time to
	 * free some objects that were just needed for loading it. rgerhards 2005-10-19
	 */
	if(pDfltHostnameCmp != NULL) {
		rsCStrDestruct(&pDfltHostnameCmp);
	}

	if(pDfltProgNameCmp != NULL) {
		rsCStrDestruct(&pDfltProgNameCmp);
	}

	/* some checks */
	if(iMainMsgQueueNumWorkers < 1) {
		errmsg.LogError(0, NO_ERRCODE, "$MainMsgQueueNumWorkers must be at least 1! Set to 1.\n");
		iMainMsgQueueNumWorkers = 1;
	}

	if(MainMsgQueType == QUEUETYPE_DISK) {
		errno = 0;	/* for logerror! */
		if(glbl.GetWorkDir() == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "No $WorkDirectory specified - can not run main message queue in 'disk' mode. "
				 "Using 'FixedArray' instead.\n");
			MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
		if(pszMainMsgQFName == NULL) {
			errmsg.LogError(0, NO_ERRCODE, "No $MainMsgQueueFileName specified - can not run main message queue in "
				 "'disk' mode. Using 'FixedArray' instead.\n");
			MainMsgQueType = QUEUETYPE_FIXED_ARRAY;
		}
	}

	/* check if we need to generate a config DAG and, if so, do that */
	if(pszConfDAGFile != NULL)
		generateConfigDAG(pszConfDAGFile);

	/* we are done checking the config - now validate if we should actually run or not.
	 * If not, terminate. -- rgerhards, 2008-07-25
	 */
	if(iConfigVerify) {
		if(bHadConfigErr) {
			/* a bit dirty, but useful... */
			exit(1);
		}
		ABORT_FINALIZE(RS_RET_VALIDATION_RUN);
	}

	if(bAbortOnUncleanConfig && bHadConfigErr) {
		fprintf(stderr, "rsyslogd: $AbortOnUncleanConfig is set, and config is not clean.\n"
		                "Check error log for details, fix errors and restart. As a last\n"
				"resort, you may want to remove $AbortOnUncleanConfig to permit a\n"
				"startup with a dirty config.\n");
		exit(2);
	}

	/* switch the message object to threaded operation, if necessary */
	if(MainMsgQueType == QUEUETYPE_DIRECT || iMainMsgQueueNumWorkers > 1) {
		MsgEnableThreadSafety();
	}

	/* create message queue */
	CHKiRet_Hdlr(qqueueConstruct(&pMsgQueue, MainMsgQueType, iMainMsgQueueNumWorkers, iMainMsgQueueSize, msgConsumer)) {
		/* no queue is fatal, we need to give up in that case... */
		fprintf(stderr, "fatal error %d: could not create message queue - rsyslogd can not run!\n", iRet);
		exit(1);
	}
	/* name our main queue object (it's not fatal if it fails...) */
	obj.SetName((obj_t*) pMsgQueue, (uchar*) "main Q");

	/* ... set some properties ... */
#	define setQPROP(func, directive, data) \
	CHKiRet_Hdlr(func(pMsgQueue, data)) { \
		errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}
#	define setQPROPstr(func, directive, data) \
	CHKiRet_Hdlr(func(pMsgQueue, data, (data == NULL)? 0 : strlen((char*) data))) { \
		errmsg.LogError(0, NO_ERRCODE, "Invalid " #directive ", error %d. Ignored, running with default setting", iRet); \
	}

	setQPROP(qqueueSetMaxFileSize, "$MainMsgQueueFileSize", iMainMsgQueMaxFileSize);
	setQPROP(qqueueSetsizeOnDiskMax, "$MainMsgQueueMaxDiskSpace", iMainMsgQueMaxDiskSpace);
	setQPROP(qqueueSetiDeqBatchSize, "$MainMsgQueueDequeueBatchSize", iMainMsgQueDeqBatchSize);
	setQPROPstr(qqueueSetFilePrefix, "$MainMsgQueueFileName", pszMainMsgQFName);
	setQPROP(qqueueSetiPersistUpdCnt, "$MainMsgQueueCheckpointInterval", iMainMsgQPersistUpdCnt);
	setQPROP(qqueueSetbSyncQueueFiles, "$MainMsgQueueSyncQueueFiles", bMainMsgQSyncQeueFiles);
	setQPROP(qqueueSettoQShutdown, "$MainMsgQueueTimeoutShutdown", iMainMsgQtoQShutdown );
	setQPROP(qqueueSettoActShutdown, "$MainMsgQueueTimeoutActionCompletion", iMainMsgQtoActShutdown);
	setQPROP(qqueueSettoWrkShutdown, "$MainMsgQueueWorkerTimeoutThreadShutdown", iMainMsgQtoWrkShutdown);
	setQPROP(qqueueSettoEnq, "$MainMsgQueueTimeoutEnqueue", iMainMsgQtoEnq);
	setQPROP(qqueueSetiHighWtrMrk, "$MainMsgQueueHighWaterMark", iMainMsgQHighWtrMark);
	setQPROP(qqueueSetiLowWtrMrk, "$MainMsgQueueLowWaterMark", iMainMsgQLowWtrMark);
	setQPROP(qqueueSetiDiscardMrk, "$MainMsgQueueDiscardMark", iMainMsgQDiscardMark);
	setQPROP(qqueueSetiDiscardSeverity, "$MainMsgQueueDiscardSeverity", iMainMsgQDiscardSeverity);
	setQPROP(qqueueSetiMinMsgsPerWrkr, "$MainMsgQueueWorkerThreadMinimumMessages", iMainMsgQWrkMinMsgs);
	setQPROP(qqueueSetbSaveOnShutdown, "$MainMsgQueueSaveOnShutdown", bMainMsgQSaveOnShutdown);
	setQPROP(qqueueSetiDeqSlowdown, "$MainMsgQueueDequeueSlowdown", iMainMsgQDeqSlowdown);
	setQPROP(qqueueSetiDeqtWinFromHr,  "$MainMsgQueueDequeueTimeBegin", iMainMsgQueueDeqtWinFromHr);
	setQPROP(qqueueSetiDeqtWinToHr,    "$MainMsgQueueDequeueTimeEnd", iMainMsgQueueDeqtWinToHr);

#	undef setQPROP
#	undef setQPROPstr

	/* ... and finally start the queue! */
	CHKiRet_Hdlr(qqueueStart(pMsgQueue)) {
		/* no queue is fatal, we need to give up in that case... */
		fprintf(stderr, "fatal error %d: could not start message queue - rsyslogd can not run!\n", iRet);
		exit(1);
	}

	bHaveMainQueue = (MainMsgQueType == QUEUETYPE_DIRECT) ? 0 : 1;
	DBGPRINTF("Main processing queue is initialized and running\n");

	/* the output part and the queue is now ready to run. So it is a good time
	 * to initialize the inputs. Please note that the net code above should be
	 * shuffled to down here once we have everything in input modules.
	 * rgerhards, 2007-12-14
	 * NOTE: as of 2009-06-29, the input modules are initialized, but not yet run.
	 * Keep in mind. though, that the outputs already run if the queue was
	 * persisted to disk. -- rgerhards
	 */
	startInputModules();

	if(Debug) {
		dbgPrintInitInfo();
	}

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);

	DBGPRINTF(" started.\n");

	/* we now generate the startup message. It now includes everything to
	 * identify this instance. -- rgerhards, 2005-08-17
	 */
	if(bLogStatusMsgs) {
		snprintf(bufStartUpMsg, sizeof(bufStartUpMsg)/sizeof(char), 
			 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION \
			 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"] start",
			 (int) myPid);
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)bufStartUpMsg, 0);
	}

finalize_it:
	RETiRet;
}


/* Switch the default ruleset (that, what servcies bind to if nothing specific
 * is specified).
 * rgerhards, 2009-06-12
 */
static rsRetVal
setDefaultRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	DEFiRet;

	CHKiRet(ruleset.SetDefaultRuleset(pszName));

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}


/* Switch to either an already existing rule set or start a new one. The
 * named rule set becomes the new "current" rule set (what means that new
 * actions are added to it).
 * rgerhards, 2009-06-12
 */
static rsRetVal
setCurrRuleset(void __attribute__((unused)) *pVal, uchar *pszName)
{
	ruleset_t *pRuleset;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.SetCurrRuleset(pszName);

	if(localRet == RS_RET_NOT_FOUND) {
		DBGPRINTF("begin new current rule set '%s'\n", pszName);
		CHKiRet(ruleset.Construct(&pRuleset));
		CHKiRet(ruleset.SetName(pRuleset, pszName));
		CHKiRet(ruleset.ConstructFinalize(pRuleset));
	} else {
		ABORT_FINALIZE(localRet);
	}

finalize_it:
	free(pszName); /* no longer needed */
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
		DBGPRINTF("main message queue type set to FIXED_ARRAY\n");
	} else if (!strcasecmp((char *) pszType, "linkedlist")) {
		MainMsgQueType = QUEUETYPE_LINKEDLIST;
		DBGPRINTF("main message queue type set to LINKEDLIST\n");
	} else if (!strcasecmp((char *) pszType, "disk")) {
		MainMsgQueType = QUEUETYPE_DISK;
		DBGPRINTF("main message queue type set to DISK\n");
	} else if (!strcasecmp((char *) pszType, "direct")) {
		MainMsgQueType = QUEUETYPE_DIRECT;
		DBGPRINTF("main message queue type set to DIRECT (no queueing at all)\n");
	} else {
		errmsg.LogError(0, RS_RET_INVALID_PARAMS, "unknown mainmessagequeuetype parameter: %s", (char *) pszType);
		iRet = RS_RET_INVALID_PARAMS;
	}
	free(pszType); /* no longer needed */

	RETiRet;
}


/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tells the main loop to do "the right thing".
 */
void sighup_handler()
{
	struct sigaction sigAct;
	
	bHadHUP = 1;

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);
	sigAct.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sigAct, NULL);
}

void sigttin_handler()
{
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
		logmsg(pMsg, iFlags);
	}
}


/* helper to doHUP(), this "HUPs" each action. The necessary locking
 * is done inside the action class and nothing we need to take care of.
 * rgerhards, 2008-10-22
 */
DEFFUNC_llExecFunc(doHUPActions)
{
	BEGINfunc
	actionCallHUPHdlr((action_t*) pData);
	ENDfunc
	return RS_RET_OK; /* we ignore errors, we can not do anything either way */
}


/* This function processes a HUP after one has been detected. Note that this
 * is *NOT* the sighup handler. The signal is recorded by the handler, that record
 * detected inside the mainloop and then this function is called to do the
 * real work. -- rgerhards, 2008-10-22
 */
static inline void
doHUP(void)
{
	char buf[512];

	if(bLogStatusMsgs) {
		snprintf(buf, sizeof(buf) / sizeof(char),
			 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION
			 "\" x-pid=\"%d\" x-info=\"http://www.rsyslog.com\"] rsyslogd was HUPed",
			 (int) myPid);
			errno = 0;
		logmsgInternal(NO_ERRCODE, LOG_SYSLOG|LOG_INFO, (uchar*)buf, 0);
	}

	ruleset.IterateAllActions(doHUPActions, NULL);
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
	/* first check if we have any internal messages queued and spit them out. We used
	 * to do that on any loop iteration, but that is no longer necessry. The reason
	 * is that once we reach this point here, we always run on multiple threads and
	 * thus the main queue is properly initialized. -- rgerhards, 2008-06-09
	 */
	processImInternal();

	while(!bFinished){
		/* this is now just a wait - please note that we do use a near-"eternal"
		 * timeout of 1 day if we do not have repeated message reduction turned on
		 * (which it is not by default). This enables us to help safe the environment
		 * by not unnecessarily awaking rsyslog on a regular tick (just think
		 * powertop, for example). In that case, we primarily wait for a signal,
		 * but a once-a-day wakeup should be quite acceptable. -- rgerhards, 2008-06-09
		 */
		tvSelectTimeout.tv_sec = (bReduceRepeatMsgs == 1) ? TIMERINTVL : 86400 /*1 day*/;
		//tvSelectTimeout.tv_sec = TIMERINTVL; /* TODO: change this back to the above code when we have a better solution for apc */
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
		if(bReduceRepeatMsgs == 1)
			doFlushRptdMsgs();

		if(bHadHUP) {
			doHUP();
			bHadHUP = 0;
			continue;
		}
		// TODO: remove execScheduled(); /* handle Apc calls (if any) */
	}
	ENDfunc
}


/* load build-in modules
 * very first version begun on 2007-07-23 by rgerhards
 */
static rsRetVal loadBuildInModules(void)
{
	DEFiRet;

	if((iRet = module.doModInit(modInitFile, UCHAR_CONSTANT("builtin-file"), NULL)) != RS_RET_OK) {
		RETiRet;
	}
#ifdef SYSLOG_INET
	if((iRet = module.doModInit(modInitFwd, UCHAR_CONSTANT("builtin-fwd"), NULL)) != RS_RET_OK) {
		RETiRet;
	}
#endif
	if((iRet = module.doModInit(modInitShell, UCHAR_CONSTANT("builtin-shell"), NULL)) != RS_RET_OK) {
		RETiRet;
	}
	if((iRet = module.doModInit(modInitDiscard, UCHAR_CONSTANT("builtin-discard"), NULL)) != RS_RET_OK) {
		RETiRet;
	}

	/* dirty, but this must be for the time being: the usrmsg module must always be
	 * loaded as last module. This is because it processes any type of action selector.
	 * If we load it before other modules, these others will never have a chance of
	 * working with the config file. We may change that implementation so that a user name
	 * must start with an alnum, that would definitely help (but would it break backwards
	 * compatibility?). * rgerhards, 2007-07-23
	 * User names now must begin with:
	 *   [a-zA-Z0-9_.]
	 */
	CHKiRet(module.doModInit(modInitUsrMsg, (uchar*) "builtin-usrmsg", NULL));

	/* ok, initialization of the command handler probably does not 100% belong right in
	 * this space here. However, with the current design, this is actually quite a good
	 * place to put it. We might decide to shuffle it around later, but for the time
	 * being, the code has found its home here. A not-just-sideeffect of this decision
	 * is that rsyslog will terminate if we can not register our built-in config commands.
	 * This, I think, is the right thing to do. -- rgerhards, 2007-07-31
	 */
	CHKiRet(regCfSysLineHdlr((uchar *)"logrsyslogstatusmessages", 0, eCmdHdlrBinary, NULL, &bLogStatusMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeretrycount", 0, eCmdHdlrInt, NULL, &glbliActionResumeRetryCount, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"defaultruleset", 0, eCmdHdlrGetWord, setDefaultRuleset, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"ruleset", 0, eCmdHdlrGetWord, setCurrRuleset, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuefilename", 0, eCmdHdlrGetWord, NULL, &pszMainMsgQFName, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesize", 0, eCmdHdlrInt, NULL, &iMainMsgQueueSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuehighwatermark", 0, eCmdHdlrInt, NULL, &iMainMsgQHighWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuelowwatermark", 0, eCmdHdlrInt, NULL, &iMainMsgQLowWtrMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardmark", 0, eCmdHdlrInt, NULL, &iMainMsgQDiscardMark, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuediscardseverity", 0, eCmdHdlrSeverity, NULL, &iMainMsgQDiscardSeverity, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuecheckpointinterval", 0, eCmdHdlrInt, NULL, &iMainMsgQPersistUpdCnt, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesyncqueuefiles", 0, eCmdHdlrBinary, NULL, &bMainMsgQSyncQeueFiles, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetype", 0, eCmdHdlrGetWord, setMainMsgQueType, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreads", 0, eCmdHdlrInt, NULL, &iMainMsgQueueNumWorkers, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutshutdown", 0, eCmdHdlrInt, NULL, &iMainMsgQtoQShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutactioncompletion", 0, eCmdHdlrInt, NULL, &iMainMsgQtoActShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuetimeoutenqueue", 0, eCmdHdlrInt, NULL, &iMainMsgQtoEnq, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkertimeoutthreadshutdown", 0, eCmdHdlrInt, NULL, &iMainMsgQtoWrkShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeueslowdown", 0, eCmdHdlrInt, NULL, &iMainMsgQDeqSlowdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueueworkerthreadminimummessages", 0, eCmdHdlrInt, NULL, &iMainMsgQWrkMinMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuemaxfilesize", 0, eCmdHdlrSize, NULL, &iMainMsgQueMaxFileSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuebatchsize", 0, eCmdHdlrSize, NULL, &iMainMsgQueDeqBatchSize, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuemaxdiskspace", 0, eCmdHdlrSize, NULL, &iMainMsgQueMaxDiskSpace, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuesaveonshutdown", 0, eCmdHdlrBinary, NULL, &bMainMsgQSaveOnShutdown, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuetimebegin", 0, eCmdHdlrInt, NULL, &iMainMsgQueueDeqtWinFromHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"mainmsgqueuedequeuetimeend", 0, eCmdHdlrInt, NULL, &iMainMsgQueueDeqtWinToHr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"abortonuncleanconfig", 0, eCmdHdlrBinary, NULL, &bAbortOnUncleanConfig, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"repeatedmsgreduction", 0, eCmdHdlrBinary, NULL, &bReduceRepeatMsgs, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlywhenpreviousissuspended", 0, eCmdHdlrBinary, NULL, &bActExecWhenPrevSusp, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionexeconlyonceeveryinterval", 0, eCmdHdlrInt, NULL, &iActExecOnceInterval, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"actionresumeinterval", 0, eCmdHdlrInt, setActionResumeInterval, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"controlcharacterescapeprefix", 0, eCmdHdlrGetChar, NULL, &cCCEscapeChar, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"escapecontrolcharactersonreceive", 0, eCmdHdlrBinary, NULL, &bEscapeCCOnRcv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"droptrailinglfonreception", 0, eCmdHdlrBinary, NULL, &bDropTrailingLF, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"template", 0, eCmdHdlrCustomHandler, conf.doNameLine, (void*)DIR_TEMPLATE, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"outchannel", 0, eCmdHdlrCustomHandler, conf.doNameLine, (void*)DIR_OUTCHANNEL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"allowedsender", 0, eCmdHdlrCustomHandler, conf.doNameLine, (void*)DIR_ALLOWEDSENDER, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"modload", 0, eCmdHdlrCustomHandler, conf.doModLoad, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"includeconfig", 0, eCmdHdlrCustomHandler, conf.doIncludeLine, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"umask", 0, eCmdHdlrFileCreateMode, setUmask, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"maxopenfiles", 0, eCmdHdlrInt, setMaxFiles, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprinttemplatelist", 0, eCmdHdlrBinary, NULL, &bDebugPrintTemplateList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintmodulelist", 0, eCmdHdlrBinary, NULL, &bDebugPrintModuleList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"debugprintcfsyslinehandlerlist", 0, eCmdHdlrBinary,
		 NULL, &bDebugPrintCfSysLineHandlerList, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"moddir", 0, eCmdHdlrGetWord, NULL, &pModDir, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"generateconfiggraph", 0, eCmdHdlrGetWord, NULL, &pszConfDAGFile, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"errormessagestostderr", 0, eCmdHdlrBinary, NULL, &bErrMsgToStderr, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"maxmessagesize", 0, eCmdHdlrSize, setMaxMsgSize, NULL, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptouser", 0, eCmdHdlrUID, NULL, &uidDropPriv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptouserid", 0, eCmdHdlrInt, NULL, &uidDropPriv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptogroup", 0, eCmdHdlrGID, NULL, &gidDropPriv, NULL));
	CHKiRet(regCfSysLineHdlr((uchar *)"privdroptogroupid", 0, eCmdHdlrGID, NULL, &gidDropPriv, NULL));

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
#ifdef	HAVE_ATOMIC_BUILTINS
	printf("\tAtomic operations supported:\t\tYes\n");
#else
	printf("\tAtomic operations supported:\t\tNo\n");
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
static rsRetVal mainThread()
{
	DEFiRet;
	uchar *pTmp;

	/* Note: signals MUST be processed by the thread this code is running in. The reason
	 * is that we need to interrupt the select() system call. -- rgerhards, 2007-10-17
	 */

	/* initialize the build-in templates */
	pTmp = template_DebugFormat;
	tplAddLine("RSYSLOG_DebugFormat", &pTmp);
	pTmp = template_SyslogProtocol23Format;
	tplAddLine("RSYSLOG_SyslogProtocol23Format", &pTmp);
	pTmp = template_FileFormat; /* new format for files with high-precision stamp */
	tplAddLine("RSYSLOG_FileFormat", &pTmp);
	pTmp = template_TraditionalFileFormat;
	tplAddLine("RSYSLOG_TraditionalFileFormat", &pTmp);
	pTmp = template_WallFmt;
	tplAddLine(" WallFmt", &pTmp);
	pTmp = template_ForwardFormat;
	tplAddLine("RSYSLOG_ForwardFormat", &pTmp);
	pTmp = template_TraditionalForwardFormat;
	tplAddLine("RSYSLOG_TraditionalForwardFormat", &pTmp);
	pTmp = template_StdUsrMsgFmt;
	tplAddLine(" StdUsrMsgFmt", &pTmp);
	pTmp = template_StdDBFmt;
	tplAddLine(" StdDBFmt", &pTmp);
        pTmp = template_StdPgSQLFmt;
        tplLastStaticInit(tplAddLine(" StdPgSQLFmt", &pTmp));

	CHKiRet(init());

	if(Debug && debugging_on) {
		DBGPRINTF("Debugging enabled, SIGUSR1 to turn off debugging.\n");
	}

	/* Send a signal to the parent so it can terminate.
	 */
	if(myPid != ppid)
		kill(ppid, SIGTERM);


	/* If instructed to do so, we now drop privileges. Note that this is not 100% secure,
	 * because outputs are already running at this time. However, we can implement
	 * dropping of privileges rather quickly and it will work in many cases. While it is not
	 * the ultimate solution, the current one is still much better than not being able to
	 * drop privileges at all. Doing it correctly, requires a change in architecture, which
	 * we should do over time. TODO -- rgerhards, 2008-11-19
	 */
	if(gidDropPriv != 0) {
		doDropPrivGid(gidDropPriv);
	}

	if(uidDropPriv != 0) {
		doDropPrivUid(uidDropPriv);
	}

	/* finally let the inputs run... */
	runInputModules();

	/* END OF INTIALIZATION
	 */
	DBGPRINTF("initialization completed, transitioning to regular run mode\n");

	/* close stderr and stdout if they are kept open during a fork. Note that this
	 * may introduce subtle security issues: if we are in a jail, one may break out of
	 * it via these descriptors. But if I close them earlier, error messages will (once
	 * again) not be emitted to the user that starts the daemon. As root jail support
	 * is still in its infancy (and not really done), we currently accept this issue.
	 * rgerhards, 2009-06-29
	 */
	if(!(Debug || NoFork)) {
		close(1);
		close(2);
		bErrMsgToStderr = 0;
	}

	mainloop();

finalize_it:
	RETiRet;
}


/* Method to initialize all global classes and use the objects that we need.
 * rgerhards, 2008-01-04
 * rgerhards, 2008-04-16: the actual initialization is now carried out by the runtime
 */
static rsRetVal
InitGlobalClasses(void)
{
	DEFiRet;
	char *pErrObj; /* tells us which object failed if that happens (useful for troubleshooting!) */

	/* Intialize the runtime system */
	pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */
	CHKiRet(rsrtInit(&pErrObj, &obj));
	CHKiRet(rsrtSetErrLogger(submitErrMsg)); /* set out error handler */

	/* Now tell the system which classes we need ourselfs */
	pErrObj = "glbl";
	CHKiRet(objUse(glbl,     CORE_COMPONENT));
	pErrObj = "errmsg";
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	pErrObj = "module";
	CHKiRet(objUse(module,   CORE_COMPONENT));
	pErrObj = "datetime";
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	pErrObj = "expr";
	CHKiRet(objUse(expr,     CORE_COMPONENT));
	pErrObj = "rule";
	CHKiRet(objUse(rule,     CORE_COMPONENT));
	pErrObj = "ruleset";
	CHKiRet(objUse(ruleset,  CORE_COMPONENT));
	pErrObj = "conf";
	CHKiRet(objUse(conf,     CORE_COMPONENT));
	pErrObj = "prop";
	CHKiRet(objUse(prop,     CORE_COMPONENT));

	/* intialize some dummy classes that are not part of the runtime */
	pErrObj = "action";
	CHKiRet(actionClassInit());
	pErrObj = "template";
	CHKiRet(templateInit());
	pErrObj = "parser";
	CHKiRet(parserClassInit());

	/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	pErrObj = "net";
	CHKiRet(objUse(net, LM_NET_FILENAME));

finalize_it:
	if(iRet != RS_RET_OK) {
		/* we know we are inside the init sequence, so we can safely emit
		 * messages to stderr. -- rgerhards, 2008-04-02
		 */
		fprintf(stderr, "Error during class init for object '%s' - failing...\n", pErrObj);
	}

	RETiRet;
}


/* Method to exit all global classes. We do not do any error checking here,
 * because that wouldn't help us at all. So better try to deinit blindly
 * as much as succeeds (which usually means everything will). We just must
 * be careful to do the de-init in the opposite order of the init, because
 * of the dependencies. However, its not as important this time, because
 * we have reference counting.
 * rgerhards, 2008-03-10
 */
static rsRetVal
GlobalClassExit(void)
{
	DEFiRet;

	/* first, release everything we used ourself */
	objRelease(net,      LM_NET_FILENAME);/* TODO: the dependency on net shall go away! -- rgerhards, 2008-03-07 */
	objRelease(prop,     CORE_COMPONENT);
	objRelease(conf,     CORE_COMPONENT);
	objRelease(ruleset,  CORE_COMPONENT);
	objRelease(rule,     CORE_COMPONENT);
	objRelease(expr,     CORE_COMPONENT);
	vmClassExit();					/* this is hack, currently core_modules do not get this automatically called */
	objRelease(datetime, CORE_COMPONENT);

	/* TODO: implement the rest of the deinit */
	/* dummy "classes */
	strExit();

#if 0
	CHKiRet(objGetObjInterface(&obj)); /* this provides the root pointer for all other queries */
	/* the following classes were intialized by objClassInit() */
	CHKiRet(objUse(errmsg,   CORE_COMPONENT));
	CHKiRet(objUse(module,   CORE_COMPONENT));
#endif
	rsrtExit(); /* *THIS* *MUST/SHOULD?* always be the first class initilizer being called (except debug)! */

	RETiRet;
}


/* some support for command line option parsing. Any non-trivial options must be
 * buffered until the complete command line has been parsed. This is necessary to
 * prevent dependencies between the options. That, in turn, means we need to have
 * something that is capable of buffering options and there values. The follwing
 * functions handle that.
 * rgerhards, 2008-04-04
 */
typedef struct bufOpt {
	struct bufOpt *pNext;
	char optchar;
	char *arg;
} bufOpt_t;
static bufOpt_t *bufOptRoot = NULL;
static bufOpt_t *bufOptLast = NULL;

/* add option buffer */
static rsRetVal
bufOptAdd(char opt, char *arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if((pBuf = malloc(sizeof(bufOpt_t))) == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

	pBuf->optchar = opt;
	pBuf->arg = arg;
	pBuf->pNext = NULL;

	if(bufOptLast == NULL) {
		bufOptRoot = pBuf; /* then there is also no root! */
	} else {
		bufOptLast->pNext = pBuf;
	}
	bufOptLast = pBuf;

finalize_it:
	RETiRet;
}



/* remove option buffer from top of list, return values and destruct buffer itself.
 * returns RS_RET_END_OF_LINKEDLIST when no more options are present.
 * (we use int *opt instead of char *opt to keep consistent with getopt())
 */
static rsRetVal
bufOptRemove(int *opt, char **arg)
{
	DEFiRet;
	bufOpt_t *pBuf;

	if(bufOptRoot == NULL)
		ABORT_FINALIZE(RS_RET_END_OF_LINKEDLIST);
	pBuf = bufOptRoot;

	*opt = pBuf->optchar;
	*arg = pBuf->arg;

	bufOptRoot = pBuf->pNext;
	free(pBuf);

finalize_it:
	RETiRet;
}


/* global initialization, to be done only once and before the mainloop is started.
 * rgerhards, 2008-07-28 (extracted from realMain())
 */
static rsRetVal
doGlblProcessInit(void)
{
	struct sigaction sigAct;
	int num_fds;
	int i;
	DEFiRet;

	thrdInit();

	if( !(Debug || NoFork) )
	{
		DBGPRINTF("Checking pidfile.\n");
		if (!check_pid(PidFile))
		{
			memset(&sigAct, 0, sizeof (sigAct));
			sigemptyset(&sigAct.sa_mask);
			sigAct.sa_handler = doexit;
			sigaction(SIGTERM, &sigAct, NULL);

			if (fork()) {
				/* Parent process
				 */
				sleep(300);
				/* Not reached unless something major went wrong.  5
				 * minutes should be a fair amount of time to wait.
				 * Please note that this procedure is important since
				 * the father must not exit before syslogd isn't
				 * initialized or the klogd won't be able to flush its
				 * logs.  -Joey
				 */
				exit(1); /* "good" exit - after forking, not diasabling anything */
			}
			num_fds = getdtablesize();
			close(0);
			/* we keep stdout and stderr open in case we have to emit something */
			for (i = 3; i < num_fds; i++)
				(void) close(i);
			untty();
		}
		else
		{
			fputs(" Already running.\n", stderr);
			exit(1); /* "good" exit, done if syslogd is already running */
		}
	} else {
		debugging_on = 1;
	}

	/* tuck my process id away */
	DBGPRINTF("Writing pidfile %s.\n", PidFile);
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

	memset(&sigAct, 0, sizeof (sigAct));
	sigemptyset(&sigAct.sa_mask);

	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGSEGV, &sigAct, NULL);
	sigAct.sa_handler = sigsegvHdlr;
	sigaction(SIGABRT, &sigAct, NULL);
	sigAct.sa_handler = doDie;
	sigaction(SIGTERM, &sigAct, NULL);
	sigAct.sa_handler = Debug ? doDie : SIG_IGN;
	sigaction(SIGINT, &sigAct, NULL);
	sigaction(SIGQUIT, &sigAct, NULL);
	sigAct.sa_handler = reapchild;
	sigaction(SIGCHLD, &sigAct, NULL);
	sigAct.sa_handler = Debug ? debug_switch : SIG_IGN;
	sigaction(SIGUSR1, &sigAct, NULL);
	sigAct.sa_handler = sigttin_handler;
	sigaction(SIGTTIN, &sigAct, NULL); /* (ab)used to interrupt input threads */
	sigAct.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigAct, NULL);
	sigaction(SIGXFSZ, &sigAct, NULL); /* do not abort if 2gig file limit is hit */

	RETiRet;
}


/* This is the main entry point into rsyslogd. Over time, we should try to
 * modularize it a bit more...
 */
int realMain(int argc, char **argv)
{	
	DEFiRet;

	register uchar *p;
	int ch;
	struct hostent *hent;
	extern int optind;
	extern char *optarg;
	int bEOptionWasGiven = 0;
	int bImUxSockLoaded = 0; /* already generated a $ModLoad imuxsock? */
	int iHelperUOpt;
	int bChDirRoot = 1; /* change the current working directory to "/"? */
	char *arg;	/* for command line option processing */
	uchar legacyConfLine[80];
	uchar *LocalHostName;
	uchar *LocalDomain;
	uchar *LocalFQDNName;

	/* first, parse the command line options. We do not carry out any actual work, just
	 * see what we should do. This relieves us from certain anomalies and we can process
	 * the parameters down below in the correct order. For example, we must know the
	 * value of -M before we can do the init, but at the same time we need to have
	 * the base classes init before we can process most of the options. Now, with the
	 * split of functionality, this is no longer a problem. Thanks to varmofekoj for
	 * suggesting this algo.
	 * Note: where we just need to set some flags and can do so without knowledge
	 * of other options, we do this during the inital option processing. With later
	 * versions (if a dependency on -c option is introduced), we must move that code
	 * to other places, but I think it is quite appropriate and saves code to do this
	 * only when actually neeeded. 
	 * rgerhards, 2008-04-04
	 */
	while((ch = getopt(argc, argv, "46a:Ac:def:g:hi:l:m:M:nN:op:qQr::s:t:T:u:vwx")) != EOF) {
		switch((char)ch) {
                case '4':
                case '6':
                case 'A':
                case 'a':
		case 'f': /* configuration file */
		case 'h':
		case 'i': /* pid file name */
		case 'l':
		case 'm': /* mark interval */
		case 'n': /* don't fork */
		case 'N': /* enable config verify mode */
                case 'o':
                case 'p':
		case 'q': /* add hostname if DNS resolving has failed */
		case 'Q': /* dont resolve hostnames in ACL to IPs */
		case 's':
		case 'T': /* chroot on startup (primarily for testing) */
		case 'u': /* misc user settings */
		case 'w': /* disable disallowed host warnings */
		case 'x': /* disable dns for remote messages */
			CHKiRet(bufOptAdd(ch, optarg));
			break;
		case 'c':		/* compatibility mode */
			iCompatibilityMode = atoi(optarg);
			break;
		case 'd': /* debug - must be handled now, so that debug is active during init! */
			Debug = 1;
			break;
		case 'e':		/* log every message (no repeat message supression) */
			fprintf(stderr, "note: -e option is no longer supported, every message is now logged by default\n");
			bEOptionWasGiven = 1;
			break;
		case 'g':		/* enable tcp gssapi logging */
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
			CHKiRet(bufOptAdd('g', optarg));
#else
			fprintf(stderr, "rsyslogd: -g not valid - not compiled with gssapi support");
#endif
			break;
		case 'M': /* default module load path -- this MUST be carried out immediately! */
			glblModPath = (uchar*) optarg;
			break;
		case 'r':		/* accept remote messages */
#ifdef SYSLOG_INET
			CHKiRet(bufOptAdd(ch, optarg));
#else
			fprintf(stderr, "rsyslogd: -r not valid - not compiled with network support\n");
#endif
			break;
		case 't':		/* enable tcp logging */
#ifdef SYSLOG_INET
			CHKiRet(bufOptAdd(ch, optarg));
#else
			fprintf(stderr, "rsyslogd: -t not valid - not compiled with network support\n");
#endif
			break;
		case 'v': /* MUST be carried out immediately! */
			printVersion();
			exit(0); /* exit for -v option - so this is a "good one" */
		case '?':              
		default:
			usage();
		}
	}

	if ((argc -= optind))
		usage();

	DBGPRINTF("rsyslogd %s startup, compatibility mode %d, module path '%s'\n",
		  VERSION, iCompatibilityMode, glblModPath == NULL ? "" : (char*)glblModPath);

	/* we are done with the initial option parsing and processing. Now we init the system. */

	ppid = getpid();

	CHKiRet_Hdlr(InitGlobalClasses()) {
		fprintf(stderr, "rsyslogd initializiation failed - global classes could not be initialized.\n"
				"Did you do a \"make install\"?\n"
				"Suggested action: run rsyslogd with -d -n options to see what exactly "
				"fails.\n");
		FINALIZE;
	}

	/* doing some core initializations */

	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInternalInputName));
	CHKiRet(prop.SetString(pInternalInputName, UCHAR_CONSTANT("rsyslogd"), sizeof("rsyslgod") - 1));
	CHKiRet(prop.ConstructFinalize(pInternalInputName));

	CHKiRet(prop.Construct(&pLocalHostIP));
	CHKiRet(prop.SetString(pLocalHostIP, UCHAR_CONSTANT("127.0.0.1"), sizeof("127.0.0.1") - 1));
	CHKiRet(prop.ConstructFinalize(pLocalHostIP));

	/* get our host and domain names - we need to do this early as we may emit
	 * error log messages, which need the correct hostname. -- rgerhards, 2008-04-04
	 */
	net.getLocalHostname(&LocalFQDNName);
	CHKmalloc(LocalHostName = (uchar*) strdup((char*)LocalFQDNName));
	glbl.SetLocalFQDNName(LocalFQDNName); /* set the FQDN before we modify it */
	if((p = (uchar*)strchr((char*)LocalHostName, '.'))) {
		*p++ = '\0';
		LocalDomain = p;
	} else {
		LocalDomain = (uchar*)"";

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
		hent = gethostbyname((char*)LocalHostName);
		if(hent) {
			free(LocalHostName);
			CHKmalloc(LocalHostName = (uchar*)strdup(hent->h_name));
				
			if((p = (uchar*)strchr((char*)LocalHostName, '.')))
			{
				*p++ = '\0';
				LocalDomain = p;
			}
		}
	}

	/* Convert to lower case to recognize the correct domain laterly */
	for(p = LocalDomain ; *p ; p++)
		*p = (char)tolower((int)*p);
	
	/* we now have our hostname and can set it inside the global vars.
	 * TODO: think if all of this would better be a runtime function
	 * rgerhards, 2008-04-17
	 */
	glbl.SetLocalHostName(LocalHostName);
	glbl.SetLocalDomain(LocalDomain);
	glbl.GenerateLocalHostNameProperty(); /* must be redone after conf processing, FQDN setting may have changed */

	/* initialize the objects */
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

	/* END core initializations - we now come back to carrying out command line options*/

	while((iRet = bufOptRemove(&ch, &arg)) == RS_RET_OK) {
		DBGPRINTF("deque option %c, optarg '%s'\n", ch, (arg == NULL) ? "" : arg);
		switch((char)ch) {
                case '4':
	                glbl.SetDefPFFamily(PF_INET);
                        break;
                case '6':
                        glbl.SetDefPFFamily(PF_INET6);
                        break;
                case 'A':
                        send_to_all++;
                        break;
                case 'a':
			if(iCompatibilityMode < 3) {
				if(!bImUxSockLoaded) {
					legacyOptsEnq((uchar *) "ModLoad imuxsock");
					bImUxSockLoaded = 1;
				}
				snprintf((char *) legacyConfLine, sizeof(legacyConfLine), "addunixlistensocket %s", arg);
				legacyOptsEnq(legacyConfLine);
			} else {
				fprintf(stderr, "error -a is no longer supported, use module imuxsock instead");
			}
                        break;
		case 'f':		/* configuration file */
			ConfFile = (uchar*) arg;
			break;
		case 'g':		/* enable tcp gssapi logging */
			if(iCompatibilityMode < 3) {
				legacyOptsParseTCP(ch, arg);
			} else
				fprintf(stderr,	"-g option only supported in compatibility modes 0 to 2 - ignored\n");
			break;
		case 'h':
			if(iCompatibilityMode < 3) {
				errmsg.LogError(0, NO_ERRCODE, "WARNING: -h option is no longer supported - ignored");
			} else {
				usage(); /* for v3 and above, it simply is an error */
			}
			break;
		case 'i':		/* pid file name */
			PidFile = arg;
			break;
		case 'l':
			if(glbl.GetLocalHosts() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -l argument allowed, the first one is taken.\n");
			} else {
				glbl.SetLocalHosts(crunch_list(arg));
			}
			break;
		case 'm':		/* mark interval */
			if(iCompatibilityMode < 3) {
				MarkInterval = atoi(arg) * 60;
			} else
				fprintf(stderr,
					"-m option only supported in compatibility modes 0 to 2 - ignored\n");
			break;
		case 'n':		/* don't fork */
			NoFork = 1;
			break;
		case 'N':		/* enable config verify mode */
			iConfigVerify = atoi(arg);
			break;
                case 'o':
			if(iCompatibilityMode < 3) {
				if(!bImUxSockLoaded) {
					legacyOptsEnq((uchar *) "ModLoad imuxsock");
					bImUxSockLoaded = 1;
				}
				legacyOptsEnq((uchar *) "OmitLocalLogging");
			} else {
				fprintf(stderr, "error -o is no longer supported, use module imuxsock instead");
			}
                        break;
                case 'p':
			if(iCompatibilityMode < 3) {
				if(!bImUxSockLoaded) {
					legacyOptsEnq((uchar *) "ModLoad imuxsock");
					bImUxSockLoaded = 1;
				}
				snprintf((char *) legacyConfLine, sizeof(legacyConfLine), "SystemLogSocketName %s", arg);
				legacyOptsEnq(legacyConfLine);
			} else {
				fprintf(stderr, "error -p is no longer supported, use module imuxsock instead");
			}
		case 'q':               /* add hostname if DNS resolving has failed */
		        *net.pACLAddHostnameOnFail = 1;
		        break;
		case 'Q':               /* dont resolve hostnames in ACL to IPs */
		        *net.pACLDontResolve = 1;
		        break;
		case 'r':		/* accept remote messages */
			if(iCompatibilityMode < 3) {
				legacyOptsEnq((uchar *) "ModLoad imudp");
				snprintf((char *) legacyConfLine, sizeof(legacyConfLine), "UDPServerRun %s", arg);
				legacyOptsEnq(legacyConfLine);
			} else
				fprintf(stderr, "-r option only supported in compatibility modes 0 to 2 - ignored\n");
			break;
		case 's':
			if(glbl.GetStripDomains() != NULL) {
				fprintf (stderr, "rsyslogd: Only one -s argument allowed, the first one is taken.\n");
			} else {
				glbl.SetStripDomains(crunch_list(arg));
			}
			break;
		case 't':		/* enable tcp logging */
			if(iCompatibilityMode < 3) {
				legacyOptsParseTCP(ch, arg);
			} else
				fprintf(stderr,	"-t option only supported in compatibility modes 0 to 2 - ignored\n");
			break;
		case 'T':/* chroot() immediately at program startup, but only for testing, NOT security yet */
			if(chroot(arg) != 0) {
				perror("chroot");
				exit(1);
			}
			break;
		case 'u':		/* misc user settings */
			iHelperUOpt = atoi(arg);
			if(iHelperUOpt & 0x01)
				bParseHOSTNAMEandTAG = 0;
			if(iHelperUOpt & 0x02)
				bChDirRoot = 0;
			break;
		case 'w':		/* disable disallowed host warnigs */
			glbl.SetOption_DisallowWarning(0);
			break;
		case 'x':		/* disable dns for remote messages */
			glbl.SetDisableDNS(1);
			break;
		case '?':              
		default:
			usage();
		}
	}

	if(iRet != RS_RET_END_OF_LINKEDLIST)
		FINALIZE;

	if(iConfigVerify) {
		fprintf(stderr, "rsyslogd: version %s, config validation run (level %d), master config %s\n",
			VERSION, iConfigVerify, ConfFile);
	}

	if(bChDirRoot) {
		if(chdir("/") != 0)
			fprintf(stderr, "Can not do 'cd /' - still trying to run\n");
	}


	/* process compatibility mode settings */
	if(iCompatibilityMode < 4) {
		errmsg.LogError(0, NO_ERRCODE, "WARNING: rsyslogd is running in compatibility mode. Automatically "
		                            "generated config directives may interfer with your rsyslog.conf settings. "
					    "We suggest upgrading your config and adding -c4 as the first "
					    "rsyslogd option.");
	}

	if(iCompatibilityMode < 3) {
		if(MarkInterval > 0) {
			legacyOptsEnq((uchar *) "ModLoad immark");
			snprintf((char *) legacyConfLine, sizeof(legacyConfLine), "MarkMessagePeriod %d", MarkInterval);
			legacyOptsEnq(legacyConfLine);
		}
		if(!bImUxSockLoaded) {
			legacyOptsEnq((uchar *) "ModLoad imuxsock");
		}
	}

	if(bEOptionWasGiven && iCompatibilityMode < 3) {
		errmsg.LogError(0, NO_ERRCODE, "WARNING: \"message repeated n times\" feature MUST be turned on in "
					    "rsyslog.conf - CURRENTLY EVERY MESSAGE WILL BE LOGGED. Visit "
					    "http://www.rsyslog.com/rptdmsgreduction to learn "
					    "more and cast your vote if you want us to keep this feature.");
	}

	if(!iConfigVerify)
		CHKiRet(doGlblProcessInit());

	/* re-generate local host name property, as the config may have changed our FQDN settings */
	glbl.GenerateLocalHostNameProperty();

	CHKiRet(mainThread());

	/* do any de-init's that need to be done AFTER this comment */

	die(bFinished);
	
	thrdExit();

finalize_it:
	if(iRet == RS_RET_VALIDATION_RUN) {
		fprintf(stderr, "rsyslogd: End of config validation run. Bye.\n");
	} else if(iRet != RS_RET_OK) {
		fprintf(stderr, "rsyslogd run failed with error %d (see rsyslog.h "
				"or try http://www.rsyslog.com/e/%d to learn what that number means)\n", iRet, iRet*-1);
	}

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
/* vim:set ai:
 */
