/**
 * \brief This is the main file of the rsyslogd daemon.
 *
 * TODO:
 * - check template lines for extra characters and provide 
 *   a warning, if they exists
 * - implement the escape-cc property replacer option
 * - it looks liek the time stamp is missing on internally-generated
 *   messages - but maybe we need to keep this for compatibility
 *   reasons.
 *
 * Please note that as of now, a lot of the code in this file stems
 * from the sysklogd project. To learn more over this project, please
 * visit
 *
 * http://www.infodrom.org/projects/sysklogd/
 *
 * I would like to express my thanks to the developers of the sysklogd
 * package - without it, I would have had a much harder start...
 *
 * Please note that I made quite some changes to the orignal package.
 * I expect to do even more changes - up
 * to a full rewrite - to meet my design goals, which among others
 * contain a (at least) dual-thread design with a memory buffer for
 * storing received bursts of data. This is also the reason why I 
 * kind of "forked" a completely new branch of the package. My intension
 * is to do many changes and only this initial release will look
 * similar to sysklogd (well, one never knows...).
 *
 * As I have made a lot of modifications, please assume that all bugs
 * in this package are mine and not those of the sysklogd team.
 *
 * As of this writing, there already exist heavy
 * modifications to the orginal sysklogd package. I suggest to no
 * longer rely too much on code knowledge you eventually have with
 * sysklogd - rgerhards 2005-07-05
 * 
 * I have decided to put my code under the GPL. The sysklog package
 * is distributed under the BSD license. As such, this package here
 * currently comes with two licenses. Both are given below. As it is
 * probably hard for you to see what was part of the sysklogd package
 * and what is part of my code, I recommend that you visit the 
 * sysklogd site on the URL above if you would like to base your
 * development on a version that is not under the GPL.
 *
 * This Project was intiated and is maintened by
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
 * Copyright 2003-2005 Rainer Gerhards and Adiscon GmbH.
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
#ifdef __FreeBSD__
#define	BSD
#endif

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
 */
#define	MAXLINE		1024		/* maximum line length */
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
#ifdef SYSV
#include <sys/types.h>
#endif
#include <utmp.h>
#include <ctype.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <time.h>

#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/param.h>
#ifdef	__sun
#include <errno.h>
#else
#include <sys/errno.h>
#endif
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/file.h>
#ifdef SYSV
#include <fcntl.h>
#else
#include <sys/msgbuf.h>
#endif
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#include <netinet/in.h>
#include <netdb.h>
#ifndef __sun
#ifndef BSD
#include <syscall.h>
#endif
#endif
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#ifndef TESTING
#include "pidfile.h"
#endif
#include "version.h"

#include <assert.h>

#ifdef	WITH_DB
#include "mysql/mysql.h" 
#include "mysql/errmsg.h"
#endif

#if defined(__linux__)
#include <paths.h>
#endif

/* handle some defines missing on more than one platform */
#ifndef SUN_LEN
#define SUN_LEN(su) \
   (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

/* missing definitions for solaris
 * 2006-02-16 Rger
 */
#ifdef __sun
#define LOG_AUTHPRIV LOG_AUTH
#define	LOG_MAKEPRI(fac, pri)	(((fac) << 3) | (pri))
#define	LOG_PRI(p)	((p) & LOG_PRIMASK)
#define	LOG_FAC(p)	(((p) & LOG_FACMASK) >> 3)
#define INTERNAL_NOPRI  0x10    /* the "no priority" priority */
#define LOG_FTP         (11<<3) /* ftp daemon */
#define INTERNAL_MARK   LOG_MAKEPRI((LOG_NFACILITIES<<3), 0)
typedef struct _code {
        char    *c_name;
        int     c_val;
} CODE;
 
CODE prioritynames[] =
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

CODE facilitynames[] =
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
#endif

#include "rsyslog.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "stringbuf.h"
#include "parse.h"

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#ifdef	WITH_DB
#define	_DB_MAXDBLEN	128	/* maximum number of db */
#define _DB_MAXUNAMELEN	128	/* maximum number of user name */
#define	_DB_MAXPWDLEN	128 	/* maximum number of user's pass */
#define _DB_DELAYTIMEONERROR	20	/* If an error occur we stop logging until
					   a delayed time is over */
#endif

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

#ifndef _PATH_LOG
#ifdef BSD
#define _PATH_LOG	"/var/run/log"
#else
#define _PATH_LOG	"/dev/log"
#endif
#endif

/* The following #ifdef sequence is a small compatibility 
 * layer. It tries to work around the different availality
 * levels of SO_BSDCOMPAT on linuxes...
 * I borrowed this code from
 *    http://www.erlang.org/ml-archive/erlang-questions/200307/msg00037.html
 * It still needs to be a bit better adapted to rsyslog.
 * rgerhards 2005-09-19
 */
#ifndef BSD
#include <sys/utsname.h>
static int should_use_so_bsdcompat(void)
{
    static int init_done;
    static int so_bsdcompat_is_obsolete;

    if (!init_done) {
	struct utsname utsname;
	unsigned int version, patchlevel;

	init_done = 1;
	if (uname(&utsname) < 0) {
	    dprintf("uname: %s\r\n", strerror(errno));
	    return 1;
	}
	/* Format is <version>.<patchlevel>.<sublevel><extraversion>
	   where the first three are unsigned integers and the last
	   is an arbitrary string. We only care about the first two. */
	if (sscanf(utsname.release, "%u.%u", &version, &patchlevel) != 2) {
	    dprintf("uname: unexpected release '%s'\r\n",
		    utsname.release);
	    return 1;
	}
	/* SO_BSCOMPAT is deprecated and triggers warnings in 2.5
	   kernels. It is a no-op in 2.4 but not in 2.2 kernels. */
	if (version > 2 || (version == 2 && patchlevel >= 5))
	    so_bsdcompat_is_obsolete = 1;
    }
    return !so_bsdcompat_is_obsolete;
}
#else	/* #ifndef BSD */
#define should_use_so_bsdcompat() 1
#endif	/* #ifndef BSD */
#ifndef SO_BSDCOMPAT
/* this shall prevent compiler errors due to undfined name */
#define SO_BSDCOMPAT 0
#endif

static char	*ConfFile = _PATH_LOGCONF; /* read-only after startup */
static char	*PidFile = _PATH_LOGPID; /* read-only after startup */
static char	ctty[] = _PATH_CONSOLE;	/* this is read-only */

static char	**parts;
/* parts is read-write in the code handling message reception. It is not
 * modified once the message has been received. So it should be safe to
 * not guard access to it once we have completed msg object compilation.
 * rgerhards 2005-10-24
 */

static int inetm = 0; /* read-only after init, except when HUPed */
static pid_t myPid;	/* our pid for use in self-generated messages, e.g. on startup */
/* mypid is read-only after the initial fork() */
static int debugging_on = 0; /* read-only, except on sig USR1 */
static int restart = 0; /* do restart (config read) - multithread safe */

static int bRequestDoMark = 0; /* do mark processing? (multithread safe) */
#define MAXFUNIX	20

int nfunix = 1; /* number of Unix sockets open / read-only after startup */
int startIndexUxLocalSockets = 0; /* process funix from that index on (used to 
 				   * suppress local logging. rgerhards 2005-08-01
				   * read-only after startup
				   */
int funixParseHost[MAXFUNIX] = { 0, }; /* should parser parse host name?  read-only after startup */
char *funixn[MAXFUNIX] = { _PATH_LOG }; /* read-only after startup */
int funix[MAXFUNIX] = { -1, }; /* read-only after startup */

#ifdef UT_NAMESIZE
# define UNAMESZ	UT_NAMESIZE	/* length of a login name */
#else
# define UNAMESZ	8	/* length of a login name */
#endif
#define MAXUNAMES	20	/* maximum number of user names */
#define MAXFNAME	200	/* max file pathname length */

#define INTERNAL_NOPRI	0x10	/* the "no priority" priority */
#define TABLE_NOPRI	0	/* Value to indicate no priority in f_pmask */
#define TABLE_ALLPRI    0xFF    /* Value to indicate all priorities in f_pmask */
#define	LOG_MARK	LOG_MAKEPRI(LOG_NFACILITIES, 0)	/* mark "facility" */

/* Flags to logmsg().
 */
/* NO LONGER NEEDED: #define IGN_CONS	0x001	* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */

/*
 * This table contains plain text for h_errno errors used by the
 * net subsystem.
 */
static const char *sys_h_errlist[] = {
    "No problem",						/* NETDB_SUCCESS */
    "Authoritative answer: host not found",			/* HOST_NOT_FOUND */
    "Non-authoritative answer: host not found, or serverfail",	/* TRY_AGAIN */
    "Non recoverable errors",					/* NO_RECOVERY */
    "Valid name, no data record of requested type",		/* NO_DATA */
    "no address, look for MX record"				/* NO_ADDRESS */
 };

/*
 * This table lists the directive lines:
 */
static const char *directive_name_list[] = {
	"template",
	"outchannel"
};
/* ... and their definitions: */
enum eDirective { DIR_TEMPLATE = 0, DIR_OUTCHANNEL = 1,
                  DIR_ALLOWEDSENDER = 2};

/* rgerhards 2004-11-11: the following structure represents
 * a time as it is used in syslog.
 */
struct syslogTime {
	int timeType;	/* 0 - unitinialized , 1 - RFC 3164, 2 - syslog-protocol */
	int year;
	int month;
	int day;
	int hour; /* 24 hour clock */
	int minute;
	int second;
	int secfrac;	/* fractional seconds (must be 32 bit!) */
	int secfracPrecision;
	char OffsetMode;	/* UTC offset + or - */
	char OffsetHour;	/* UTC offset in hours */
	int OffsetMinute;	/* UTC offset in minutes */
	/* full UTC offset minutes = OffsetHours*60 + OffsetMinute. Then use
	 * OffsetMode to know the direction.
	 */
};

/* rgerhards 2004-11-08: The following structure represents a
 * syslog message. 
 *
 * Important Note:
 * The message object is used for multiple purposes (once it
 * has been created). Once created, it actully is a read-only
 * object (though we do not specifically express this). In order
 * to avoid multiple copies of the same object, we use a
 * reference counter. This counter is set to 1 by the constructer
 * and increased by 1 with a call to MsgAddRef(). The destructor
 * checks the reference count. If it is more than 1, only the counter
 * will be decremented. If it is 1, however, the object is actually
 * destroyed. To make this work, it is vital that MsgAddRef() is
 * called each time a "copy" is stored somewhere.
 */
struct msg {
	int	iRefCount;	/* reference counter (0 = unused) */
	short	iSyslogVers;	/* version of syslog protocol
				 * 0 - RFC 3164
				 * 1 - RFC draft-protocol-08 */
	short	bParseHOSTNAME;	/* should the hostname be parsed from the message? */
	   /* background: the hostname is not present on "regular" messages
	    * received via UNIX domain sockets from the same machine. However,
	    * it is available when we have a forwarder (e.g. rfc3195d) using local
	    * sockets. All in all, the parser would need parse templates, that would
	    * resolve all these issues... rgerhards, 2005-10-06
	    */
	short	iSeverity;	/* the severity 0..7 */
	char *pszSeverity;	/* severity as string... */
	int iLenSeverity;	/* ... and its length. */
	int	iFacility;	/* Facility code (up to 2^32-1) */
	char *pszFacility;	/* Facility as string... */
	int iLenFacility;	/* ... and its length. */
	char *pszPRI;		/* the PRI as a string */
	int iLenPRI;		/* and its length */
	char	*pszRawMsg;	/* message as it was received on the
				 * wire. This is important in case we
				 * need to preserve cryptographic verifiers.
				 */
	int	iLenRawMsg;	/* length of raw message */
	char	*pszMSG;	/* the MSG part itself */
	int	iLenMSG;	/* Length of the MSG part */
	char	*pszUxTradMsg;	/* the traditional UNIX message */
	int	iLenUxTradMsg;/* Length of the traditional UNIX message */
	char	*pszTAG;	/* pointer to tag value */
	int	iLenTAG;	/* Length of the TAG part */
	char	*pszHOSTNAME;	/* HOSTNAME from syslog message */
	int	iLenHOSTNAME;	/* Length of HOSTNAME */
	char	*pszRcvFrom;	/* System message was received from */
	int	iLenRcvFrom;	/* Length of pszRcvFrom */
	int	iProtocolVersion;/* protocol version of message received 0 - legacy, 1 syslog-protocol) */
	rsCStrObj *pCSProgName;	/* the (BSD) program name */
	rsCStrObj *pCSStrucData;/* STRUCTURED-DATA */
	rsCStrObj *pCSAPPNAME;	/* APP-NAME */
	rsCStrObj *pCSPROCID;	/* PROCID */
	rsCStrObj *pCSMSGID;	/* MSGID */
	struct syslogTime tRcvdAt;/* time the message entered this program */
	char *pszRcvdAt3164;	/* time as RFC3164 formatted string (always 15 charcters) */
	char *pszRcvdAt3339;	/* time as RFC3164 formatted string (32 charcters at most) */
	char *pszRcvdAt_MySQL;	/* rcvdAt as MySQL formatted string (always 14 charcters) */
	struct syslogTime tTIMESTAMP;/* (parsed) value of the timestamp */
	char *pszTIMESTAMP3164;	/* TIMESTAMP as RFC3164 formatted string (always 15 charcters) */
	char *pszTIMESTAMP3339;	/* TIMESTAMP as RFC3339 formatted string (32 charcters at most) */
	char *pszTIMESTAMP_MySQL;/* TIMESTAMP as MySQL formatted string (always 14 charcters) */
	int msgFlags;		/* flags associated with this message */
};


/* values for host comparisons specified with host selector blocks
 * (+host, -host). rgerhards 2005-10-18.
 */
enum _EHostnameCmpMode {
	HN_NO_COMP = 0, /* do not compare hostname */
	HN_COMP_MATCH = 1, /* hostname must match */
	HN_COMP_NOMATCH = 2 /* hostname must NOT match */
};
typedef enum _EHostnameCmpMode EHostnameCmpMode;

/*
 * This structure represents the files that will have log
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
 */
struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	off_t	f_sizeLimit;		/* file size limit, 0 = no limit */
	char	*f_sizeLimitCmd;	/* command to carry out when size limit is reached */
#ifdef	WITH_DB
	MYSQL	f_hmysql;		/* handle to MySQL */
	/* TODO: optimize memory layout & consumption; rgerhards 2004-11-19 */
	char	f_dbsrv[MAXHOSTNAMELEN+1];	/* IP or hostname of DB server*/ 
	char	f_dbname[_DB_MAXDBLEN+1];	/* DB name */
	char	f_dbuid[_DB_MAXUNAMELEN+1];	/* DB user */
	char	f_dbpwd[_DB_MAXPWDLEN+1];	/* DB user's password */
	time_t	f_timeResumeOnError;		/* 0 if no error is present,	
						   otherwise is it set to the
						   time a retrail should be attampt */
	int	f_iLastDBErrNo;			/* Last db error number. 0 = no error */
#endif
	time_t	f_time;			/* time this was last written */
	/* filter properties */
	enum {
		FILTER_PRI = 0,		/* traditional PRI based filer */
		FILTER_PROP = 1		/* extended filter, property based */
	} f_filter_type;
	EHostnameCmpMode eHostnameCmpMode;
	rsCStrObj *pCSHostnameComp;/* hostname to check */
	rsCStrObj *pCSProgNameComp;	/* tag to check or NULL, if not to be checked */
	union {
		u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
		struct {
			rsCStrObj *pCSPropName;
			enum {
				FIOP_NOP = 0,		/* do not use - No Operation */
				FIOP_CONTAINS  = 1,	/* contains string? */
				FIOP_ISEQUAL  = 2,	/* is (exactly) equal? */
				FIOP_STARTSWITH = 3	/* starts with a string? */
			} operation;
			rsCStrObj *pCSCompValue;	/* value to "compare" against */
			char isNegated;			/* actually a boolean ;) */
		} prop;
	} f_filterData;
	union {
		char	f_uname[MAXUNAMES][UNAMESZ+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN+1];
			struct sockaddr_in	f_addr;
			int port;
			int protocol;
#			define	FORW_UDP 0
#			define	FORW_TCP 1
			/* following fields for TCP-based delivery */
			enum TCPSendStatus {
				TCP_SEND_NOTCONNECTED = 0,
				TCP_SEND_CONNECTING = 1,
				TCP_SEND_READY = 2
			} status;
			char *savedMsg;
#			ifdef USE_PTHREADS
			pthread_mutex_t mtxTCPSend;
#			endif
		} f_forw;		/* forwarding address */
		char	f_fname[MAXFNAME];
	} f_un;
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN+1];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	int	f_repeatcount;			/* number of "repeated" msgs */
	int	f_flags;			/* store some additional flags */
	struct template *f_pTpl;		/* pointer to template to use */
	struct iovec *f_iov;			/* dyn allocated depinding on template */
	unsigned short *f_bMustBeFreed;		/* indicator, if iov_base must be freed to destruct */
	int	f_iIovUsed;			/* nbr of elements used in IOV */
	char	*f_psziov;			/* iov as string */
	int	f_iLenpsziov;			/* length of iov as string */
	struct msg* f_pMsg;			/* pointer to the message (this wil
					         * replace the other vars with msg
						 * content later). This is preserved after
						 * the message has been processed - it is
						 * also used to detect duplicates. */
};

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

/* supporting structures for multithreading */
#ifdef USE_PTHREADS
/* this is the first approach to a queue, this time with static
 * memory.
 */
#define QUEUESIZE 10000
typedef struct {
	void* buf[QUEUESIZE];
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} msgQueue;

int bRunningMultithreaded = 0;	/* Is this program running in multithreaded mode? */
msgQueue *pMsgQueue = NULL;
static pthread_t thrdWorker;
static int bGlblDone = 0;
#endif
/* END supporting structures for multithreading */

static int bParseHOSTNAMEandTAG = 1; /* global config var: should the hostname and tag be
                                      * parsed inside message - rgerhards, 2006-03-13 */
static int bFinished = 0;	/* used by termination signal handler, read-only except there
				 * is either 0 or the number of the signal that requested the
 				 * termination.
				 */

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 60 };	/* # of secs before flush */
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}
#ifdef SYSLOG_INET
#define INET_SUSPEND_TIME 60		/* equal to 1 minute 
					 * rgerhards, 2005-07-26: This was 3 minutes. As the
					 * same timer is used for tcp based syslog, we have
					 * reduced it. However, it might actually be worth
					 * thinking about a buffered tcp sender, which would be 
					 * a much better alternative. When that happens, this
					 * time here can be re-adjusted to 3 minutes (or,
					 * even better, made configurable).
					 */
#define INET_RETRY_MAX 30		/* maximum of retries for gethostbyname() */
	/* was 10, changed to 30 because we reduced INET_SUSPEND_TIME by one third. So
	 * this "fixes" some of implications of it (see comment on INET_SUSPEND_TIME).
	 * rgerhards, 2005-07-26
	 */
#endif

#define LIST_DELIMITER	':'		/* delimiter between two hosts */

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_FORW_SUSP	7		/* suspended host forwarding */
#define F_FORW_UNKN	8		/* unknown host forwarding */
#define F_PIPE		9		/* named pipe */
#define F_MYSQL		10		/* MySQL database */
#define F_DISCARD	11		/* discard event (do not process any further selector lines) */
#define F_SHELL		12		/* execute a shell */
char	*TypeNames[] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"FORW(SUSPENDED)",
	"FORW(UNKNOWN)", "PIPE", 	"MYSQL",	"DISCARD",
	"SHELL"
};

struct	filed *Files = NULL; /* read-only after init() (but beware of sigusr1!) */
struct	filed consfile; /* initialized on startup, used during actions - maybe NON THREAD-SAFE */
struct 	filed emergfile; /* this is only used for emergency logging when
			  * no actual config has been loaded.
			  * useded during actions in emergencase - thread-safety doubtful
			  */

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

static int	Debug;		/* debug flag  - read-only after startup */
static char	LocalHostName[MAXHOSTNAMELEN+1];/* our hostname  - read-only after startup */
static char	*LocalDomain;	/* our local domain name  - read-only after startup */
static int	InetInuse = 0;	/* non-zero if INET sockets are being used
				 * read-only after init(), but beware of restart! */
static int	finet = -1;	/* Internet datagram socket *
				 * read-only after init(), but beware of restart! */
static int	LogPort = 0;	/* port number for INET connections - read-only after startup */
static int	MarkInterval = 20 * 60;	/* interval between marks in seconds - read-only after startup */
static int	MarkSeq = 0;	/* mark sequence number - modified in domark() only */
static int	NoFork = 0; 	/* don't fork - don't run in daemon mode - read-only after startup */
static int	AcceptRemote = 0;/* receive messages that come via UDP - read-only after startup */
static char	**StripDomains = NULL;/* these domains may be stripped before writing logs  - r/o after s.u.*/
static char	**LocalHosts = NULL;/* these hosts are logged with their hostname  - read-only after startup*/
static int	NoHops = 1;	/* Can we bounce syslog messages through an
				   intermediate host.  Read-only after startup */
static int     Initialized = 0; /* set when we have initialized ourselves
                                 * rgerhards 2004-11-09: and by initialized, we mean that
                                 * the configuration file could be properly read AND the
                                 * syslog/udp port could be obtained (the later is debatable).
                                 * It is mainly a setting used for emergency logging: if
                                 * something really goes wild, we can not do as indicated in
                                 * the log file, but we still log messages to the system
                                 * console. This is probably the best that can be done in
                                 * such a case.
				 * read-only after startup, but modified during restart
                                 */

extern	int errno;


/* support for simple textual representatio of FIOP names
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
		default:
			pRet = "NOP";
			break;
	}
	return pRet;
}


/* support for defining allowed TCP and UDP senders. We use the same
 * structure to implement this (a linked list), but we define two different
 * list roots, one for UDP and one for TCP.
 * rgerhards, 2005-09-26
 */
#ifdef SYSLOG_INET
struct AllowedSenders {
	unsigned long allowedSender;/* ip address allowed */
	unsigned char bitsToShift;  /* defines how many bits should be discarded (eqiv to mask) */
	struct AllowedSenders *pNext;
};

/* All of the five below are read-only after startup */
static struct AllowedSenders *pAllowedSenders_UDP = NULL; /* the roots of the allowed sender */
static struct AllowedSenders *pAllowedSenders_TCP = NULL; /* lists. If NULL, all senders are ok! */
static struct AllowedSenders *pLastAllowedSenders_UDP = NULL; /* and now the pointers to the last */
static struct AllowedSenders *pLastAllowedSenders_TCP = NULL; /* element in the respective list */
#endif /* #ifdef SYSLOG_INET */

static int option_DisallowWarning = 1;	/* complain if message from disallowed sender is received */


/* hardcoded standard templates (used for defaults) */
static char template_TraditionalFormat[] = "\"%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::drop-last-lf%\n\"";
static char template_WallFmt[] = "\"\r\n\7Message from syslogd@%HOSTNAME% at %timegenerated% ...\r\n %syslogtag%%msg%\n\r\"";
static char template_StdFwdFmt[] = "\"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag%%msg%\"";
static char template_StdUsrMsgFmt[] = "\" %syslogtag%%msg%\n\r\"";
static char template_StdDBFmt[] = "\"insert into SystemEvents (Message, Facility, FromHost, Priority, DeviceReportedTime, ReceivedAt, InfoUnitID, SysLogTag) values ('%msg%', %syslogfacility%, '%HOSTNAME%', %syslogpriority%, '%timereported:::date-mysql%', '%timegenerated:::date-mysql%', %iut%, '%syslogtag%')\",SQL";
/* end template */


/* up to the next comment, prototypes that should be removed by reordering */
#ifdef USE_PTHREADS
static msgQueue *queueInit (void);
static void *singleWorker(void *vParam); /* REMOVEME later 2005-10-24 */
#endif
/* Function prototypes. */
static rsRetVal aquirePROCIDFromTAG(struct msg *pM);
static char* getProgramName(struct msg*);
static char **crunch_list(char *list);
static void printchopped(char *hname, char *msg, int len, int fd, int iSourceType);
static void printline(char *hname, char *msg, int iSource);
static void logmsg(int pri, struct msg*, int flags);
static void fprintlog(register struct filed *f);
static void wallmsg(register struct filed *f);
static void reapchild();
static const char *cvthname(struct sockaddr_in *f);
static void debug_switch();
static rsRetVal cfline(char *line, register struct filed *f);
static int decode(char *name, struct code *codetab);
static void sighup_handler();
#ifdef WITH_DB
static void initMySQL(register struct filed *f);
static void writeMySQL(register struct filed *f);
static void closeMySQL(register struct filed *f);
static void reInitMySQL(register struct filed *f);
static int checkDBErrorState(register struct filed *f);
static void DBErrorHandler(register struct filed *f);
#endif

static int getSubString(char **pSrc, char *pDst, size_t DstSize, char cSep);
static void getCurrTime(struct syslogTime *t);
static void cflineSetTemplateAndIOV(struct filed *f, char *pTemplateName);

#ifdef SYSLOG_UNIXAF
static int create_udp_socket();
#endif


/* Access functions for the struct filed. These functions are primarily
 * necessary to make things thread-safe. Consequently, they are slim
 * if we compile without pthread support.
 * rgerhards 2005-10-24
 */

/* END Access functions for the struct filed */

/* Code for handling allowed/disallowed senders
 */

#ifdef SYSLOG_INET
/* function to add an allowed sender to the allowed sender list. The
 * root of the list is caller-provided, so it can be used for all
 * supported lists. The caller must provide a pointer to the root,
 * as it eventually needs to be updated. Also, a pointer to the
 * pointer to the last element must be provided (to speed up adding
 * list elements).
 * rgerhards, 2005-09-26
 */
static rsRetVal AddAllowedSender(struct AllowedSenders **ppRoot, struct AllowedSenders **ppLast,
		     		 unsigned int iAllow, int iSignificantBits)
{
	struct AllowedSenders *pEntry;

	assert(ppRoot != NULL);
	assert(ppLast != NULL);

	if((pEntry = (struct AllowedSenders*) calloc(1, sizeof(struct AllowedSenders)))
	   == NULL)
	   	return RS_RET_OUT_OF_MEMORY; /* no options left :( */

	if(iSignificantBits == 0)
		/* we handle this seperatly just to provide a better
		 * error message.
		 */
		logerror("You can not specify 0 bits of the netmask, this would "
		         "match ALL systems. If you really intend to do that, "
			 "remove all $AllowedSender directives.");
	if((iSignificantBits < 1) || (iSignificantBits > 32)) {
		logerrorInt("Invalid bit number in IP address - adjusted to 32",
			    iSignificantBits);
		iSignificantBits = 32;
	}

	/* populate entry */
	pEntry->bitsToShift = 32 - iSignificantBits; /* IPv4! */
	pEntry->allowedSender = iAllow >> pEntry->bitsToShift;
	pEntry->pNext = NULL;

	/* enqueue */
	if(*ppRoot == NULL) {
		*ppRoot = pEntry;
	} else {
		(*ppLast)->pNext = pEntry;
	}
	*ppLast = pEntry;

	return RS_RET_OK;
}
#endif /* #ifdef SYSLOG_INET */


#ifdef SYSLOG_INET
/* Print an allowed sender list. The caller must tell us which one.
 * iListToPrint = 1 means UDP, 2 means TCP
 * rgerhards, 2005-09-27
 */
static void PrintAllowedSenders(int iListToPrint)
{
	struct AllowedSenders *pSender;
	unsigned uSender;

	assert((iListToPrint == 1) || (iListToPrint == 2));

	printf("\nAllowed %s Senders:\n",
	       (iListToPrint == 1) ? "UDP" : "TCP");
	pSender = (iListToPrint == 1) ?
		  pAllowedSenders_UDP : pAllowedSenders_TCP;
	if(pSender == NULL) {
		printf("\tNo restrictions set.\n");
	} else {
		while(pSender != NULL) {
			uSender = pSender->allowedSender  << pSender->bitsToShift;
			/* it might be wiser to use socket functions to
			 * do the conversion, but we need to change it for
			 * IPv6 anyhow... so we use the quick way.
			 */
			printf("\t%u.%u.%u.%u/%u\n",
			       uSender >> 24 & 0xff,
			       uSender >> 16 & 0xff,
			       uSender >>  8 & 0xff,
			       uSender       & 0xff,
			       32 - pSender->bitsToShift
			      );
		    pSender = pSender->pNext;
		}
	}
}
#endif /* #ifdef SYSLOG_INET */

#ifdef SYSLOG_INET
/* check if a sender is allowed. The root of the the allowed sender.
 * list must be proveded by the caller. As such, this function can be
 * used to check both UDP and TCP allowed sender lists.
 * returns 1, if the sender is allowed, 0 otherwise.
 * rgerhads, 2005-09-26
 */
static int isAllowedSender(struct AllowedSenders *pAllowRoot, struct sockaddr_in *pFrom)
{
	struct AllowedSenders *pAllow;
	unsigned long ulAddrInLocalByteOrder;

	assert(pFrom != NULL);

	if(pAllowRoot == NULL)
		return 1; /* checking disabled, everything is valid! */

	if(pFrom->sin_family != AF_INET)
		return 0; /* malformed, so by default no valid sender! */

	ulAddrInLocalByteOrder = ntohl(pFrom->sin_addr.s_addr);

	/* now we loop through the list of allowed senders. As soon as
	 * we find a match, we return back (indicating allowed). We loop
	 * until we are out of allowed senders. If so, we fall through the
	 * loop and the function's terminal return statement will indicate
	 * that the sender is disallowed.
	 */
	for(pAllow = pAllowRoot ; pAllow != NULL ; pAllow = pAllow->pNext) {
		if(   (ulAddrInLocalByteOrder >> pAllow->bitsToShift)
		   == pAllow->allowedSender)
		   	return 1;
	}
	dprintf("%x is not an allowed sender\n", (unsigned) ulAddrInLocalByteOrder);
	return 0;
}
#endif /* #ifdef SYSLOG_INET */



/********************************************************************
 *                    ###  SYSLOG/TCP CODE ###
 * This is code for syslog/tcp. This code would belong to a separate
 * file - but I have put it here to avoid hassle with CVS. Over
 * time, I expect rsyslog to utilize liblogging for actual network
 * I/O. So the tcp code will be (re)moved some time. I don't like
 * to add a new file to cvs that I will push to the attic in just
 * a few weeks (month at most...). So I simply add the code here.
 *
 * Place no unrelated code between this comment and the
 * END tcp comment!
 *
 * 2005-07-04 RGerhards (Happy independence day to our US friends!)
 ********************************************************************/
#ifdef SYSLOG_INET

#define TCPSESS_MAX 100 /* TODO: remove hardcoded limit */
static int TCPLstnPort = 0; /* read-only after startup */
static int bEnableTCP = 0; /* read-only after startup */
static int sockTCPLstn = -1; /* read-only after startup, modified by restart */
struct TCPSession {
	int sock;
	int iMsg; /* index of next char to store in msg */
	char msg[MAXLINE+1];
	char *fromHost;
} TCPSessions[TCPSESS_MAX];
/* The thread-safeness of the sesion table is doubtful */


/* Initialize the session table
 */
static void TCPSessInit(void)
{
	register int i;

	for(i = 0 ; i < TCPSESS_MAX ; ++i) {
		TCPSessions[i].sock = -1; /* no sock */
		TCPSessions[i].iMsg = 0; /* just make sure... */
	}
}


/* find a free spot in the session table. If the table
 * is full, -1 is returned, else the index of the free
 * entry (0 or higher).
 */
static int TCPSessFindFreeSpot(void)
{
	register int i;

	for(i = 0 ; i < TCPSESS_MAX ; ++i) {
		if(TCPSessions[i].sock == -1)
			break;
	}

	return((i < TCPSESS_MAX) ? i : -1);
}


/* Get the next session index. Free session tables entries are
 * skipped. This function is provided the index of the last
 * session entry, or -1 if no previous entry was obtained. It
 * returns the index of the next session or -1, if there is no
 * further entry in the table. Please note that the initial call
 * might as well return -1, if there is no session at all in the
 * session table.
 */
static int TCPSessGetNxtSess(int iCurr)
{
	register int i;

	for(i = iCurr + 1 ; i < TCPSESS_MAX ; ++i)
		if(TCPSessions[i].sock != -1)
			break;

	return((i < TCPSESS_MAX) ? i : -1);
}


/* Initialize TCP sockets (for listener)
 */
static int create_tcp_socket(void)
{
	int fd, on = 1;
	struct sockaddr_in sin;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		logerror("syslog: TCP: Unknown protocol, suspending tcp inet service.");
		return fd;
	}

	if(TCPLstnPort == 0) {
		TCPLstnPort = 514; /* use default, no error */
	}
	if(TCPLstnPort < 1 || TCPLstnPort > 65535) {
		TCPLstnPort = 514; /* just let's use our default... */
		errno = 0;
		logerror("TCP listen port was invalid, changed to 514");
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(TCPLstnPort);
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, \
		       (char *) &on, sizeof(on)) < 0 ) {
		logerror("setsockopt(REUSEADDR), suspending tcp inet");
		close(fd);
		return -1;
	}
	/* We need to enable BSD compatibility. Otherwise an attacker
	 * could flood our log files by sending us tons of ICMP errors.
	 */
#ifndef BSD	
	if (should_use_so_bsdcompat()) {
		if (setsockopt(fd, SOL_SOCKET, SO_BSDCOMPAT, \
				(char *) &on, sizeof(on)) < 0) {
			logerror("setsockopt(BSDCOMPAT), suspending tcp inet");
			close(fd);
			return -1;
		}
	}
#endif
	if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		logerror("bind, suspending tcp inet");
		close(fd);
		return -1;
	}

	if(listen(fd, 5) < 0) {
		logerror("listen, suspending tcp inet");
		close(fd);
		return -1;
	}

	sockTCPLstn = fd;

	/* OK, we had success. Now it is also time to
	 * initialize our connections
	 */
	TCPSessInit();

	dprintf("Opened TCP socket %d.\n", sockTCPLstn);

	return fd;
}

/* Accept new TCP connection; make entry in session table. If there
 * is no more space left in the connection table, the new TCP
 * connection is immediately dropped.
 */
static void TCPSessAccept(void)
{
	int newConn;
	int iSess;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(struct sockaddr);
	int lenHostName;
	char *fromHost;
	char *pBuf;

	newConn = accept(sockTCPLstn, (struct sockaddr*) &addr, &addrlen);
	if (newConn < 0) {
		logerror("tcp accept, ignoring error and connection request");
		return;
	}

	/* Add to session list */
	iSess = TCPSessFindFreeSpot();
	if(iSess == -1) {
		errno = 0;
		logerror("too many tcp sessions - dropping incoming request");
		close(newConn);
		return;
	}

	/* OK, we have a "good" index... */
	/* get the host name */
	fromHost = (char *)cvthname(&addr);

	/* Here we check if a host is permitted to send us
	 * syslog messages. If it isn't, we do not further
	 * process the message but log a warning (if we are
	 * configured to do this).
	 * rgerhards, 2005-09-26
	 */
	if(!isAllowedSender(pAllowedSenders_TCP, &addr)) {
		if(option_DisallowWarning) {
			errno = 0;
			logerrorSz("TCP message from disallowed sender %s discarded",
				   fromHost);
		}
		close(newConn);
		return;
	}

	/* OK, we have an allowed sender, so let's continue */
	lenHostName = strlen(fromHost) + 1; /* for \0 byte */
	if((pBuf = (char*) malloc(sizeof(char) * lenHostName)) == NULL) {
		logerror("couldn't allocate buffer for hostname - ignored");
		TCPSessions[iSess].fromHost = "NO-MEMORY-FOR-HOSTNAME";
	} else {
		memcpy(pBuf, fromHost, lenHostName);
		TCPSessions[iSess].fromHost = pBuf;
	}

	TCPSessions[iSess].sock = newConn;
	TCPSessions[iSess].iMsg = 0; /* init msg buffer! */
}


/* Closes a TCP session and marks its slot in the session
 * table as unused. No attention is paid to the return code
 * of close, so potential-double closes are not detected.
 */
static void TCPSessClose(int iSess)
{
	if(iSess < 0 || iSess > TCPSESS_MAX) {
		errno = 0;
		logerror("internal error, trying to close an invalid TCP session!");
		return;
	}

	close(TCPSessions[iSess].sock);
	TCPSessions[iSess].sock = -1;
	free(TCPSessions[iSess].fromHost);
	TCPSessions[iSess].fromHost = NULL; /* not really needed, but... */
}


/* Processes the data received via a TCP session. If there
 * is no other way to handle it, data is discarded.
 * Input parameter data is the data received, iLen is its
 * len as returned from recv(). iLen must be 1 or more (that
 * is errors must be handled by caller!). iTCPSess must be
 * the index of the TCP session that received the data.
 * rgerhards 2005-07-04
 */
static void TCPSessDataRcvd(int iTCPSess, char *pData, int iLen)
{
	register int iMsg;
	char *pMsg;
	char *pEnd;
	assert(pData != NULL);
	assert(iLen > 0);
	assert(iTCPSess >= 0);
	assert(iTCPSess < TCPSESS_MAX);
	assert(TCPSessions[iTCPSess].sock != -1);

	 /* We now copy the message to the session buffer. As
	  * it looks, we need to do this in any case because
	  * we might run into multiple messages inside a single
	  * buffer. Of course, we could think about optimizations,
	  * but as this code is to be replaced by liblogging, it
	  * probably doesn't make so much sense...
	  * rgerhards 2005-07-04
	  *
	  * Algo:
	  * - copy message to buffer until the first LF is found
	  * - printline() the buffer
	  * - continue with copying
	  */
	iMsg = TCPSessions[iTCPSess].iMsg; /* copy for speed */
	pMsg = TCPSessions[iTCPSess].msg; /* just a shortcut */
	pEnd = pData + iLen; /* this is one off, which is intensional */

	while(pData < pEnd) {
		if(iMsg >= MAXLINE) {
			/* emergency, we now need to flush, no matter if
			 * we are at end of message or not...
			 */
			*(pMsg + iMsg) = '\0'; /* space *is* reserved for this! */
			printline(TCPSessions[iTCPSess].fromHost, pMsg, 1);
			iMsg = 0;
		}
		if(*pData == '\0') { /* guard against \0 characters... */
			*(pMsg + iMsg++) = '\\';
			*(pMsg + iMsg++) = '0';
			++pData;
		} else if(*pData == '\n') { /* record delemiter */
			*(pMsg + iMsg) = '\0'; /* space *is* reserved for this! */
			printline(TCPSessions[iTCPSess].fromHost, pMsg, 1);
			iMsg = 0;
			++pData;
		} else {
			*(pMsg + iMsg++) = *pData++;
		}
	}

	TCPSessions[iTCPSess].iMsg = iMsg; /* persist value */
}

/* CODE FOR SENDING TCP MESSAGES */

/* get send status
 * rgerhards, 2005-10-24
 */
static void TCPSendSetStatus(struct filed *f, enum TCPSendStatus iNewState)
{
	assert(f != NULL);
	assert(f->f_type == F_FORW);
	assert(f->f_un.f_forw.protocol == FORW_TCP);
	assert(   (iNewState == TCP_SEND_NOTCONNECTED)
	       || (iNewState == TCP_SEND_CONNECTING)
	       || (iNewState == TCP_SEND_READY));

	/* there can potentially be a race condition, so guard by mutex */
#	ifdef	USE_PTHREADS
		pthread_mutex_lock(&f->f_un.f_forw.mtxTCPSend);
#	endif
	f->f_un.f_forw.status = iNewState;
#	ifdef	USE_PTHREADS
		pthread_mutex_unlock(&f->f_un.f_forw.mtxTCPSend);
#	endif
}


/* set send status
 * rgerhards, 2005-10-24
 */
static enum TCPSendStatus TCPSendGetStatus(struct filed *f)
{
	enum TCPSendStatus eState;
	assert(f != NULL);
	assert(f->f_type == F_FORW);
	assert(f->f_un.f_forw.protocol == FORW_TCP);

	/* there can potentially be a race condition, so guard by mutex */
#	ifdef	USE_PTHREADS
		pthread_mutex_lock(&f->f_un.f_forw.mtxTCPSend);
#	endif
	eState = f->f_un.f_forw.status;
#	ifdef	USE_PTHREADS
		pthread_mutex_unlock(&f->f_un.f_forw.mtxTCPSend);
#	endif

	return eState;
}


/* Initialize TCP sockets (for sender)
 */
static int TCPSendCreateSocket(struct filed *f)
{
	int fd;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	assert(f != NULL);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		dprintf("couldn't create send socket\n");
		return fd;
	}

	/* We can not allow the TCP sender to block syslogd, at least
	 * not in a single-threaded design. That would cause rsyslogd to
	 * loose input messages - which obviously also would affect
	 * other selector lines, too. So we do set it to non-blocking and 
	 * handle the situation ourselfs (by discarding messages). IF we run
	 * dual-threaded, however, the situation is different: in this case,
	 * the receivers and the selector line processing is only loosely
	 * coupled via a memory buffer. Now, I think, we can afford the extra
	 * wait time. Thus, we enable blocking mode for TCP if we compile with
	 * pthreads.
	 * rgerhards, 2005-10-25
	 */
#	ifndef USE_PTHREADS
	/* set to nonblocking - rgerhards 2005-07-20 */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#	endif

	if(connect(fd, (struct sockaddr*) &(f->f_un.f_forw.f_addr), addrlen) < 0) {
		if(errno == EINPROGRESS) {
			/* this is normal - will complete during select */
			TCPSendSetStatus(f, TCP_SEND_CONNECTING);
		} else {
			dprintf("create tcp connection failed, reason %s", strerror(errno));
			close(fd);
			return -1;
		}
	} else {
		TCPSendSetStatus(f, TCP_SEND_READY);
	}

	return fd;
}


/* Sends a TCP message. It is first checked if the
 * session is open and, if not, it is opened. Then the send
 * is tried. If it fails, one silent re-try is made. If the send
 * fails again, an error status (-1) is returned. If all goes well,
 * 0 is returned. The TCP session is NOT torn down.
 * For now, EAGAIN is ignored (causing message loss) - but it is
 * hard to do something intelligent in this case. With this
 * implementation here, we can not block and/or defer. Things are
 * probably a bit better when we move to liblogging. The alternative
 * would be to enhance the current select server with buffering and
 * write descriptors. This seems not justified, given the expected
 * short life span of this code (and the unlikeliness of this event).
 * rgerhards 2005-07-06
 */
static int TCPSend(struct filed *f, char *msg)
{
	int retry = 0;
	int done = 0;
	size_t len;
	size_t lenSend;
	short f_type;
	char *buf = NULL;	/* if this is non-NULL, it MUST be freed before return! */
	enum TCPSendStatus eState;

	assert(f != NULL);
	assert(msg != NULL);

	len = strlen(msg);

	do { /* try to send message */
		if(f->f_file <= 0) {
			/* we need to open the socket first */
			if((f->f_file = TCPSendCreateSocket(f)) <= 0) {
				return -1;
			}
		}

		eState = TCPSendGetStatus(f); /* cache info */

		if(eState == TCP_SEND_CONNECTING) {
			/* In this case, we save the buffer. If we have a
			 * system with few messages, that hopefully prevents
			 * message loss at all. However, we make no further attempts,
			 * just the first message is saved. So we only try this
			 * if there is not yet a saved message present.
			 * rgerhards 2005-07-20
			 */
			if(f->f_un.f_forw.savedMsg == NULL) {
				f->f_un.f_forw.savedMsg = malloc((len + 1) * sizeof(char));
				if(f->f_un.f_forw.savedMsg == NULL)
					return 0; /* nothing we can do... */
				memcpy(f->f_un.f_forw.savedMsg, msg, len + 1);
			}
			return 0;
		} else if(eState != TCP_SEND_READY)
			/* This here is debatable. For the time being, we
			 * accept the loss of a single message (e.g. during
			 * connection setup in favour of not messing with
			 * wait time and timeouts. The reason is that such
			 * things might otherwise cost us considerable message
			 * loss on the receiving side (even at a timeout set
			 * to just 1 second).  - rgerhards 2005-07-20
			 */
			return 0;

		/* now check if we need to add a line terminator. We need to
		 * copy the string in memory in this case, this is probably
		 * quicker than using writev and definitely quicker than doing
		 * two socket calls.
		 * rgerhards 2005-07-22
		 *
		 * Some messages already contain a \n character at the end
		 * of the message. We append one only if we there is not
		 * already one. This seems the best fit, though this also
		 * means the message does not arrive unaltered at the final
		 * destination. But in the spirit of legacy syslog, this is
		 * probably the best to do...
		 * rgerhards 2005-07-20
		 */
		if((*(msg+len-1) != '\n')) {
			if(buf != NULL)
				free(buf);
			/* in the malloc below, we need to add 2 to the length. The
			 * reason is that we a) add one character and b) len does
			 * not take care of the '\0' byte. Up until today, it was just
			 * +1 , which caused rsyslogd to sometimes dump core.
			 * I have added this comment so that the logic is not accidently
			 * changed again. rgerhards, 2005-10-25
			 */
			if((buf = malloc((len + 2) * sizeof(char))) == NULL) {
				/* extreme mem shortage, try to solve
				 * as good as we can. No point in calling
				 * any alarms, they might as well run out
				 * of memory (the risk is very high, so we
				 * do NOT risk that). If we have a message of
				 * more than 1 byte (what I guess), we simply
				 * overwrite the last character.
				 * rgerhards 2005-07-22
				 */
				if(len > 1) {
					*(msg+len-1) = '\n';
				} else {
					/* we simply can not do anything in
					 * this case (its an error anyhow...).
					 */
				}
			} else {
				/* we got memory, so we can copy the message */
				memcpy(buf, msg, len); /* do not copy '\0' */
				*(buf+len) = '\n';
				*(buf+len+1) = '\0';
				msg = buf; /* use new one */
				++len; /* care for the \n */
			}
		}

		lenSend = send(f->f_file, msg, len, 0);
		dprintf("TCP sent %d bytes, requested %d, msg: '%s'\n", lenSend, len, msg);
		if(lenSend == len) {
			/* all well */
			if(buf != NULL) {
				free(buf);
			}
			return 0;
		} else if(lenSend != -1) {
			/* no real error, could "just" not send everything... 
			 * For the time being, we ignore this...
			 * rgerhards, 2005-10-25
			 */
			dprintf("message not completely (tcp)send, ignoring %d\n", lenSend);
#			if USE_PTHREADS
			usleep(1000); /* experimental - might be benefitial in this situation */
#			endif
			if(buf != NULL)
				free(buf);
			return 0;
		}

		switch(errno) {
		case EMSGSIZE:
			dprintf("message not (tcp)send, too large\n");
			/* This is not a real error, so it is not flagged as one */
			if(buf != NULL)
				free(buf);
			return 0;
			break;
		case EINPROGRESS:
		case EAGAIN:
			dprintf("message not (tcp)send, would block\n");
#			if USE_PTHREADS
			usleep(1000); /* experimental - might be benefitial in this situation */
#			endif
			/* we loose this message, but that's better than loosing
			 * all ;)
			 */
			/* This is not a real error, so it is not flagged as one */
			if(buf != NULL)
				free(buf);
			return 0;
			break;
		default:
			f_type = f->f_type;
			f->f_type = F_UNUSED;
			logerror("message not (tcp)send");
			f->f_type = f_type;
			break;
		}
	
		if(retry == 0) {
			++retry;
			/* try to recover */
			close(f->f_file);
			TCPSendSetStatus(f, TCP_SEND_NOTCONNECTED);
			f->f_file = -1;
		} else {
			if(buf != NULL)
				free(buf);
			return -1;
		}
	} while(!done); /* warning: do ... while() */
	/*NOT REACHED*/

	if(buf != NULL)
		free(buf);
	return -1; /* only to avoid compiler warning! */
}

#endif
/********************************************************************
 *                  ###  END OF SYSLOG/TCP CODE ###
 ********************************************************************/


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

#define FALSE 0
#define TRUE 1
/**
 * Parse a 32 bit integer number from a string.
 *
 * \param ppsz Pointer to the Pointer to the string being parsed. It
 *             must be positioned at the first digit. Will be updated 
 *             so that on return it points to the first character AFTER
 *             the integer parsed.
 * \retval The number parsed.
 */

static int srSLMGParseInt32(unsigned char** ppsz)
{
	int i;

	i = 0;
	while(isdigit(**ppsz))
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
static int srSLMGParseTIMESTAMP3339(struct syslogTime *pTime, unsigned char** ppszTS)
{
	unsigned char *pszTS = *ppszTS;

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
		unsigned char *pszStart = ++pszTS;
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
static int srSLMGParseTIMESTAMP3164(struct syslogTime *pTime, unsigned char* pszTS)
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
static int formatTimestampToMySQL(struct syslogTime *ts, char* pDst, size_t iLenDst)
{
	/* TODO: currently we do not consider localtime/utc */
	assert(ts != NULL);
	assert(pDst != NULL);

	if (iLenDst < 15) /* we need at least 14 bytes
			     14 digits for timestamp + '\n' */
		return(0); 

	return(snprintf(pDst, iLenDst, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", 
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
static int formatTimestamp3339(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
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
static int formatTimestamp3164(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
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
static void getCurrTime(struct syslogTime *t)
{
	struct timeval tp;
	struct tm *tm;
	long lBias;

	assert(t != NULL);
	gettimeofday(&tp, NULL);
	tm = localtime((time_t*) &(tp.tv_sec));

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

/* rgerhards 2004-11-09: the following function is used to 
 * log emergency message when syslogd has no way of using
 * regular meas of processing. It is supposed to be 
 * primarily be called when there is memory shortage. As
 * we now rely on dynamic memory allocation for the messages,
 * we can no longer act correctly when we do not receive
 * memory.
 */
static void syslogdPanic(char* ErrMsg)
{
	/* TODO: provide a meaningful implementation! */
	dprintf("rsyslogdPanic: '%s'\n", ErrMsg);
}

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
static struct msg* MsgConstruct()
{
	struct msg *pM;

	if((pM = calloc(1, sizeof(struct msg))) != NULL)
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
static void MsgDestruct(struct msg * pM)
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
		if(pM->pszSeverity != NULL)
			free(pM->pszSeverity);
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

/* Increment reference count - see description of the "msg"
 * structure for details. As a convenience to developers,
 * this method returns the msg pointer that is passed to it.
 * It is recommended that it is called as follows:
 *
 * pSecondMsgPointer = MsgAddRef(pOrgMsgPointer);
 */
static struct msg *MsgAddRef(struct msg *pM)
{
	assert(pM != NULL);
	pM->iRefCount++;
	/* DEV debugging only! dprintf("MsgAddRef\t0x%x done, Ref now: %d\n", (int)pM, pM->iRefCount);*/
	return(pM);
}

/* Access methods - dumb & easy, not a comment for each ;)
 */
static void setProtocolVersion(struct msg *pM, int iNewVersion)
{
	assert(pM != NULL);
	if(iNewVersion != 0 && iNewVersion != 1) {
		dprintf("Tried to set unsupported protocol version %d - changed to 0.\n", iNewVersion);
		iNewVersion = 0;
	}
	pM->iProtocolVersion = iNewVersion;
}

static int getProtocolVersion(struct msg *pM)
{
	assert(pM != NULL);
	return(pM->iProtocolVersion);
}

/* note: string is taken from constant pool, do NOT free */
static char *getProtocolVersionString(struct msg *pM)
{
	assert(pM != NULL);
	return(pM->iProtocolVersion ? "1" : "0");
}

static int getMSGLen(struct msg *pM)
{
	return((pM == NULL) ? 0 : pM->iLenMSG);
}


static char *getRawMsg(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszRawMsg == NULL)
			return "";
		else
			return pM->pszRawMsg;
}

static char *getUxTradMsg(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszUxTradMsg == NULL)
			return "";
		else
			return pM->pszUxTradMsg;
}

static char *getMSG(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszMSG == NULL)
			return "";
		else
			return pM->pszMSG;
}


static char *getPRI(struct msg *pM)
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
		pM->iLenPRI = snprintf(pM->pszPRI, 5, "%d",
			LOG_MAKEPRI(pM->iFacility, pM->iSeverity));
	}

	return pM->pszPRI;
}


static char *getTimeReported(struct msg *pM, enum tplFormatTypes eFmt)
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

static char *getTimeGenerated(struct msg *pM, enum tplFormatTypes eFmt)
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


char *getSeverity(struct msg *pM)
{
	if(pM == NULL)
		return "";

	if(pM->pszSeverity == NULL) {
		/* we use a 2 byte buffer - can only be one digit */
		if((pM->pszSeverity = malloc(2)) == NULL) return "";
		pM->iLenSeverity =
		   snprintf(pM->pszSeverity, 2, "%d", pM->iSeverity);
	}
	return(pM->pszSeverity);
}

static char *getFacility(struct msg *pM)
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
		   snprintf(pM->pszFacility, 12, "%d", pM->iFacility);
	}
	return(pM->pszFacility);
}


/* rgerhards 2004-11-24: set APP-NAME in msg object
 */
static rsRetVal MsgSetAPPNAME(struct msg *pMsg, char* pszAPPNAME)
{
	assert(pMsg != NULL);
	if(pMsg->pCSAPPNAME == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSAPPNAME = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSAPPNAME, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSAPPNAME, pszAPPNAME);
}


/* This function tries to emulate APPNAME if it is not present. Its
 * main use is when we have received a log record via legacy syslog and
 * now would like to send out the same one via syslog-protocol.
 */
static void tryEmulateAPPNAME(struct msg *pM)
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
static int getAPPNAMELen(struct msg *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	return (pM->pCSAPPNAME == NULL) ? 0 : rsCStrLen(pM->pCSAPPNAME);
}


/* rgerhards, 2005-11-24
 */
static char *getAPPNAME(struct msg *pM)
{
	assert(pM != NULL);
	if(pM->pCSAPPNAME == NULL)
		tryEmulateAPPNAME(pM);
	return (pM->pCSAPPNAME == NULL) ? "" : rsCStrGetSzStrNoNULL(pM->pCSAPPNAME);
}




/* rgerhards 2004-11-24: set PROCID in msg object
 */
static rsRetVal MsgSetPROCID(struct msg *pMsg, char* pszPROCID)
{
	assert(pMsg != NULL);
	if(pMsg->pCSPROCID == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSPROCID = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSPROCID, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSPROCID, pszPROCID);
}

/* rgerhards, 2005-11-24
 */
static int getPROCIDLen(struct msg *pM)
{
	assert(pM != NULL);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	return (pM->pCSPROCID == NULL) ? 1 : rsCStrLen(pM->pCSPROCID);
}


/* rgerhards, 2005-11-24
 */
static char *getPROCID(struct msg *pM)
{
	assert(pM != NULL);
	if(pM->pCSPROCID == NULL)
		aquirePROCIDFromTAG(pM);
	return (pM->pCSPROCID == NULL) ? "-" : rsCStrGetSzStrNoNULL(pM->pCSPROCID);
}


/* rgerhards 2004-11-24: set MSGID in msg object
 */
static rsRetVal MsgSetMSGID(struct msg *pMsg, char* pszMSGID)
{
	assert(pMsg != NULL);
	if(pMsg->pCSMSGID == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSMSGID = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSMSGID, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSMSGID, pszMSGID);
}

/* rgerhards, 2005-11-24
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int getMSGIDLen(struct msg *pM)
{
	return (pM->pCSMSGID == NULL) ? 1 : rsCStrLen(pM->pCSMSGID);
}
#endif


/* rgerhards, 2005-11-24
 */
static char *getMSGID(struct msg *pM)
{
	return (pM->pCSMSGID == NULL) ? "-" : rsCStrGetSzStrNoNULL(pM->pCSMSGID);
}


/* Set the TAG to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetTAG().
 * rgerhards 2004-11-19
 */
static void MsgAssignTAG(struct msg *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	pMsg->iLenTAG = (pBuf == NULL) ? 0 : strlen(pBuf);
	pMsg->pszTAG = pBuf;
}


/* rgerhards 2004-11-16: set TAG in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetTAG(struct msg *pMsg, char* pszTAG)
{
	assert(pMsg != NULL);
	pMsg->iLenTAG = strlen(pszTAG);
	if((pMsg->pszTAG = malloc(pMsg->iLenTAG + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszTAG buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszTAG, pszTAG, pMsg->iLenTAG + 1);

	return(0);
}


/* This function tries to emulate the TAG if none is
 * set. Its primary purpose is to provide an old-style TAG
 * when a syslog-protocol message has been received. Then,
 * the tag is APP-NAME "[" PROCID "]". The function first checks
 * if there is a TAG and, if not, if it can emulate it.
 * rgerhards, 2005-11-24
 */
static void tryEmulateTAG(struct msg *pM)
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
static int getTAGLen(struct msg *pM)
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


static char *getTAG(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else {
		tryEmulateTAG(pM);
		if(pM->pszTAG == NULL)
			return "";
		else
			return pM->pszTAG;
	}
}


static int getHOSTNAMELen(struct msg *pM)
{
	if(pM == NULL)
		return 0;
	else
		if(pM->pszHOSTNAME == NULL)
			return 0;
		else
			return pM->iLenHOSTNAME;
}


static char *getHOSTNAME(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszHOSTNAME == NULL)
			return "";
		else
			return pM->pszHOSTNAME;
}


static char *getRcvFrom(struct msg *pM)
{
	if(pM == NULL)
		return "";
	else
		if(pM->pszRcvFrom == NULL)
			return "";
		else
			return pM->pszRcvFrom;
}
/* rgerhards 2004-11-24: set STRUCTURED DATA in msg object
 */
static rsRetVal MsgSetStructuredData(struct msg *pMsg, char* pszStrucData)
{
	assert(pMsg != NULL);
	if(pMsg->pCSStrucData == NULL) {
		/* we need to obtain the object first */
		if((pMsg->pCSStrucData = rsCStrConstruct()) == NULL) 
			return RS_RET_OBJ_CREATION_FAILED; /* best we can do... */
		rsCStrSetAllocIncrement(pMsg->pCSStrucData, 128);
	}
	/* if we reach this point, we have the object */
	return rsCStrSetSzStr(pMsg->pCSStrucData, pszStrucData);
}

/* get the length of the "STRUCTURED-DATA" sz string
 * rgerhards, 2005-11-24
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int getStructuredDataLen(struct msg *pM)
{
	return (pM->pCSStrucData == NULL) ? 1 : rsCStrLen(pM->pCSStrucData);
}
#endif


/* get the "STRUCTURED-DATA" as sz string
 * rgerhards, 2005-11-24
 */
static char *getStructuredData(struct msg *pM)
{
	return (pM->pCSStrucData == NULL) ? "-" : rsCStrGetSzStrNoNULL(pM->pCSStrucData);
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
static void moveHOSTNAMEtoTAG(struct msg *pM)
{
	assert(pM != NULL);
	pM->pszTAG = pM->pszHOSTNAME;
	pM->iLenTAG = pM->iLenHOSTNAME;
	pM->pszHOSTNAME = NULL;
	pM->iLenHOSTNAME = 0;
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
static rsRetVal aquirePROCIDFromTAG(struct msg *pM)
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
static rsRetVal aquireProgramName(struct msg *pM)
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
		    ; (i < pM->iLenTAG) && isprint(pM->pszTAG[i])
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


/* get the length of the "programname" sz string
 * rgerhards, 2005-10-19
 */
static int getProgramNameLen(struct msg *pM)
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
static char *getProgramName(struct msg *pM)
{
	int iRet;

	assert(pM != NULL);
	if((iRet = aquireProgramName(pM)) != RS_RET_OK) {
		dprintf("error %d returned by aquireProgramName() in getProgramName()\n", iRet);
		return ""; /* best we can do */
	}

	return (pM->pCSProgName == NULL) ? "" : rsCStrGetSzStrNoNULL(pM->pCSProgName);
}


/* rgerhards 2004-11-16: set pszRcvFrom in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetRcvFrom(struct msg *pMsg, char* pszRcvFrom)
{
	assert(pMsg != NULL);
	if(pMsg->pszRcvFrom != NULL)
		free(pMsg->pszRcvFrom);

	pMsg->iLenRcvFrom = strlen(pszRcvFrom);
	if((pMsg->pszRcvFrom = malloc(pMsg->iLenRcvFrom + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszRcvFrom buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszRcvFrom, pszRcvFrom, pMsg->iLenRcvFrom + 1);

	return(0);
}


/* Set the HOSTNAME to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetHOSTNAME().
 * rgerhards 2004-11-19
 */
static void MsgAssignHOSTNAME(struct msg *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	assert(pBuf != NULL);
	pMsg->iLenHOSTNAME = strlen(pBuf);
	pMsg->pszHOSTNAME = pBuf;
}


/* rgerhards 2004-11-09: set HOSTNAME in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetHOSTNAME(struct msg *pMsg, char* pszHOSTNAME)
{
	assert(pMsg != NULL);
	if(pMsg->pszHOSTNAME != NULL)
		free(pMsg->pszHOSTNAME);

	pMsg->iLenHOSTNAME = strlen(pszHOSTNAME);
	if((pMsg->pszHOSTNAME = malloc(pMsg->iLenHOSTNAME + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszHOSTNAME buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszHOSTNAME, pszHOSTNAME, pMsg->iLenHOSTNAME + 1);

	return(0);
}


/* Set the UxTradMsg to a caller-provided string. This is thought
 * to be a heap buffer that the caller will no longer use. This
 * function is a performance optimization over MsgSetUxTradMsg().
 * rgerhards 2004-11-19
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static void MsgAssignUxTradMsg(struct msg *pMsg, char *pBuf)
{
	assert(pMsg != NULL);
	assert(pBuf != NULL);
	pMsg->iLenUxTradMsg = strlen(pBuf);
	pMsg->pszUxTradMsg = pBuf;
}
#endif


/* rgerhards 2004-11-17: set the traditional Unix message in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetUxTradMsg(struct msg *pMsg, char* pszUxTradMsg)
{
	assert(pMsg != NULL);
	assert(pszUxTradMsg != NULL);
	pMsg->iLenUxTradMsg = strlen(pszUxTradMsg);
	if(pMsg->pszUxTradMsg != NULL)
		free(pMsg->pszUxTradMsg);
	if((pMsg->pszUxTradMsg = malloc(pMsg->iLenUxTradMsg + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszUxTradMsg buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszUxTradMsg, pszUxTradMsg, pMsg->iLenUxTradMsg + 1);

	return(0);
}


/* rgerhards 2004-11-09: set MSG in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetMSG(struct msg *pMsg, char* pszMSG)
{
	assert(pMsg != NULL);
	assert(pszMSG != NULL);

	if(pMsg->pszMSG != NULL) {
		free(pMsg->pszMSG);
	}
	pMsg->iLenMSG = strlen(pszMSG);
	if((pMsg->pszMSG = malloc(pMsg->iLenMSG + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszMSG buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszMSG, pszMSG, pMsg->iLenMSG + 1);

	return(0);
}

/* rgerhards 2004-11-11: set RawMsg in msg object
 * returns 0 if OK, other value if not. In case of failure,
 * logs error message and destroys msg object.
 */
static int MsgSetRawMsg(struct msg *pMsg, char* pszRawMsg)
{
	assert(pMsg != NULL);
	if(pMsg->pszRawMsg != NULL) {
		free(pMsg->pszRawMsg);
	}
	pMsg->iLenRawMsg = strlen(pszRawMsg);
	if((pMsg->pszRawMsg = malloc(pMsg->iLenRawMsg + 1)) == NULL) {
		syslogdPanic("Could not allocate memory for pszRawMsg buffer.");
		MsgDestruct(pMsg);
		return(-1);
	}
	memcpy(pMsg->pszRawMsg, pszRawMsg, pMsg->iLenRawMsg + 1);

	return(0);
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
static char *MsgGetProp(struct msg *pMsg, struct templateEntry *pTpe,
                 rsCStrObj *pCSPropName, unsigned short *pbMustBeFreed)
{
	char *pName;
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
		pName = rsCStrGetSzStr(pCSPropName);
	}
	*pbMustBeFreed = 0;

	/* sometimes there are aliases to the original MonitoWare
	 * property names. These come after || in the ifs below. */
	if(!strcmp(pName, "msg")) {
		pRes = getMSG(pMsg);
	} else if(!strcmp(pName, "rawmsg")) {
		pRes = getRawMsg(pMsg);
	} else if(!strcmp(pName, "UxTradMsg")) {
		pRes = getUxTradMsg(pMsg);
	} else if(!strcmp(pName, "FROMHOST")) {
		pRes = getRcvFrom(pMsg);
	} else if(!strcmp(pName, "source")
		  || !strcmp(pName, "HOSTNAME")) {
		pRes = getHOSTNAME(pMsg);
	} else if(!strcmp(pName, "syslogtag")) {
		pRes = getTAG(pMsg);
	} else if(!strcmp(pName, "PRI")) {
		pRes = getPRI(pMsg);
	} else if(!strcmp(pName, "iut")) {
		pRes = "1"; /* always 1 for syslog messages (a MonitorWare thing;)) */
	} else if(!strcmp(pName, "syslogfacility")) {
		pRes = getFacility(pMsg);
	} else if(!strcmp(pName, "syslogpriority")) {
		pRes = getSeverity(pMsg);
	} else if(!strcmp(pName, "timegenerated")) {
		pRes = getTimeGenerated(pMsg, pTpe->data.field.eDateFormat);
	} else if(!strcmp(pName, "timereported")
		  || !strcmp(pName, "TIMESTAMP")) {
		pRes = getTimeReported(pMsg, pTpe->data.field.eDateFormat);
	} else if(!strcmp(pName, "programname")) {
		pRes = getProgramName(pMsg);
// TODO DOC: document new properties
	} else if(!strcmp(pName, "PROTOCOL-VERSION")) {
		pRes = getProtocolVersionString(pMsg);
	} else if(!strcmp(pName, "STRUCTURED-DATA")) {
		pRes = getStructuredData(pMsg);
	} else if(!strcmp(pName, "APP-NAME")) {
		pRes = getAPPNAME(pMsg);
	} else if(!strcmp(pName, "PROCID")) {
		pRes = getPROCID(pMsg);
	} else if(!strcmp(pName, "MSGID")) {
		pRes = getMSGID(pMsg);
	} else {
		pRes = "**INVALID PROPERTY NAME**";
	}

	/* If we did not receive a template pointer, we are already done... */
	if(pTpe == NULL) {
		*pbMustBeFreed = 0;
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
		int iCurrFld;
		char *pFld;
		char *pFldEnd;
		/* first, skip to the field in question. The field separator
		 * is always one character and is stored in the template entry.
		 */
		iCurrFld = 1;
		pFld = pRes;
		while(*pFld && iCurrFld < pTpe->data.field.iToPos) {
			/* skip fields until the requested field or end of string is found */
			while(*pFld && (unsigned char) *pFld != pTpe->data.field.field_delim)
				++pFld; /* skip to field terminator */
			if(*pFld == pTpe->data.field.field_delim) {
				++pFld; /* eat it */
				++iCurrFld;
			}
		}
		dprintf("field requested %d, field found %d\n", pTpe->data.field.iToPos, iCurrFld);
		
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
		if(iFrom) {
		/* skip to the start of the substring (can't do pointer arithmetic
		 * because the whole string might be smaller!!)
		 */
		//	++iFrom; /* nbr of chars to skip! */
			while(*pRes && iFrom) {
				--iFrom;
				++pRes;
			}
		}
		/* OK, we are at the begin - now let's copy... */
		while(*pRes && iLen) {
			*pBuf++ = *pRes;
			++pRes;
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

			dprintf("debug: String to match for regex is: %s\n",
			        pRes);

			if (0 != regexec(&pTpe->data.field.re, pRes, nmatch,
				    pmatch, 0)) {
				/* we got no match! */
				return "**NO MATCH**";
			} else {
				/* Match! */
				/* I need to malloc pBuf */
				int iLen;
				char *pBuf;

				iLen = pmatch[1].rm_eo - pmatch[1].rm_so;
				pBuf = (char *) malloc((iLen + 1) * sizeof(char));

				if (pBuf == NULL) {
					if (*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return
					    "**OUT OF MEMORY ALLOCATING pBuf**";
				}
				*pBuf = '\0';

				/* Lets copy the matched substring to the buffer */
				/* TODO: RGer: I think we can use memcpy() here, too (faster) */
				strncpy(pBuf, pRes + pmatch[1].rm_so,
					iLen);
				pBuf[iLen] = '\0';	/* Null termination of string */

				if (*pbMustBeFreed == 1)
					free(pRes);
				pRes = pBuf;
				*pbMustBeFreed = 1;
			}
		}
#endif /* #ifdef FEATURE_REGEXP */
	}

	/* case conversations (should go after substring, because so we are able to
	 * work on the smallest possible buffer).
	 */
	if(pTpe->data.field.eCaseConv != tplCaseConvNo) {
		/* we need to obtain a private copy */
		int iLen = strlen(pRes);
		char *pBufStart;
		char *pBuf;
		pBufStart = pBuf = malloc((iLen + 1) * sizeof(char));
		if(pBuf == NULL) {
			if(*pbMustBeFreed == 1)
				free(pRes);
			*pbMustBeFreed = 0;
			return "**OUT OF MEMORY**";
		}
		while(*pRes) {
			*pBuf++ = (pTpe->data.field.eCaseConv == tplCaseConvUpper) ?
			          toupper(*pRes) : tolower(*pRes);
				  /* currently only these two exist */
			++pRes;
		}
		*pBuf = '\0';
		if(*pbMustBeFreed == 1)
			free(pRes);
		pRes = pBufStart;
		*pbMustBeFreed = 1;
	}

	/* now do control character dropping/escaping/replacement
	 * RGerhards, 2006-11-17
	 */
	if(pTpe->data.field.options.bEscapeCC) {
		/* we must first count how many control charactes are
		 * present, because we need this to compute the new string
		 * buffer length. While doing so, we also compute the string
		 * length.
		 */
		int iNumCC = 0;
		int iLen = 0;
		char *pBuf;

		for(pBuf = pRes ; *pBuf ; ++pBuf) {
			++iLen;
			if(iscntrl(*pBuf))
				++iNumCC;
		}

		if(iNumCC > 0) { /* if 0, there is nothing to escape, so we are done */
			/* OK, let's do the escaping... */
			char *pBufStart;
			char szCCEsc[8]; /* buffer for escape sequence */

			iLen += iNumCC * 4;
			pBufStart = pBuf = malloc((iLen + 1) * sizeof(char));
			if(pBuf == NULL) {
				if(*pbMustBeFreed == 1)
					free(pRes);
				*pbMustBeFreed = 0;
				return "**OUT OF MEMORY**";
			}
			while(*pRes) {
				if(iscntrl(*pRes)) {
					/* TODO: fill escape coding */
				} else {
					*pBuf++ = *pRes;
				}
				++pRes;
			}
			*pBuf = '\0';
			if(*pbMustBeFreed == 1)
				free(pRes);
			pRes = pBufStart;
			*pbMustBeFreed = 1;
		}
	}

	/* Now drop last LF if present (pls note that this must not be done
	 * if bEscapeCC was set! - once that is implemented ;)).
	 */
	if(pTpe->data.field.options.bDropLastLF && !pTpe->data.field.options.bEscapeCC) {
		int iLen = strlen(pRes);
		char *pBuf;
		if(*(pRes + iLen - 1) == '\n') {
			/* we have a LF! */
			/* check if we need to obtain a private copy */
			if(pbMustBeFreed == 0) {
				/* ok, original copy, need a private one */
				pBuf = malloc((iLen + 1) * sizeof(char));
				if(pBuf == NULL) {
					if(*pbMustBeFreed == 1)
						free(pRes);
					*pbMustBeFreed = 0;
					return "**OUT OF MEMORY**";
				}
				memcpy(pBuf, pRes, iLen - 1);
				pRes = pBuf;
				*pbMustBeFreed = 1;
			}
			*(pRes + iLen - 1) = '\0'; /* drop LF ;) */
		}
	}

	/*dprintf("MsgGetProp(\"%s\"): \"%s\"\n", pName, pRes); only for verbose debug logging */
	return(pRes);
}

/* rgerhards 2004-11-09: end of helper routines. On to the 
 * "real" code ;)
 */


static int usage(void)
{
	fprintf(stderr, "usage: rsyslogd [-dhvw] [-l hostlist] [-m markinterval] [-n] [-p path]\n" \
		" [-s domainlist] [-r port] [-t port] [-f conffile]\n");
	exit(1); /* "good" exit - done to terminate usage() */
}

#ifdef SYSLOG_UNIXAF
static int create_unix_socket(const char *path)
{
	struct sockaddr_un sunx;
	int fd;
	char line[MAXLINE +1];

	if (path[0] == '\0')
		return -1;

	(void) unlink(path);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void) strncpy(sunx.sun_path, path, sizeof(sunx.sun_path));
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0 || bind(fd, (struct sockaddr *) &sunx,
			   SUN_LEN(&sunx)) < 0 ||
	    chmod(path, 0666) < 0) {
		snprintf(line, sizeof(line), "cannot create %s", path);
		logerror(line);
		dprintf("cannot create %s (%d).\n", path, errno);
		close(fd);
		return -1;
	}
	return fd;
}
#endif

#ifdef SYSLOG_INET
static int create_udp_socket()
{
	int fd, on = 1;
	struct sockaddr_in sin;
	int sockflags;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		logerror("syslog: Unknown protocol, suspending inet service.");
		return fd;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(LogPort);
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, \
		       (char *) &on, sizeof(on)) < 0 ) {
		logerror("setsockopt(REUSEADDR), suspending inet");
		close(fd);
		return -1;
	}
	/* We need to enable BSD compatibility. Otherwise an attacker
	 * could flood our log files by sending us tons of ICMP errors.
	 */
#ifndef BSD	
	if (should_use_so_bsdcompat()) {
		if (setsockopt(fd, SOL_SOCKET, SO_BSDCOMPAT, \
				(char *) &on, sizeof(on)) < 0) {
			logerror("setsockopt(BSDCOMPAT), suspending inet");
			close(fd);
			return -1;
		}
	}
#endif
	/* We must not block on the network socket, in case a packet
	 * gets lost between select and recv, otherwise the process
	 * will stall until the timeout, and other processes trying to
	 * log will also stall.
	 * Patch vom Colin Phipps <cph@cph.demon.co.uk> to the original
	 * sysklogd source. Applied to rsyslogd on 2005-10-19.
	 */
	if ((sockflags = fcntl(fd, F_GETFL)) != -1) {
		sockflags |= O_NONBLOCK;
		/*
		 * SETFL could fail too, so get it caught by the subsequent
		 * error check.
		 */
		sockflags = fcntl(fd, F_SETFL, sockflags);
	}
	if (sockflags == -1) {
		logerror("fcntl(O_NONBLOCK), suspending inet");
		close(fd);
		return -1;
	}

	if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		logerror("bind, suspending inet");
		close(fd);
		return -1;
	}
	return fd;
}
#endif

/* rgerhards, 2005-10-24: crunch_list is called only during option processing. So
 * it is never called once rsyslogd is running (not even when HUPed). This code
 * contains some exist, but they are considered safe because they only happen
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
		dprintf ("#%d: %s\n", count, StripDomains[count++]);
#endif
	return result;
}


static void untty()
#ifdef SYSV
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


/*
 * Parse the line to make sure that the msg is not a composite of more
 * than one message.
 * This function is called from ALL receivers, no matter how the message
 * was received. rgerhards
 * Partial messages are reconstruced via the parts[] table. This table is
 * dynamically allocated on startup based on getdtablesize(). This design has
 * its advantages, however it typically allocates way too many table
 * entries. If we've nothing better to do, we might want to look into this.
 * rgerhards 2004-11-08
 * I added the "iSource" parameter. This is needed to distinguish between
 * messages that have a hostname in them (received from the internet) and
 * those that do not have (most prominently /dev/log).  rgerhards 2004-11-16
 * And now I removed the "iSource" parameter and changed it to be "bParseHost",
 * because all that it actually controls is whether the host is parsed or not.
 * For rfc3195 support, we needed to modify the algo for host parsing, so we can
 * no longer rely just on the source (rfc3195d forwarded messages arrive via
 * unix domain sockets but contain the hostname). rgerhards, 2005-10-06
 */
static void printchopped(char *hname, char *msg, int len, int fd, int bParseHost)
{
	auto int ptlngth;
	auto char *start = msg,
		  *p,
	          *end,
		  tmpline[MAXLINE + 1];

	dprintf("Message length: %d, File descriptor: %d.\n", len, fd);
	tmpline[0] = '\0';
	if(parts[fd] != NULL) {
		dprintf("Including part from messages.\n");
		strcpy(tmpline, parts[fd]);
		free(parts[fd]);
		parts[fd] = (char *) 0;
		if ( (strlen(msg) + strlen(tmpline)) > MAXLINE ) {
			logerror("Cannot glue message parts together");
			printline(hname, tmpline, bParseHost);
			start = msg;
		} else {
			dprintf("Previous: %s\n", tmpline);
			dprintf("Next: %s\n", msg);
			strcat(tmpline, msg);	/* length checked above */
			printline(hname, tmpline, bParseHost);
			if ( (strlen(msg) + 1) == len )
				return;
			else
				start = strchr(msg, '\0') + 1;
		}
	}

	if ( msg[len-1] != '\0' ) {
		msg[len] = '\0';
		for(p= msg+len-1; *p != '\0' && p > msg; )
			--p;
		if(*p == '\0') p++;
		ptlngth = strlen(p);
		if ( (parts[fd] = malloc(ptlngth + 1)) == (char *) 0 )
			logerror("Cannot allocate memory for message part.");
		else
		{
			strcpy(parts[fd], p);
			dprintf("Saving partial msg: %s\n", parts[fd]);
			memset(p, '\0', ptlngth);
		}
	}

	do {
		end = strchr(start + 1, '\0');
		printline(hname, start, bParseHost);
		start = end + 1;
	} while ( *start != '\0' );

	return;
}

/* Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 * rgerhards 2004-11-08: Please note
 * that this function does only a partial decoding. At best, it splits 
 * the PRI part. No further decode happens. The rest is done in 
 * logmsg(). Please note that printsys() calls logmsg() directly, so
 * this is something we need to restructure once we are moving the
 * real decoder in here. I now (2004-11-09) found that printsys() seems
 * not to be called from anywhere. So we might as well decode the full
 * message here.
 * Added the iSource parameter so that we know if we have to parse
 * HOSTNAME or not. rgerhards 2004-11-16.
 * changed parameter iSource to bParseHost. For details, see comment in
 * printchopped(). rgerhards 2005-10-06
 */
void printline(char *hname, char *msg, int bParseHost)
{
	register char *p;
	int pri;
	struct msg *pMsg;

	/* Now it is time to create the message object (rgerhards)
	*/
	if((pMsg = MsgConstruct()) == NULL){
		/* rgerhards 2004-11-09: calling panic might not be the
		 * brightest idea - however, it is the best I currently have
		 * (TODO: think a bit more about this).
		 */
		syslogdPanic("Could not construct Msg object.");
		return;
	}
	if(MsgSetRawMsg(pMsg, msg) != 0) return;
	
	pMsg->bParseHOSTNAME  = bParseHost;
	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
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

	/* got the buffer, now copy over the message. We use the "old" code
	 * here, it doesn't make sense to optimize as that code will soon
	 * be replaced.
	 */
#if 0	 /* TODO: REMOVE THIS LATER */
	/* we soon need to support UTF-8, so we will soon need to remove
	 * this. As a side-note, the current code destroys MBCS messages
	 * (like Japanese).
	 */
	q = pMsg->pszMSG;
	pEnd = pMsg->pszMSG + pMsg->iLenMSG;	 /* was -4 */
	while ((c = *p++) && q < pEnd) {
		if (c == '\n')
			*q++ = ' ';
/* not yet!		else if (c < 040) {
			*q++ = '^';
			*q++ = c ^ 0100;
		} else if (c == 0177 || (c & 0177) < 040) {
			*q++ = '\\';
			*q++ = '0' + ((c & 0300) >> 6);
			*q++ = '0' + ((c & 0070) >> 3);
			*q++ = '0' + (c & 0007);
		}*/ else
			*q++ = c;
	}
	*q = '\0';
#endif

	/* Now we look at the HOSTNAME. That is a bit complicated...
	 * If we have a locally received message, it does NOT
	 * contain any hostname information in the message itself.
	 * As such, the HOSTNAME is the same as the system that
	 * the message was received from (that, for obvious reasons,
	 * being the local host).  rgerhards 2004-11-16
	 */
	if(bParseHost == 0)
		if(MsgSetHOSTNAME(pMsg, hname) != 0) return;
	if(MsgSetRcvFrom(pMsg, hname) != 0) return;

	/* rgerhards 2004-11-19: well, well... we've now seen that we
	 * have the "hostname problem" also with the traditional Unix
	 * message. As we like to emulate it, we need to add the hostname
	 * to it.
	 */
	if(MsgSetUxTradMsg(pMsg, p) != 0) return;

	logmsg(pri, pMsg, SYNC_FILE);

	/* rgerhards 2004-11-11:
	 * we are done with the message object. If it still is
	 * stored somewhere, we can call discard anyhow. This
	 * is handled via the reference count - see description
	 * of struct msg for details.
	 */
	MsgDestruct(pMsg);
	return;
}

/* Decode a priority into textual information like auth.emerg.
 */
char *textpri(pri)
	int pri;
{
	static char res[20];
	CODE *c_pri, *c_fac;

	for (c_fac = facilitynames; c_fac->c_name && !(c_fac->c_val == LOG_FAC(pri)<<3); c_fac++);
	for (c_pri = prioritynames; c_pri->c_name && !(c_pri->c_val == LOG_PRI(pri)); c_pri++);

	snprintf (res, sizeof(res), "%s.%s<%d>", c_fac->c_name, c_pri->c_name, pri);

	return res;
}

time_t	now;

/* rgerhards 2004-11-09: the following is a function that can be used
 * to log a message orginating from the syslogd itself. In sysklogd code,
 * this is done by simply calling logmsg(). However, logmsg() is changed in
 * rsyslog so that it takes a msg "object". So it can no longer be called
 * directly. This method here solves the need. It provides an interface that
 * allows to construct a locally-generated message. Please note that this
 * function here probably is only an interim solution and that we need to
 * think on the best way to do this.
 */
static void logmsgInternal(int pri, char * msg, char* from, int flags)
{
	struct msg *pMsg;

	if((pMsg = MsgConstruct()) == NULL){
		/* rgerhards 2004-11-09: calling panic might not be the
		 * brightest idea - however, it is the best I currently have
		 * (TODO: think a bit more about this).
		 */
		syslogdPanic("Could not construct Msg object.");
		return;
	}

	if(MsgSetUxTradMsg(pMsg, msg) != 0) return;
	if(MsgSetRawMsg(pMsg, msg) != 0) return;
	if(MsgSetHOSTNAME(pMsg, LocalHostName) != 0) return;
	if(MsgSetTAG(pMsg, "rsyslogd:") != 0) return;
	pMsg->iFacility = LOG_FAC(pri);
	pMsg->iSeverity = LOG_PRI(pri);
	pMsg->bParseHOSTNAME = 0;
	getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */

	logmsg(pri, pMsg, flags);
	MsgDestruct(pMsg);
}

/*
 * This functions looks at the given message and checks if it matches the
 * provided filter condition. If so, it returns true, else it returns
 * false. This is a helper to logmsg() and meant to drive the decision
 * process if a message is to be processed or not. As I expect this
 * decision code to grow more complex over time AND logmsg() is already
 * a very lengthe function, I thought a separate function is more appropriate.
 * 2005-09-19 rgerhards
 */
int shouldProcessThisMessage(struct filed *f, struct msg *pMsg)
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
		if(rsCStrSzStrCmp(f->pCSHostnameComp, getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dprintf("hostname filter '+%s' does not match '%s'\n", 
				rsCStrGetSzStr(f->pCSHostnameComp), getHOSTNAME(pMsg));
			return 0;
		}
	} else { /* must be -hostname */
		if(!rsCStrSzStrCmp(f->pCSHostnameComp, getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dprintf("hostname filter '-%s' does not match '%s'\n", 
				rsCStrGetSzStr(f->pCSHostnameComp), getHOSTNAME(pMsg));
			return 0;
		}
	}
	
	if(f->pCSProgNameComp != NULL) {
		if(rsCStrSzStrCmp(f->pCSProgNameComp, getProgramName(pMsg), getProgramNameLen(pMsg))) {
			/* not equal, so we are already done... */
			dprintf("programname filter '%s' does not match '%s'\n", 
				rsCStrGetSzStr(f->pCSProgNameComp), getProgramName(pMsg));
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
		pszPropVal = MsgGetProp(pMsg, NULL,
			        f->f_filterData.prop.pCSPropName, &pbMustBeFreed);

		/* Now do the compares (short list currently ;)) */
		switch(f->f_filterData.prop.operation ) {
		case FIOP_CONTAINS:
			if(rsCStrLocateInSzStr(f->f_filterData.prop.pCSCompValue, pszPropVal) != -1)
				iRet = 1;
			break;
		case FIOP_ISEQUAL:
			if(rsCStrSzStrCmp(f->f_filterData.prop.pCSCompValue,
					  pszPropVal, strlen(pszPropVal)) == 0)
				iRet = 1; /* process message! */
			break;
		case FIOP_STARTSWITH:
			if(rsCStrSzStrStartsWithCStr(f->f_filterData.prop.pCSCompValue,
					  pszPropVal, strlen(pszPropVal)) == 0)
				iRet = 1; /* process message! */
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

		/* cleanup */
		if(pbMustBeFreed)
			free(pszPropVal);
		
		if(Debug) {
			char *pszPropVal;
			unsigned short pbMustBeFreed;
			pszPropVal = MsgGetProp(pMsg, NULL,
					f->f_filterData.prop.pCSPropName, &pbMustBeFreed);
			printf("Filter: check for property '%s' (value '%s') ",
			        rsCStrGetSzStr(f->f_filterData.prop.pCSPropName),
			        pszPropVal);
			if(f->f_filterData.prop.isNegated)
				printf("NOT ");
			printf("%s '%s': %s\n",
			       getFIOPName(f->f_filterData.prop.operation),
			       rsCStrGetSzStr(f->f_filterData.prop.pCSCompValue),
			       iRet ? "TRUE" : "FALSE");
			if(pbMustBeFreed)
				free(pszPropVal);
		}
	}

	return(iRet);
}


/* Process (consume) a received message. Calls the actions configured.
 * Can some time later run in its own thread. To aid this, the calling
 * parameters should be reduced to just pMsg.
 * See comment dated 2005-10-13 in logmsg() on multithreading.
 * rgerhards, 2005-10-13
 */
static void processMsg(struct msg *pMsg)
{
	struct filed *f;

	assert(pMsg != NULL);

	/* log the message to the particular outputs */
	if (!Initialized) {
		/* If we reach this point, the daemon initialization FAILED. That is,
		 * syslogd is NOT actually running. So what we do here is just
		 * initialize a pointer to the system console and then output
		 * the message to the it. So at least we have a little
		 * chance that messages show up somewhere.
		 * rgerhards 2004-11-09
		 */
		f = &consfile;
		f->f_file = open(ctty, O_WRONLY|O_NOCTTY);

		if (f->f_file >= 0) {
			untty();
			f->f_pMsg = MsgAddRef(pMsg); /* is expected here... */
			fprintlog(f);
			MsgDestruct(pMsg);
			(void) close(f->f_file);
			f->f_file = -1;
		}

		/* now log to a second emergency log... 2005-06-21 rgerhards */
		/* TODO: make this configurable, eventually via the command line */
		if(ttyname(0) != NULL) {
			memset(&emergfile, 0, sizeof(emergfile));
			f = &emergfile;
			emergfile.f_type = F_TTY;
			(void) strcpy(emergfile.f_un.f_fname, ttyname(0));
			cflineSetTemplateAndIOV(&emergfile, " TradFmt");
			f->f_file = open(ttyname(0), O_WRONLY|O_NOCTTY);

			if (f->f_file >= 0) {
				untty();
				f->f_pMsg = MsgAddRef(pMsg); /* is expected here... */
				fprintlog(f);
				MsgDestruct(pMsg);
				(void) close(f->f_file);
				f->f_file = -1;
			}
		}
		return; /* we are done with emergency loging */
	}

	for (f = Files; f != NULL ; f = f->f_next) {
		/* first, we need to check if this is a disabled (F_UNUSED)
		 * entry. If so, we must not further process it, as the data
		 * structure probably contains invalid pointers and other
		 * such mess.
		 * rgerhards 2005-09-26
		 */
		if(f->f_type == F_UNUSED)
			continue; /* on to next */

		/* This is actually the "filter logic". Looks like we need
		 * to improve it a little for complex selector line conditions. We
		 * won't do that for now, but at least we now know where
		 * to look at.
		 * 2005-09-09 rgerhards
		 * ok, we are now ready to move to something more advanced. Because
		 * of this, I am moving the actual decision code to outside this function.
		 * 2005-09-19 rgerhards
		 */
		if(!shouldProcessThisMessage(f, pMsg)) {
			continue;
		}

		/* We now need to check a special case - F_DISCARD. If that
		 * action is specified in the selector line, no futher processing
		 * must be done. Thus, we stop the for-loop.
		 * 2005-09-09 rgerhards
		 */
		if(f->f_type == F_DISCARD) {
			dprintf("Discarding message based on selector config\n");
			break; /* that's it for this message ;) */
			}

		/* don't output marks to recently written files */
		if ((pMsg->msgFlags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/* suppress duplicate lines to this file
		 */
		if ((pMsg->msgFlags & MARK) == 0 && getMSGLen(pMsg) == getMSGLen(f->f_pMsg) &&
		    !strcmp(getMSG(pMsg), getMSG(f->f_pMsg)) &&
		    !strcmp(getHOSTNAME(pMsg), getHOSTNAME(f->f_pMsg))) {
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d.\n",
			    f->f_prevcount, now - f->f_time,
			    repeatinterval[f->f_repeatcount]);
			/* If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			/* first check if we have a previous message stored
			 * if so, discard that first
			 */
			if(f->f_pMsg != NULL)
				MsgDestruct(f->f_pMsg);
			f->f_pMsg = MsgAddRef(pMsg);
			/* call the output driver */
			fprintlog(f);
		}
	}
}


#ifdef	USE_PTHREADS
/* This block contains code that is only present when USE_PTHREADS is
 * enabled. I plan to move it to some other file, but for the time
 * being, I include it here because that saves me from the need to
 * do so many external definitons.
 * rgerhards, 2005-10-24
 */

/* shuts down the worker process. The worker will first finish
 * with the message queue. Control returns, when done.
 * This function is intended to be called during syslogd shutdown
 * AND restat (init()!).
 * rgerhards, 2005-10-25
 */
static void stopWorker(void)
{
	if(bRunningMultithreaded) {
		/* we could run single-threaded if there was an error
		 * during startup. Then, we obviously do not need to
		 * do anything to stop the worker ;)
		 */
		dprintf("Initiating worker thread shutdown sequence...\n");
		/* We are now done with all messages, so we need to wake up the
		 * worker thread and then wait for it to finish.
		 */
		bGlblDone = 1;
		/* It's actually not "not empty" below but awaking the worker. The worker
		 * then finds out that it shall terminate and does so.
		 */
		pthread_cond_signal(pMsgQueue->notEmpty);
		pthread_join(thrdWorker, NULL);
		bRunningMultithreaded = 0;
		dprintf("Worker thread terminated.\n");
	}
}


/* starts the worker thread. It must be made sure that the queue is
 * already existing and the worker is NOT already running.
 * rgerhards 2005-10-25
 */
static void startWorker(void)
{
	int i;
	if(pMsgQueue != NULL) {
		bGlblDone = 0; /* we are NOT done (else worker would immediately terminate) */
		i = pthread_create(&thrdWorker, NULL, singleWorker, NULL);
		dprintf("Worker thread started with state %d.\n", i);
		bRunningMultithreaded = 1;
	} else {
		dprintf("message queue not existing, remaining single-threaded.\n");
	}
}


static msgQueue *queueInit (void)
{
	msgQueue *q;

	q = (msgQueue *)malloc (sizeof (msgQueue));
	if (q == NULL) return (NULL);

	q->empty = 1;
	q->full = 0;
	q->head = 0;
	q->tail = 0;
	q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (q->mut, NULL);
	q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notFull, NULL);
	q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notEmpty, NULL);
	
	return (q);
}

static void queueDelete (msgQueue *q)
{
	pthread_mutex_destroy (q->mut);
	free (q->mut);	
	pthread_cond_destroy (q->notFull);
	free (q->notFull);
	pthread_cond_destroy (q->notEmpty);
	free (q->notEmpty);
	free (q);
}

static void queueAdd (msgQueue *q, void* in)
{
	q->buf[q->tail] = in;
	q->tail++;
	if (q->tail == QUEUESIZE)
		q->tail = 0;
	if (q->tail == q->head)
		q->full = 1;
	q->empty = 0;

	return;
}

static void queueDel (msgQueue *q, struct msg **out)
{
	*out = (struct msg*) q->buf[q->head];

	q->head++;
	if (q->head == QUEUESIZE)
		q->head = 0;
	if (q->head == q->tail)
		q->empty = 1;
	q->full = 0;

	return;
}


/* The worker thread (so far, we have dual-threading, so only one
 * worker thread. Having more than one worker requires considerable
 * additional code review in regard to thread-safety.
 */
static void *singleWorker(void *vParam)
{
	msgQueue *fifo = pMsgQueue;
	struct msg *pMsg;

	assert(fifo != NULL);

	while(!bGlblDone || !fifo->empty) {
		pthread_mutex_lock(fifo->mut);
		while (fifo->empty && !bGlblDone) {
			dprintf ("singleWorker: queue EMPTY, waiting for next message.\n");
			pthread_cond_wait (fifo->notEmpty, fifo->mut);
		}
		if(!fifo->empty) {
			/* dequeue element (still protected from mutex) */
			queueDel(fifo, &pMsg);
			assert(pMsg != NULL);
			pthread_mutex_unlock(fifo->mut);
			pthread_cond_signal (fifo->notFull);
			/* do actual processing (the lengthy part, runs in parallel) */
			dprintf("Lone worker is running...\n");
			processMsg(pMsg);
			MsgDestruct(pMsg);
			/* If you need a delay for testing, here do a */
			/* sleep(1); */
		} else { /* the mutex must be unlocked in any case (important for termination) */
			pthread_mutex_unlock(fifo->mut);
		}
		if(debugging_on && bGlblDone && !fifo->empty)
			dprintf("Worker does not yet terminate because it still has messages to process.\n");
	}

	dprintf("Worker thread terminates\n");
	pthread_exit(0);
}

/* END threads-related code */
#endif /* #ifdef USE_PTHREADS */


/* This method enqueues a message into the the message buffer. It also
 * the worker thread, so that the message will be processed. If we are
 * compiled without PTHREADS support, we simply use this method as
 * an alias for processMsg().
 * See comment dated 2005-10-13 in logmsg() on multithreading.
 * rgerhards, 2005-10-24
 */
#ifndef	USE_PTHREADS
#define enqueueMsg(x) processMsg((x))
#else
static void enqueueMsg(struct msg *pMsg)
{
	int iRet;
	msgQueue *fifo = pMsgQueue;

	assert(pMsg != NULL);

	if(bRunningMultithreaded == 0) {
		/* multi-threading is not yet initialized, happens e.g.
		 * during startup and restart. rgerhards, 2005-10-25
		 */
		 dprintf("enqueueMsg: not yet running on multiple threads\n");
		 processMsg(pMsg);
	} else {
		/* "normal" mode, threading initialized */
		iRet = pthread_mutex_lock(fifo->mut);
		while (fifo->full) {
			dprintf ("enqueueMsg: queue FULL.\n");
			pthread_cond_wait (fifo->notFull, fifo->mut);
		}
		queueAdd(fifo, MsgAddRef(pMsg));
		/* now activate the worker thread */
		pthread_mutex_unlock(fifo->mut);
		iRet = pthread_cond_signal(fifo->notEmpty);
		dprintf("EnqueueMsg signaled condition (%d)\n", iRet);
	}
}
#endif /* #ifndef USE_PTHREADS */


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
static int parseRFCSyslogMsg(struct msg *pMsg, int flags)
{
	char *p2parse;
	char *pBuf;
	int bContParse = 1;

	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	p2parse = pMsg->pszUxTradMsg;

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
	if(srSLMGParseTIMESTAMP3339(&(pMsg->tTIMESTAMP), (unsigned char **) &p2parse) == FALSE) {
		dprintf("no TIMESTAMP detected!\n");
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
	if(MsgSetMSG(pMsg, p2parse) != 0) return 1;

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
static int parseLegacySyslogMsg(struct msg *pMsg, int flags)
{
	char *p2parse;
	char *pBuf;
	char *pWork;
	rsCStrObj *pStrB;
	int iCnt;
	int bTAGCharDetected;

	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	p2parse = pMsg->pszUxTradMsg;

	/*
	 * Check to see if msg contains a timestamp
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
	(void) time(&now);
	if (flags & ADDDATE) {
		getCurrTime(&(pMsg->tTIMESTAMP)); /* use the current time! */
	}

	/* rgerhards, 2006-03-13: next, we parse the hostname and tag. But we 
	 * do this only when the user has not forbidden this. I now introduce some
	 * code that allows a user to configure rsyslogd to treat the rest of the
	 * message as MSG part completely. In this case, the hostname will be the
	 * machine that we received the message from and the tag will be empty. This
	 * is meant to be an interim solution, but for now it is in the code.
	 */

	if(bParseHOSTNAMEandTAG) {
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
			if((pBuf = malloc(sizeof(char)* strlen(p2parse) +1)) == NULL)
				return 1;
			pWork = pBuf;
			/* this is the actual parsing loop */
			while(*p2parse && *p2parse != ' ' && *p2parse != ':') {
				if(   *p2parse == '[' || *p2parse == ']' || *p2parse == '/')
					bTAGCharDetected = 1;
				*pWork++ = *p2parse++;
			}
			/* we need to handle ':' seperately, because it terminates the
			 * TAG - so we also need to terminate the parser here!
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
			dprintf("HOSTNAME contains invalid characters, assuming it to be a TAG.\n");
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
		 * it going for a test, TODO: redo later. rgerhards 2004-11-16 */
		/* TODO: quick and dirty memory allocation */
		/* lol.. we tried to solve it, just to remind ourselfs that 32 octets
		 * is the max size ;) we need to shuffle the code again... Just for 
		 * the records: the code is currently clean, but we could optimize it! */
		if(!bTAGCharDetected) {
			char *pszTAG;
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

			pszTAG = rsCStrConvSzStrAndDestruct(pStrB);
			if(pszTAG == NULL)
			{	/* rger, 2005-11-10: no TAG found - this implies that what
				 * we have considered to be the HOSTNAME is most probably the
				 * TAG. We consider it so probable, that we now adjust it
				 * that way. So we pick up the previously set hostname, assign
				 * it to tag and use the sender system (from IP stack) as
				 * the hostname. This situation is the standard case with
				 * stock BSD syslogd.
				 */
				dprintf("No TAG in message, assuming that HOSTNAME is missing.\n");
				moveHOSTNAMEtoTAG(pMsg);
				MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
			}
			else
			{ /* we have a TAG, so we can happily set it ;) */
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
		dprintf("HOSTNAME and TAG not parsed by user configuraton.\n");
		MsgSetHOSTNAME(pMsg, getRcvFrom(pMsg));
	}

	/* The rest is the actual MSG */
	if(MsgSetMSG(pMsg, p2parse) != 0) return 1;

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
void logmsg(int pri, struct msg *pMsg, int flags)
{
	char *msg;

	assert(pMsg != NULL);
	assert(pMsg->pszUxTradMsg != NULL);
	msg = pMsg->pszUxTradMsg;
	dprintf("logmsg: %s, flags %x, from '%s', msg %s\n", textpri(pri), flags, getRcvFrom(pMsg), msg);

#ifndef SYSV
	omask = sigblock(sigmask(SIGHUP)|sigmask(SIGALRM));
#endif
	/* rger 2005-11-24 (happy thanksgiving!): we now need to check if we have
	 * a traditional syslog message or one formatted according to syslog-protocol.
	 * We need to apply different parsers depending on that. We use the
	 * -protocol VERSION field for the detection.
	 */
	if(msg[0] == '1' && msg[1] == ' ') {
		dprintf("Message has syslog-protocol format.\n");
		setProtocolVersion(pMsg, 1);
		if(parseRFCSyslogMsg(pMsg, flags) == 1)
			return;
	} else { /* we have legacy syslog */
		dprintf("Message has legacy syslog format.\n");
		setProtocolVersion(pMsg, 0);
		if(parseLegacySyslogMsg(pMsg, flags) == 1)
			return;
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
	enqueueMsg(pMsg);

#ifndef SYSV
	(void) sigsetmask(omask);
#endif
}


/* Helper to doSQLEscape. This is called if doSQLEscape
 * runs out of memory allocating the escaped string.
 * Then we are in trouble. We can
 * NOT simply return the unmodified string because this
 * may cause SQL injection. But we also can not simply
 * abort the run, this would be a DoS. I think an appropriate
 * measure is to remove the dangerous \' characters. We
 * replace them by \", which will break the message and
 * signatures eventually present - but this is the
 * best thing we can do now (or does anybody 
 * have a better idea?). rgerhards 2004-11-23
 * added support for "escapeMode" (so doSQLEscape for details).
 * if mode = 1, then backslashes are changed to slashes.
 * rgerhards 2005-09-22
 */
void doSQLEmergencyEscape(register char *p, int escapeMode)
{
	while(*p) {
		if(*p == '\'')
			*p = '"';
		else if((escapeMode == 1) && (*p == '\\'))
			*p = '/';
		++p;
	}
}


/* SQL-Escape a string. Single quotes are found and
 * replaced by two of them. A new buffer is allocated
 * for the provided string and the provided buffer is
 * freed. The length is updated. Parameter pbMustBeFreed
 * is set to 1 if a new buffer is allocated. Otherwise,
 * it is left untouched.
 * --
 * We just discovered a security issue. MySQL is so
 * "smart" to not only support the standard SQL mechanism
 * for escaping quotes, but to also provide its own (using
 * c-type syntax with backslashes). As such, it is actually
 * possible to do sql injection via rsyslogd. The cure is now
 * to escape backslashes, too. As we have found on the web, some
 * other databases seem to be similar "smart" (why do we have standards
 * at all if they are violated without any need???). Even better, MySQL's
 * smartness depends on config settings. So we add a new option to this
 * function that allows the caller to select if they want to standard or
 * "smart" encoding ;)
 * new parameter escapeMode is 0 - standard sql, 1 - "smart" engines
 * 2005-09-22 rgerhards
 */
void doSQLEscape(char **pp, size_t *pLen, unsigned short *pbMustBeFreed, int escapeMode)
{
	char *p;
	int iLen;
	rsCStrObj *pStrB;
	char *pszGenerated;

	assert(pp != NULL);
	assert(*pp != NULL);
	assert(pLen != NULL);
	assert(pbMustBeFreed != NULL);

	/* first check if we need to do anything at all... */
	if(escapeMode == 0)
		for(p = *pp ; *p && *p != '\'' ; ++p)
			;
	else
		for(p = *pp ; *p && *p != '\'' && *p != '\\' ; ++p)
			;
	/* when we get out of the loop, we are either at the
	 * string terminator or the first \'. */
	if(*p == '\0')
		return; /* nothing to do in this case! */

	p = *pp;
	iLen = *pLen;
	if((pStrB = rsCStrConstruct()) == NULL) {
		/* oops - no mem ... Do emergency... */
		doSQLEmergencyEscape(p, escapeMode);
		return;
	}
	
	while(*p) {
		if(*p == '\'') {
			if(rsCStrAppendChar(pStrB, (escapeMode == 0) ? '\'' : '\\') != RS_RET_OK) {
				doSQLEmergencyEscape(*pp, escapeMode);
				rsCStrFinish(pStrB);
				if((pszGenerated = rsCStrConvSzStrAndDestruct(pStrB)) != NULL)
					free(pszGenerated);
				return;
				}
			iLen++;	/* reflect the extra character */
		} else if((escapeMode == 1) && (*p == '\\')) {
			if(rsCStrAppendChar(pStrB, '\\') != RS_RET_OK) {
				doSQLEmergencyEscape(*pp, escapeMode);
				rsCStrFinish(pStrB);
				if((pszGenerated = rsCStrConvSzStrAndDestruct(pStrB)) != NULL)
					free(pszGenerated);
				return;
				}
			iLen++;	/* reflect the extra character */
		}
		if(rsCStrAppendChar(pStrB, *p) != RS_RET_OK) {
			doSQLEmergencyEscape(*pp, escapeMode);
			rsCStrFinish(pStrB);
			if((pszGenerated = rsCStrConvSzStrAndDestruct(pStrB)) != NULL) 
				free(pszGenerated);
			return;
		}
		++p;
	}
	rsCStrFinish(pStrB);
	if((pszGenerated = rsCStrConvSzStrAndDestruct(pStrB)) == NULL) {
		doSQLEmergencyEscape(*pp, escapeMode);
		return;
	}

	if(*pbMustBeFreed)
		free(*pp); /* discard previous value */

	*pp = pszGenerated;
	*pLen = iLen;
	*pbMustBeFreed = 1;
}


/* create a string from the provided iovec. This can
 * be called by all functions who need the template
 * text in a single string. The function takes an
 * entry of the filed structure. It uses all data
 * from there. It returns a pointer to the generated
 * string if it succeeded, or NULL otherwise.
 * rgerhards 2004-11-22 
 */
char *iovAsString(struct filed *f)
{
	struct iovec *v;
	int i;
	rsCStrObj *pStrB;

	assert(f != NULL);

	if(f->f_psziov != NULL) {
		/* for now, we always free a previous buffer.
		 * The idea, however, is to keep a copy of the
		 * buffer until we know we no longer can re-use it.
		 */
		free(f->f_psziov);
	}

	if((pStrB = rsCStrConstruct()) == NULL) {
		/* oops - no mem, let's try to set the message we have
		 * most probably, this will fail, too. But at least we
		 * can try... */
		return NULL;
	}

	i = 0;
	f->f_iLenpsziov = 0;
	v = f->f_iov;
	while(i++ < f->f_iIovUsed) {
		if(v->iov_len > 0) {
			rsCStrAppendStr(pStrB, v->iov_base);
			f->f_iLenpsziov += v->iov_len;
		}
		++v;
	}

	rsCStrFinish(pStrB);
	f->f_psziov = rsCStrConvSzStrAndDestruct(pStrB);
	return f->f_psziov;
}


/* rgerhards 2004-11-24: free the to-be-freed string in
 * iovec. Some strings point to read-only constants in the
 * msg object, these must not be taken care of. But some
 * are specifically created for this instance and those
 * must be freed before the next is created. This is done
 * here. After this method has been called, the iovec
 * string array is invalid and must either be totally
 * discarded or e-initialized!
 */
void iovDeleteFreeableStrings(struct filed *f)
{
	register int i;

	assert(f != NULL);

	for(i = 0 ; i < f->f_iIovUsed ; ++i) {
		/* free to-be-freed strings in iovec */
		if(*(f->f_bMustBeFreed + i)) {
			free((f->f_iov + i)->iov_base);
			*(f->f_bMustBeFreed) = 0;
		}
	}
}


/* rgerhards 2004-11-19: create the iovec for
 * a given message. This is called by all methods
 * using iovec's for their output. Returns the number
 * of iovecs used (might be different from max if the
 * template contains an invalid entry).
 */
void  iovCreate(struct filed *f)
{
	register struct iovec *v;
	int iIOVused;
	struct template *pTpl;
	struct templateEntry *pTpe;
	struct msg *pMsg;

	assert(f != NULL);

	/* discard previous memory buffers */
	iovDeleteFreeableStrings(f);
	
	pMsg = f->f_pMsg;
	pTpl = f->f_pTpl;
	v = f->f_iov;
	
	iIOVused = 0;
	pTpe = pTpl->pEntryRoot;
	while(pTpe != NULL) {
		if(pTpe->eEntryType == CONSTANT) {
			v->iov_base = pTpe->data.constant.pConstant;
			v->iov_len = pTpe->data.constant.iLenConstant;
			++v;
			++iIOVused;
		} else 	if(pTpe->eEntryType == FIELD) {
			v->iov_base = MsgGetProp(pMsg, pTpe, NULL, f->f_bMustBeFreed + iIOVused);
			v->iov_len = strlen(v->iov_base);
			/* TODO: performance optimize - can we obtain the length? */
			/* we now need to check if we should use SQL option. In this case,
			 * we must go over the generated string and escape '\'' characters.
			 * rgerhards, 2005-09-22: the option values below look somewhat misplaced,
			 * but they are handled in this way because of legacy (don't break any
			 * existing thing).
			 */
			if(f->f_pTpl->optFormatForSQL == 1)
				doSQLEscape((char**)&v->iov_base, &v->iov_len,
					    f->f_bMustBeFreed + iIOVused, 1);
			else if(f->f_pTpl->optFormatForSQL == 2)
				doSQLEscape((char**)&v->iov_base, &v->iov_len,
					    f->f_bMustBeFreed + iIOVused, 0);
			++v;
			++iIOVused;
		}
		pTpe = pTpe->pNext;
	}
	
	f->f_iIovUsed = iIOVused;

#if 0 /* debug aid */
{
	int i;
	v = f->f_iov;
	for(i = 0 ; i < iIOVused ; ++i, ++v) {
		printf("iovCreate(%d), string '%s', mustbeFreed %d\n", i,
		        v->iov_base, *(f->f_bMustBeFreed + i));
	}
}
#endif /* debug aid */
	return;
}

/* rgerhards 2005-06-21: Try to resolve a size limit
 * situation. This first runs the command, and then
 * checks if we are still above the treshold.
 * returns 0 if ok, 1 otherwise
 * TODO: consider moving the initial check in here, too
 */
int resolveFileSizeLimit(struct filed *f)
{
	off_t actualFileSize;
	assert(f != NULL);

	if(f->f_sizeLimitCmd == NULL)
		return 1; /* nothing we can do in this case... */
	
	/* TODO: this is a really quick hack. We need something more
	 * solid when it goes into production. This was just to see if
	 * the overall idea works (I hope it won't survive...).
	 * rgerhards 2005-06-21
	 */
	system(f->f_sizeLimitCmd);

	f->f_file = open(f->f_un.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
			 0644);

	actualFileSize = lseek(f->f_file, 0, SEEK_END);
	if(actualFileSize >= f->f_sizeLimit) {
		/* OK, it didn't work out... */
		return 1;
		}

	return 0;
}

/* rgerhards 2004-11-11: write to a file output. This
 * will be called for all outputs using file semantics,
 * for example also for pipes.
 */
void writeFile(struct filed *f)
{
	off_t actualFileSize;
	assert(f != NULL);

	/* create the message based on format specified */
	iovCreate(f);
again:
	/* first check if we have a file size limit and, if so,
	 * obey to it.
	 */
	if(f->f_sizeLimit != 0) {
		actualFileSize = lseek(f->f_file, 0, SEEK_END);
		if(actualFileSize >= f->f_sizeLimit) {
			char errMsg[256];
			/* for now, we simply disable a file once it is
			 * beyond the maximum size. This is better than having
			 * us aborted by the OS... rgerhards 2005-06-21
			 */
			(void) close(f->f_file);
			/* try to resolve the situation */
			if(resolveFileSizeLimit(f) != 0) {
				/* didn't work out, so disable... */
				f->f_type = F_UNUSED;
				snprintf(errMsg, sizeof(errMsg),
					 "no longer writing to file %s; grown beyond configured file size of %lld bytes, actual size %lld - configured command did not resolve situation",
					 f->f_un.f_fname, (long long) f->f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				logerror(errMsg);
				return;
			} else {
				snprintf(errMsg, sizeof(errMsg),
					 "file %s had grown beyond configured file size of %lld bytes, actual size was %lld - configured command resolved situation",
					 f->f_un.f_fname, (long long) f->f_sizeLimit, (long long) actualFileSize);
				errno = 0;
				logerror(errMsg);
			}
		}
	}

	if (writev(f->f_file, f->f_iov, f->f_iIovUsed) < 0) {
		int e = errno;

		/* If a named pipe is full, just ignore it for now
		   - mrn 24 May 96 */
		if (f->f_type == F_PIPE && e == EAGAIN)
			return;

		(void) close(f->f_file);
		/*
		 * Check for EBADF on TTY's due to vhangup()
		 * Linux uses EIO instead (mrn 12 May 96)
		 */
		if ((f->f_type == F_TTY || f->f_type == F_CONSOLE)
#ifdef linux
			&& e == EIO) {
#else
			&& e == EBADF) {
#endif
			f->f_file = open(f->f_un.f_fname, O_WRONLY|O_APPEND|O_NOCTTY);
			if (f->f_file < 0) {
				f->f_type = F_UNUSED;
				logerror(f->f_un.f_fname);
			} else {
				untty();
				goto again;
			}
		} else {
			f->f_type = F_UNUSED;
			errno = e;
			logerror(f->f_un.f_fname);
		}
	} else if (f->f_flags & SYNC_FILE)
		(void) fsync(f->f_file);
}

/* rgerhards 2004-11-09: fprintlog() is the actual driver for
 * the output channel. It receives the channel description (f) as
 * well as the message and outputs them according to the channel
 * semantics. The message is typically already contained in the
 * channel save buffer (f->f_prevline). This is not only the case
 * when a message was already repeated but also when a new message
 * arrived. Parameter "msg", which sounds like the message content,
 * actually contains the message only in those few cases where it
 * was too large to fit into the channel save buffer.
 *
 * This whole function is probably about to change once we have the
 * message abstraction.
 */
void fprintlog(register struct filed *f)
{
	char *msg;
	char *psz; /* for shell support */
	int esize; /* for shell support */
	char *exec; /* for shell support */
	rsCStrObj *pCSCmdLine; /* for shell support: command to execute */
	rsRetVal iRet;
#ifdef SYSLOG_INET
	register int l;
	time_t fwd_suspend;
	struct hostent *hp;
#endif

	msg = f->f_pMsg->pszMSG;
	dprintf("Called fprintlog, ");

#if 0
	/* finally commented code out, because it is not working at all ;)
	 * TODO: handle the case of message repeation. Currently, there is still
	 * some code to do it, but that code is defunct due to our changes!
	 */
	if (msg) {
		v->iov_base = f->f_pMsg->pszRawMsg;
		v->iov_len = f->f_pMsg->iLenRawMsg;
	} else if (f->f_prevcount > 1) {
		(void) snprintf(repbuf, sizeof(repbuf), "last message repeated %d times",
		    f->f_prevcount);
		v->iov_base = repbuf;
		v->iov_len = strlen(repbuf);
	} else {
		v->iov_base = f->f_pMsg->pszMSG;
		v->iov_len = f->f_pMsg->iLenMSG;
	}
#endif

	dprintf("logging to %s", TypeNames[f->f_type]);

	switch (f->f_type) {
	case F_UNUSED:
		f->f_time = now;
		dprintf("\n");
		break;

#ifdef SYSLOG_INET
	case F_FORW_SUSP:
		fwd_suspend = time((time_t *) 0) - f->f_time;
		if ( fwd_suspend >= INET_SUSPEND_TIME ) {
			dprintf("\nForwarding suspension over, " \
				"retrying FORW ");
			f->f_type = F_FORW;
			goto f_forw;
		}
		else {
			dprintf(" %s\n", f->f_un.f_forw.f_hname);
			dprintf("Forwarding suspension not over, time " \
				"left: %d.\n", INET_SUSPEND_TIME - \
				fwd_suspend);
		}
		break;
		
	/*
	 * The trick is to wait some time, then retry to get the
	 * address. If that fails retry x times and then give up.
	 *
	 * You'll run into this problem mostly if the name server you
	 * need for resolving the address is on the same machine, but
	 * is started after syslogd. 
	 */
	case F_FORW_UNKN:
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		fwd_suspend = time((time_t *) 0) - f->f_time;
		if ( fwd_suspend >= INET_SUSPEND_TIME ) {
			dprintf("Forwarding suspension to unknown over, retrying\n");
			if ( (hp = gethostbyname(f->f_un.f_forw.f_hname)) == NULL ) {
				dprintf("Failure: %s\n", sys_h_errlist[h_errno]);
				dprintf("Retries: %d\n", f->f_prevcount);
				if ( --f->f_prevcount < 0 ) {
					dprintf("Giving up.\n");
					f->f_type = F_UNUSED;
				}
				else
					dprintf("Left retries: %d\n", f->f_prevcount);
			}
			else {
			        dprintf("%s found, resuming.\n", f->f_un.f_forw.f_hname);
				memcpy((char *) &f->f_un.f_forw.f_addr.sin_addr, hp->h_addr, hp->h_length);
				f->f_prevcount = 0;
				f->f_type = F_FORW;
				goto f_forw;
			}
		}
		else
			dprintf("Forwarding suspension not over, time " \
				"left: %d\n", INET_SUSPEND_TIME - fwd_suspend);
		break;

	case F_FORW:
	f_forw:
		dprintf(" %s:%d/%s\n", f->f_un.f_forw.f_hname, f->f_un.f_forw.port,
			 f->f_un.f_forw.protocol == FORW_UDP ? "udp" : "tcp");
		iovCreate(f);
		if ( strcmp(getHOSTNAME(f->f_pMsg), LocalHostName) && NoHops )
			dprintf("Not sending message to remote.\n");
		else {
			char *psz;
			f->f_time = now;
			psz = iovAsString(f);
			l = f->f_iLenpsziov;
			if (l > MAXLINE)
				l = MAXLINE;
			if(f->f_un.f_forw.protocol == FORW_UDP) {
				/* forward via UDP */
				if (sendto(finet, psz, l, 0, \
					   (struct sockaddr *) &f->f_un.f_forw.f_addr,
					   sizeof(f->f_un.f_forw.f_addr)) != l) {
					int e = errno;
					dprintf("INET sendto error: %d = %s.\n", 
						e, strerror(e));
					f->f_type = F_FORW_SUSP;
					errno = e;
					logerror("sendto");
				}
			} else {
				/* forward via TCP */
				if(TCPSend(f, psz) != 0) {
					/* error! */
					f->f_type = F_FORW_SUSP;
					errno = 0;
					logerror("error forwarding via tcp, suspending...");
				}
			}
		}
		break;
#endif

	case F_CONSOLE:
		f->f_time = now;
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
	case F_PIPE:
		dprintf(" (%s)\n", f->f_un.f_fname);
		/* TODO: check if we need f->f_time = now;*/
		/* f->f_file == -1 is an indicator that the we couldn't
		   open the file at startup. */
		if (f->f_file != -1)
			writeFile(f);
		break;

	case F_USERS:
	case F_WALL:
		f->f_time = now;
		dprintf("\n");
		wallmsg(f);
		break;

#ifdef	WITH_DB
	case F_MYSQL:
		f->f_time = now;
		dprintf("\n");
		writeMySQL(f);
		break;
#endif

	case F_SHELL: /* shell support by bkalkbrenner 2005-09-20 */
		f->f_time = now;
		iovCreate(f);
		psz = iovAsString(f);
		l = f->f_iLenpsziov;
		if (l > MAXLINE)
			l = MAXLINE;
		esize = strlen(f->f_un.f_fname) + strlen(psz) + 4;
		if((pCSCmdLine = rsCStrConstruct()) == NULL) {
			/* nothing smart we can do - just keep going... */
			dprintf("memory shortage - can not execute\n");
			break;
		}
		if((iRet = rsCStrAppendStr(pCSCmdLine, f->f_un.f_fname)) != RS_RET_OK) {
			dprintf("error %d during build command line(1)\n", iRet);
			break;
		}
		if((iRet = rsCStrAppendStr(pCSCmdLine, " \"")) != RS_RET_OK) {
			dprintf("error %d during build command line(2)\n", iRet);
			break;
		}
		/* now copy the message as parameter but escape dangerous things.
		 * we probably have not taken care of everything an attacker might
		 * think of, so execute shell *is* a dangerous command.
		 * rgerhards 2005-09-22
		 */
		while(*psz) {
			if(*psz == '"' || *psz == '\\') 
				if((iRet = rsCStrAppendChar(pCSCmdLine, '\\')) != RS_RET_OK) {
					dprintf("error %d during build command line(3)\n", iRet);
					break;
				}
			if((iRet = rsCStrAppendChar(pCSCmdLine, *psz)) != RS_RET_OK) {
				dprintf("error %d during build command line(4)\n", iRet);
				break;
			}
			++psz;
		}
		if((iRet = rsCStrAppendChar(pCSCmdLine, '"')) != RS_RET_OK) {
			dprintf("error %d during build command line(5)\n", iRet);
			break;
		}
		rsCStrFinish(pCSCmdLine);
		exec = rsCStrConvSzStrAndDestruct(pCSCmdLine);
		dprintf("Executing \"%s\"\n",exec);
		system(exec);	/* rgerhards: TODO: need to change this for root jail support! */
		free(exec);
		break;

	} /* switch */

	if (f->f_type != F_FORW_UNKN)
		f->f_prevcount = 0;
	return;		
}

jmp_buf ttybuf;

static void endtty()
{
	longjmp(ttybuf, 1);
}

/**
 * BSD setutent/getutent() replacement routines
 * The following routines emulate setutent() and getutent() under
 * BSD because they are not available there. We only emulate what we actually
 * need! rgerhards 2005-03-18
 */
#ifdef BSD
static FILE *BSD_uf = NULL;
void setutent(void)
{
	assert(BSD_uf == NULL);
	if ((BSD_uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		return;
	}
}

struct utmp* getutent(void)
{
	static struct utmp st_utmp;

	if(fread((char *)&st_utmp, sizeof(st_utmp), 1, BSD_uf) != 1)
		return NULL;

	return(&st_utmp);
}

void endutent(void)
{
	fclose(BSD_uf);
	BSD_uf = NULL;
}
#endif


/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 *
 * rgerhards, 2005-10-19: applying the following sysklogd patch:
 * Tue May  4 16:52:01 CEST 2004: Solar Designer <solar@openwall.com>
 *	Adjust the size of a variable to prevent a buffer overflow
 *	should _PATH_DEV ever contain something different than "/dev/".
 */
static void wallmsg(register struct filed *f)
{
  
	char p[sizeof(_PATH_DEV) + UNAMESZ];
	register int i;
	int ttyf;
	static int reenter = 0;
	struct utmp ut;
	struct utmp *uptr;

	assert(f != NULL);

	if (reenter++)
		return;

	iovCreate(f);	/* init the iovec */

	/* open the user login file */
	setutent();

	/*
	 * Might as well fork instead of using nonblocking I/O
	 * and doing notty().
	 */
	if (fork() == 0) {
		(void) signal(SIGTERM, SIG_DFL);
		(void) alarm(0);
#ifndef SYSV
		(void) signal(SIGTTOU, SIG_IGN);
		(void) sigsetmask(0);
#endif
	/* TODO: find a way to limit the max size of the message. hint: this
	 * should go into the template!
	 */
	
	/* rgerhards 2005-10-24: HINT: this code might be run in a seperate thread
	 * instead of a seperate process once we have multithreading...
	 */

		/* scan the user login file */
		while ((uptr = getutent())) {
			memcpy(&ut, uptr, sizeof(ut));
			/* is this slot used? */
			if (ut.ut_name[0] == '\0')
				continue;
#ifndef BSD
			if (ut.ut_type != USER_PROCESS)
			        continue;
#endif
			if (!(strncmp (ut.ut_name,"LOGIN", 6))) /* paranoia */
			        continue;

			/* should we send the message to this user? */
			if (f->f_type == F_USERS) {
				for (i = 0; i < MAXUNAMES; i++) {
					if (!f->f_un.f_uname[i][0]) {
						i = MAXUNAMES;
						break;
					}
					if (strncmp(f->f_un.f_uname[i],
					    ut.ut_name, UNAMESZ) == 0)
						break;
				}
				if (i >= MAXUNAMES)
					continue;
			}

			/* compute the device name */
			strcpy(p, _PATH_DEV);
			strncat(p, ut.ut_line, UNAMESZ);

			if (setjmp(ttybuf) == 0) {
				(void) signal(SIGALRM, endtty);
				(void) alarm(15);
				/* open the terminal */
				ttyf = open(p, O_WRONLY|O_NOCTTY);
				if (ttyf >= 0) {
					struct stat statb;

					if (fstat(ttyf, &statb) == 0 &&
					    (statb.st_mode & S_IWRITE))
						(void) writev(ttyf, f->f_iov, f->f_iIovUsed);
					close(ttyf);
					ttyf = -1;
				}
			}
			(void) alarm(0);
		}
		exit(0); /* "good" exit - this terminates the child forked just for message delivery */
	}
	/* close the user login file */
	endutent();
	reenter = 0;
}

static void reapchild()
{
	int saved_errno = errno;
#if defined(SYSV) && !defined(linux)
	(void) signal(SIGCHLD, reapchild);	/* reset signal handler -ASP */
	wait ((int *)0);
#else
	union wait status;

	while (wait3(&status, WNOHANG, (struct rusage *) NULL) > 0)
		;
#endif
#ifdef linux
	(void) signal(SIGCHLD, reapchild);	/* reset signal handler -ASP */
#endif
	errno = saved_errno;
}

/* Return a printable representation of a host address.
 */
static const char *cvthname(struct sockaddr_in *f)
{
	struct hostent *hp;
	register char *p;
	int count;

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address.\n");
		return ("???");
	}
	hp = gethostbyaddr((char *) &f->sin_addr, sizeof(struct in_addr), \
			   f->sin_family);
	if (hp == NULL) {
		dprintf("Host name for your address (%s) unknown.\n",
			inet_ntoa(f->sin_addr));
		return (inet_ntoa(f->sin_addr));
	}
	/* Convert to lower case, just like LocalDomain above
	 */
	for (p = (char *)hp->h_name; *p ; p++)
		if (isupper(*p))
			*p = tolower(*p);

	/* Notice that the string still contains the fqdn, but your
	 * hostname and domain are separated by a '\0'.
	 */
	if ((p = strchr(hp->h_name, '.'))) {
		if (strcmp(p + 1, LocalDomain) == 0) {
			*p = '\0';
			return (hp->h_name);
		} else {
			if (StripDomains) {
				count=0;
				while (StripDomains[count]) {
					if (strcmp(p + 1, StripDomains[count]) == 0) {
						*p = '\0';
						return (hp->h_name);
					}
					count++;
				}
			}
			if (LocalHosts) {
				count=0;
				while (LocalHosts[count]) {
					if (!strcmp(hp->h_name, LocalHosts[count])) {
						*p = '\0';
						return (hp->h_name);
					}
					count++;
				}
			}
		}
	}

	return (hp->h_name);
}


/* This method writes mark messages and - some time later - flushes reapeat
 * messages (not doing this currently).
 * This method was initially called by an alarm handler. As such, it could potentially
 * have  race-conditons. For details, see
 * http://lkml.org/lkml/2005/3/26/37
 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=301511
 * I have now changed it so that the alarm handler only sets a global variable, telling
 * the main thread that it must do mark processing. So domark() is now called from the
 * main thread itself, which is the only thing to make sure rsyslogd will not do
 * strange things. The way it originally was seemed to work because mark occurs very
 * seldom. However, the code called was anything else but reentrant, so it was like
 * russian roulette.
 * rgerhards, 2005-10-20
 */
static void domark(void)
{
	register struct filed *f;
	if (MarkInterval > 0) {
		now = time(0);
		MarkSeq += TIMERINTVL;
		if (MarkSeq >= MarkInterval) {
			logmsgInternal(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
			MarkSeq = 0;
		}

		for (f = Files; f != NULL ; f = f->f_next) {
			if (f->f_prevcount && now >= REPEATTIME(f)) {
				dprintf("flush %s: repeated %d times, %d sec.\n",
				    TypeNames[f->f_type], f->f_prevcount,
				    repeatinterval[f->f_repeatcount]);
				/* TODO: re-implement fprintlog(f, LocalHostName, 0, (char *)NULL); */
				BACKOFF(f);
			}
		}
	}
}


/* This is the alarm handler setting the global variable for
 * domark request. See domark() comments for further details.
 * rgerhards, 2005-10-20
 */
static void domarkAlarmHdlr()
{
	bRequestDoMark = 1; /* request alarm */
	(void) signal(SIGALRM, domarkAlarmHdlr);
	(void) alarm(TIMERINTVL);
}


static void debug_switch()
{
	dprintf("Switching debugging_on to %s\n", (debugging_on == 0) ? "true" : "false");
	debugging_on = (debugging_on == 0) ? 1 : 0;
	signal(SIGUSR1, debug_switch);
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
	logerror(buf);
	return;
}

/* Print syslogd errors some place.
 */
void logerror(char *type)
{
	char buf[1024];

	dprintf("Called logerr, msg: %s\n", type);

	if (errno == 0)
		snprintf(buf, sizeof(buf), "%s", type);
	else
		snprintf(buf, sizeof(buf), "%s: %s", type, strerror(errno));
	errno = 0;
	logmsgInternal(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
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
	dprintf("DoDie called.\n");
	bFinished = sig;
}


/* die() is called when the program shall end. This typically only occurs
 * during sigterm or during the initialization. If you search for places where
 * it is called, search for "die", not "die(", because the later will not find
 * setting of signal handlers! As die() is intended to shutdown rsyslogd, it is
 * safe to call exit() here. Just make sure that die() itself is not called
 * at inapropriate places. As a general rule of thumb, it is a bad idea to add
 * any calls to die() in new code!
 * rgerhards, 2005-10-24
 */
static void die(int sig)
{
	register struct filed *f;
	char buf[256];
	int i;
	int was_initialized = Initialized;

	Initialized = 0;	/* Don't log SIGCHLDs in case we receive one during exiting */

	for (f = Files; f != NULL ; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			/* rgerhards: 2004-11-09: I am now changing it, but
			 * I am not sure it is correct as done.
			 * TODO: verify later!
			 */
			fprintlog(f);
	}

	Initialized = was_initialized; /* we restore this so that the logmsgInternal() 
	                                * below can work ... and keep in mind we need the
					* filed structure still intact (initialized) for the below! */
	if (sig) {
		dprintf(" exiting on signal %d\n", sig);
		(void) snprintf(buf, sizeof(buf) / sizeof(char),
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION "." \
		 PATCHLEVEL "\" x-pid=\"%d\"]" " exiting on signal %d.",
		 (int) myPid, sig);
		errno = 0;
		logmsgInternal(LOG_SYSLOG|LOG_INFO, buf, LocalHostName, ADDDATE);
	}

#ifdef	USE_PTHREADS
	stopWorker();
	queueDelete(pMsgQueue); /* delete fifo here! */
	pMsgQueue = 0;
#endif


	/* Free ressources and close MySQL connections */
	for (f = Files; f != NULL ; f = f->f_next) {
		/* free iovec if it was allocated */
		if(f->f_iov != NULL) {
			if(f->f_bMustBeFreed != NULL) {
				iovDeleteFreeableStrings(f);
				free(f->f_bMustBeFreed);
			}
			free(f->f_iov);
		}
		/* Now delete cached messages */
		if(f->f_pMsg != NULL)
			MsgDestruct(f->f_pMsg);
#ifdef WITH_DB
		if (f->f_type == F_MYSQL)
			closeMySQL(f);
#endif
	}
	
	/* now clean up the listener part */

	/* Close the UNIX sockets. */
        for (i = 0; i < nfunix; i++)
		if (funix[i] != -1)
			close(funix[i]);
	/* Close the UDP inet socket. */
	if (InetInuse) close(inetm);

	/* Close the TCP inet socket. */
	if(bEnableTCP && sockTCPLstn != -1) {
		int iTCPSess;
		/* close all TCP connections! */
		iTCPSess = TCPSessGetNxtSess(-1);
		while(iTCPSess != -1) {
			int fd;
			fd = TCPSessions[iTCPSess].sock;
			dprintf("Closing TCP Session %d\n", fd);
			close(fd);
			/* now get next... */
			iTCPSess = TCPSessGetNxtSess(iTCPSess);
		}

		close(sockTCPLstn);
	}

	/* Clean-up files. */
        for (i = 0; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			(void)unlink(funixn[i]);

	/* rger 2005-02-22
	 * now clean up the in-memory structures. OK, the OS
	 * would also take care of that, but if we do it
	 * ourselfs, this makes finding memory leaks a lot
	 * easier.
	 */
	tplDeleteAll();
	free(parts);
	free(Files);
	if(consfile.f_iov != NULL)
		free(consfile.f_iov);
	if(consfile.f_bMustBeFreed != NULL)
		free(consfile.f_bMustBeFreed);

#ifndef TESTING
	(void) remove_pid(PidFile);
#endif
	exit(0); /* "good" exit, this is the terminator function for rsyslog [die()] */
}

/*
 * Signal handler to terminate the parent process.
 * rgerhards, 2005-10-24: this is only called during forking of the
 * detached syslogd. I consider this method to be safe.
 */
#ifndef TESTING
static void doexit(int sig)
{
	exit(0); /* "good" exit, only during child-creation */
}
#endif


/* parse an allowed sender config line and add the allowed senders
 * (if the line is correct).
 * rgerhards, 2005-09-27
 */
static rsRetVal addAllowedSenderLine(char* pName, char** ppRestOfConfLine)
{
	struct AllowedSenders **ppRoot;
	struct AllowedSenders **ppLast;
	rsParsObj *pPars;
	rsRetVal iRet;
	unsigned long uIP;
	int iBits;

	assert(pName != NULL);
	assert(ppRestOfConfLine != NULL);
	assert(*ppRestOfConfLine != NULL);

#ifndef SYSLOG_INET
	errno = 0;
	logerror("config file contains allowed sender list, but rsyslogd "
	         "compiled without Internet support - line ignored");
#else
	if(!strcasecmp(pName, "udp")) {
		ppRoot = &pAllowedSenders_UDP;
		ppLast = &pLastAllowedSenders_UDP;
	} else if(!strcasecmp(pName, "tcp")) {
		ppRoot = &pAllowedSenders_TCP;
		ppLast = &pLastAllowedSenders_TCP;
	} else {
		logerrorSz("Invalid protocol '%s' in allowed sender "
		           "list, line ignored", pName);
		return RS_RET_ERR;
	}

	/* OK, we now know the protocol and have valid list pointers.
	 * So let's process the entries. We are using the parse class
	 * for this.
	 */
	/* create parser object starting with line string without leading colon */
	if((iRet = rsParsConstructFromSz(&pPars, *ppRestOfConfLine) != RS_RET_OK)) {
		logerrorInt("Error %d constructing parser object - ignoring allowed sender list", iRet);
		return(iRet);
	}

	while(!parsIsAtEndOfParseString(pPars)) {
		if(parsPeekAtCharAtParsPtr(pPars) == '#')
			break; /* a comment-sign stops processing of line */
		/* now parse a single IP address */
		if((iRet = parsIPv4WithBits(pPars, &uIP, &iBits)) != RS_RET_OK) {
			logerrorInt("Error %d parsing IP address in allowed sender"
				    "list - ignoring.", iRet);
			rsParsDestruct(pPars);
			return(iRet);
		}
		if((iRet = AddAllowedSender(ppRoot, ppLast, uIP, iBits))
			!= RS_RET_OK) {
			logerrorInt("Error %d adding allowed sender entry "
				    "- ignoring.", iRet);
			rsParsDestruct(pPars);
			return(iRet);
		}

	}

	/* cleanup */
	return rsParsDestruct(pPars);
#endif /*#ifndef SYSLOG_INET */
}


/* parse and interpret a $-config line that starts with
 * a name (this is common code). It is parsed to the name
 * and then the proper sub-function is called to handle
 * the actual directive.
 * rgerhards 2004-11-17
 * rgerhards 2005-06-21: previously only for templates, now 
 *    generalized.
 */
static void doNameLine(char **pp, enum eDirective eDir)
{
	char *p = *pp;
	char szName[128];

	assert(pp != NULL);
	assert(p != NULL);
	assert(   (eDir == DIR_TEMPLATE) || (eDir == DIR_OUTCHANNEL)
	       || (eDir == DIR_ALLOWEDSENDER));

	if(getSubString(&p, szName, sizeof(szName) / sizeof(char), ',')  != 0) {
		char errMsg[128];
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
		         "Invalid $%s line: could not extract name - line ignored",
			 directive_name_list[eDir]);
		logerror(errMsg);
		return;
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
	}

	*pp = p;
	return;
}


/* Parse and interpret a system-directive in the config line
 * A system directive is one that starts with a "$" sign. It offers
 * extended configuration parameters.
 * 2004-11-17 rgerhards
 */
void cfsysline(char *p)
{
	char szCmd[32];

	assert(p != NULL);
	errno = 0;
	dprintf("cfsysline --> %s", p);
	if(getSubString(&p, szCmd, sizeof(szCmd) / sizeof(char), ' ')  != 0) {
		logerror("Invalid $-configline - could not extract command - line ignored\n");
		return;
	}

	/* check the command and carry out processing */
	if(!strcasecmp(szCmd, "template")) { 
		doNameLine(&p, DIR_TEMPLATE);
	} else if(!strcasecmp(szCmd, "outchannel")) { 
		doNameLine(&p, DIR_OUTCHANNEL);
	} else if(!strcasecmp(szCmd, "allowedsender")) { 
		doNameLine(&p, DIR_ALLOWEDSENDER);
	} else { /* invalid command! */
		char err[100];
		snprintf(err, sizeof(err)/sizeof(char),
		         "Invalid command in $-configline: '%s' - line ignored\n", szCmd);
		logerror(err);
		return;
	}
}


/* INIT -- Initialize syslogd from configuration table
 */
static void init()
{
	register int i;
	register FILE *cf;
	register struct filed *f;
	register struct filed *nextp;
	register char *p;
	register unsigned int Forwarding = 0;
#ifdef CONT_LINE
	char cbuf[BUFSIZ];
	char *cline;
#else
	char cline[BUFSIZ];
#endif
	struct servent *sp;
	char bufStartUpMsg[512];

	/* initialize some static variables */
	pDfltHostnameCmp = NULL;
	pDfltProgNameCmp = NULL;
	eDfltHostnameCmpMode = HN_NO_COMP;

	nextp = NULL;
	if(LogPort == 0) {
		/* we shall use the default syslog/udp port, so let's
		 * look it up.
		 */
		sp = getservbyname("syslog", "udp");
		if (sp == NULL) {
			errno = 0;
			logerror("Could not find syslog/udp port in /etc/services."
			         "Now using IANA-assigned default of 514.");
			LogPort = 514;
		}
		else
			LogPort = sp->s_port;
	}

	/*
	 *  Close all open log files and free log descriptor array.
	 */
	dprintf("Called init.\n");
	Initialized = 0;
	if(Files != NULL) {
		struct filed *fPrev;
		dprintf("Initializing log structures.\n");

		f = Files;
		while (f != NULL) {
			/* flush any pending output */
			if (f->f_prevcount) {
				/* rgerhards: 2004-11-09: I am now changing it, but
				 * I am not sure it is correct as done.
				 * TODO: verify later!
				 */
				fprintlog(f);
			}

			/* free iovec if it was allocated */
			if(f->f_iov != NULL) {
				if(f->f_bMustBeFreed != NULL) {
					iovDeleteFreeableStrings(f);
					free(f->f_bMustBeFreed);
				}
				free(f->f_iov);
			}

			switch (f->f_type) {
				case F_FILE:
				case F_PIPE:
				case F_TTY:
				case F_CONSOLE:
					(void) close(f->f_file);
				break;
#				ifdef	WITH_DB
				case F_MYSQL:
					closeMySQL(f);
				break;
#				endif
			}
#			ifdef USE_PTHREADS
			/* delete any mutex objects, if present */
			if(   (   (f->f_type == F_FORW_SUSP)
			       || (f->f_type == F_FORW)
			       || (f->f_type == F_FORW_UNKN) )
			   && (f->f_un.f_forw.protocol == FORW_TCP)) {
				pthread_mutex_destroy(&f->f_un.f_forw.mtxTCPSend);
			}
#			endif
			/* done with this entry, we now need to delete itself */
			fPrev = f;
			f = f->f_next;
			free(fPrev);
		}

		/* Reflect the deletion of the Files linked list. */
		Files = NULL;
	}
	
	f = NULL;
	nextp = NULL;
	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		/* rgerhards: this code is executed to set defaults when the
		 * config file could not be opened. We might think about
		 * abandoning the run in this case - but this, too, is not
		 * very clever...
		 */
		dprintf("cannot open %s (%s).\n", ConfFile, strerror(errno));
		nextp = (struct filed *)calloc(1, sizeof(struct filed));
		Files = nextp; /* set the root! */
		cfline("*.ERR\t" _PATH_CONSOLE, nextp);
		nextp->f_next = (struct filed *)calloc(1, sizeof(struct filed));
		cfline("*.PANIC\t*", nextp->f_next);
		nextp->f_next = (struct filed *)calloc(1, sizeof(struct filed));
		snprintf(cbuf,sizeof(cbuf), "*.*\t%s", ttyname(0));
		cfline(cbuf, nextp->f_next);
		Initialized = 1;
	}
	else { /* we should consider moving this into a separate function - TODO */
		/*
		 *  Foreach line in the conf table, open that file.
		 */
	#if CONT_LINE
		cline = cbuf;
		while (fgets(cline, sizeof(cbuf) - (cline - cbuf), cf) != NULL) {
	#else
		while (fgets(cline, sizeof(cline), cf) != NULL) {
	#endif
			/*
			 * check for end-of-section, comments, strip off trailing
			 * spaces and newline character.
			 */
			for (p = cline; isspace(*p); ++p) /*SKIP SPACES*/;
			if (*p == '\0' || *p == '#')
				continue;

			if(*p == '$') {
				cfsysline(++p);
				continue;
			}
	#if CONT_LINE
			strcpy(cline, p);
	#endif
			for (p = strchr(cline, '\0'); isspace(*--p););
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
			*++p = '\0';

			/* allocate next entry and add it */
			f = (struct filed *)calloc(1, sizeof(struct filed));
			/* TODO: check for NULL pointer (this is a general issue in this code...)! */
			if(nextp == NULL) {
				Files = f;
			}
			else {
				nextp->f_next = f;
			}
			nextp = f;

			/* be careful: the default below must be set BEFORE calling cfline()! */
			f->f_sizeLimit = 0; /* default value, use outchannels to configure! */
	#if CONT_LINE
			cfline(cbuf, f);
	#else
			cfline(cline, f);
	#endif
			if (f->f_type == F_FORW || f->f_type == F_FORW_SUSP || f->f_type == F_FORW_UNKN) {
				Forwarding++;
			}
		}

		/* close the configuration file */
		(void) fclose(cf);
	}

	/* we are now done with reading the configuraton. This is the right time to
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


#ifdef SYSLOG_UNIXAF
	for (i = startIndexUxLocalSockets ; i < nfunix ; i++) {
		if (funix[i] != -1)
			/* Don't close the socket, preserve it instead
			close(funix[i]);
			*/
			continue;
		if ((funix[i] = create_unix_socket(funixn[i])) != -1)
			dprintf("Opened UNIX socket `%s' (fd %d).\n", funixn[i], funix[i]);
	}
#endif

#ifdef SYSLOG_INET
	if (bEnableTCP) {
		if (sockTCPLstn < 0) {
			sockTCPLstn = create_tcp_socket();
			if (sockTCPLstn >= 0) {
				dprintf("Opened syslog TCP port.\n");
			}
		}
	}

	if (Forwarding || AcceptRemote) {
		if (finet < 0) {
			finet = create_udp_socket();
			if (finet >= 0) {
				InetInuse = 1;
				dprintf("Opened syslog UDP port.\n");
			}
		}
	}
	else {
		if (finet >= 0)
			close(finet);
		finet = -1;
		InetInuse = 0;
	}
	inetm = finet;
#endif

	Initialized = 1;

	if(Debug) {
		printf("Active selectors:\n");
		for (f = Files; f != NULL ; f = f->f_next) {
			if (1) {
			//if (f->f_type != F_UNUSED) {
				if(f->pCSProgNameComp != NULL)
					printf("tag: '%s'\n", rsCStrGetSzStr(f->pCSProgNameComp));
				if(f->eHostnameCmpMode != HN_NO_COMP)
					printf("hostname: %s '%s'\n",
						f->eHostnameCmpMode == HN_COMP_MATCH ?
							"only" : "allbut",
						rsCStrGetSzStr(f->pCSHostnameComp));
				if(f->f_filter_type == FILTER_PRI) {
					for (i = 0; i <= LOG_NFACILITIES; i++)
						if (f->f_filterData.f_pmask[i] == TABLE_NOPRI)
							printf(" X ");
						else
							printf("%2X ", f->f_filterData.f_pmask[i]);
				} else {
					printf("PROPERTY-BASED Filter:\n");
					printf("\tProperty.: '%s'\n",
					       rsCStrGetSzStr(f->f_filterData.prop.pCSPropName));
					printf("\tOperation: ");
					if(f->f_filterData.prop.isNegated)
						printf("NOT ");
					printf("'%s'\n", getFIOPName(f->f_filterData.prop.operation));
					printf("\tValue....: '%s'\n",
					       rsCStrGetSzStr(f->f_filterData.prop.pCSCompValue));
					printf("\tAction...: ");
				}
				printf("%s: ", TypeNames[f->f_type]);
				switch (f->f_type) {
				case F_FILE:
				case F_PIPE:
				case F_TTY:
				case F_CONSOLE:
					printf("%s", f->f_un.f_fname);
					if (f->f_file == -1)
						printf(" (unused)");
					break;

				case F_SHELL:
					printf("%s", f->f_un.f_fname);
					break;

				case F_FORW:
				case F_FORW_SUSP:
				case F_FORW_UNKN:
					printf("%s", f->f_un.f_forw.f_hname);
					break;

				case F_USERS:
					for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
						printf("%s, ", f->f_un.f_uname[i]);
					break;
				}
				printf("\n");
			}
		}
		printf("\n");
		tplPrintList();
		ochPrintList();

#ifdef	SYSLOG_INET
		/* now the allowedSender lists: */
		PrintAllowedSenders(1); /* UDP */
		PrintAllowedSenders(2); /* TCP */
#endif 	/* #ifdef SYSLOG_INET */
	}

	/* we now generate the startup message. It now includes everything to
	 * identify this instance.
	 * rgerhards, 2005-08-17
	 */
	snprintf(bufStartUpMsg, sizeof(bufStartUpMsg)/sizeof(char), 
		 " [origin software=\"rsyslogd\" " "swVersion=\"" VERSION "." \
		 PATCHLEVEL "\" x-pid=\"%d\"][x-configInfo udpReception=\"%s\" " \
		 "udpPort=\"%d\" tcpReception=\"%s\" tcpPort=\"%d\"]" \
		 " restart",
		 (int) myPid,
		 AcceptRemote ? "Yes" : "No", LogPort,
		 bEnableTCP   ? "Yes" : "No",  TCPLstnPort );
	logmsgInternal(LOG_SYSLOG|LOG_INFO, bufStartUpMsg, LocalHostName, ADDDATE);

	(void) signal(SIGHUP, sighup_handler);
	dprintf(" restarted.\n");
}

/* helper to cfline() and its helpers. Assign the right template
 * to a filed entry and allocates memory for its iovec.
 * rgerhards 2004-11-19
 */
static void cflineSetTemplateAndIOV(struct filed *f, char *pTemplateName)
{
	char errMsg[512];

	assert(f != NULL);
	assert(pTemplateName != NULL);

	/* Ok, we got everything, so it now is time to look up the
	 * template (Hint: templates MUST be defined before they are
	 * used!) and initialize the pointer to it PLUS the iov 
	 * pointer. We do the later because the template tells us
	 * how many elements iov must have - and this can never change.
	 */
	if((f->f_pTpl = tplFind(pTemplateName, strlen(pTemplateName))) == NULL) {
		snprintf(errMsg, sizeof(errMsg) / sizeof(char),
			 " Could not find template '%s' - selector line disabled\n",
			 pTemplateName);
		errno = 0;
		logerror(errMsg);
		f->f_type = F_UNUSED;
	} else {
		if((f->f_iov = calloc(tplGetEntryCount(f->f_pTpl),
		    sizeof(struct iovec))) == NULL) {
			/* TODO: provide better message! */
			errno = 0;
			logerror("Could not allocate iovec memory - 1 selector line disabled\n");
			f->f_type = F_UNUSED;
		}
		if((f->f_bMustBeFreed = calloc(tplGetEntryCount(f->f_pTpl),
		    sizeof(unsigned short))) == NULL) {
			errno = 0;
			logerror("Could not allocate bMustBeFreed memory - 1 selector line disabled\n");
			f->f_type = F_UNUSED;
		}
	}
}
	
/* Helper to cfline() and its helpers. Parses a template name
 * from an "action" line. Must be called with the Line pointer
 * pointing to the first character after the semicolon.
 * Everything is stored in the filed struct. If there is no
 * template name (it is empty), than it is ensured that the
 * returned string is "\0". So you can count on the first character
 * to be \0 in this case.
 * rgerhards 2004-11-19
 */
static void cflineParseTemplateName(struct filed *f, char** pp,
			     register char* pTemplateName, int iLenTemplate)
{
	register char *p;
	int i;

	assert(f != NULL);
	assert(pp != NULL);
	assert(*pp != NULL);

	p =*pp;

	/* Just as a general precaution, we skip whitespace.  */
	while(*p && isspace(*p))
		++p;

	i = 1; /* we start at 1 so that we reserve space for the '\0'! */
	while(*p && i < iLenTemplate) {
		*pTemplateName++ = *p++;
		++i;
	}
	*pTemplateName = '\0';

	*pp = p;
}

/* Helper to cfline(). Parses a file name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template. Everything is stored in the
 * filed struct.
 * rgerhards 2004-11-18
 */
static void cflineParseFileName(struct filed *f, char* p)
{
	register char *pName;
	int i;
	char szTemplateName[128];	/* should be more than sufficient */

	if(*p == '|') {
		f->f_type = F_PIPE;
		++p;
	} else {
		f->f_type = F_FILE;
	}

	pName = f->f_un.f_fname;
	i = 1; /* we start at 1 so that we resever space for the '\0'! */
	while(*p && *p != ';' && i < MAXFNAME) {
		*pName++ = *p++;
		++i;
	}
	*pName = '\0';

	/* got the file name - now let's look for the template to use
	 * Just as a general precaution, we skip whitespace.
	 */
	while(*p && isspace(*p))
		++p;
	if(*p == ';')
		++p; /* eat it */

	cflineParseTemplateName(f, &p, szTemplateName,
	                        sizeof(szTemplateName) / sizeof(char));

	if(szTemplateName[0] == '\0')	/* no template? */
		strcpy(szTemplateName, " TradFmt"); /* use default! */

	cflineSetTemplateAndIOV(f, szTemplateName);
	
	dprintf("filename: '%s', template: '%s'\n", f->f_un.f_fname, szTemplateName);
}


/* Helper to cfline(). Parses a output channel name up until the first
 * comma and then looks for the template specifier. Tries
 * to find that template. Maps the output channel to the 
 * proper filed structure settings. Everything is stored in the
 * filed struct. Over time, the dependency on filed might be
 * removed.
 * rgerhards 2005-06-21
 */
static void cflineParseOutchannel(struct filed *f, char* p)
{
	int i;
	struct outchannel *pOch;
	char szBuf[128];	/* should be more than sufficient */

	/* this must always be a file, because we can not set a size limit
	 * on a pipe...
	 * rgerhards 2005-06-21: later, this will be a separate type, but let's
	 * emulate things for the time being. When everything runs, we can
	 * extend it...
	 */
	f->f_type = F_FILE;

	++p; /* skip '$' */
	i = 0;
	/* get outchannel name */
	while(*p && *p != ';' && *p != ' ' &&
	      i < sizeof(szBuf) / sizeof(char)) {
	      szBuf[i++] = *p++;
	}
	szBuf[i] = '\0';

	/* got the name, now look up the channel... */
	pOch = ochFind(szBuf, i);

	if(pOch == NULL) {
		char errMsg[128];
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			 "outchannel '%s' not found - ignoring action line",
			 szBuf);
		logerror(errMsg);
		f->f_type = F_UNUSED;
		return;
	}

	/* check if there is a file name in the outchannel... */
	if(pOch->pszFileTemplate == NULL) {
		char errMsg[128];
		errno = 0;
		snprintf(errMsg, sizeof(errMsg)/sizeof(char),
			 "outchannel '%s' has no file name template - ignoring action line",
			 szBuf);
		logerror(errMsg);
		f->f_type = F_UNUSED;
		return;
	}

	/* OK, we finally got a correct template. So let's use it... */
	strncpy(f->f_un.f_fname, pOch->pszFileTemplate, MAXFNAME);
	f->f_sizeLimit = pOch->uSizeLimit;
	/* WARNING: It is dangerous "just" to pass the pointer. As wer
	 * never rebuild the output channel description, this is acceptable here.
	 */
	f->f_sizeLimitCmd = pOch->cmdOnSizeLimit;

	/* back to the input string - now let's look for the template to use
	 * Just as a general precaution, we skip whitespace.
	 */
	while(*p && isspace(*p))
		++p;
	if(*p == ';')
		++p; /* eat it */

	cflineParseTemplateName(f, &p, szBuf,
	                        sizeof(szBuf) / sizeof(char));

	if(szBuf[0] == '\0')	/* no template? */
		strcpy(szBuf, " TradFmt"); /* use default! */

	cflineSetTemplateAndIOV(f, szBuf);
	
	dprintf("[outchannel]filename: '%s', template: '%s', size: %lu\n", f->f_un.f_fname, szBuf,
		f->f_sizeLimit);
}


/*
 * Helper to cfline(). This function takes the filter part of a traditional, PRI
 * based line and decodes the PRIs given in the selector line. It processed the
 * line up to the beginning of the action part. A pointer to that beginnig is
 * passed back to the caller.
 * rgerhards 2005-09-15
 */
static rsRetVal cflineProcessTradPRIFilter(char **pline, register struct filed *f)
{
	char *p;
	register char *q;
	register int i, i2;
	char *bp;
	int pri;
	int singlpri = 0;
	int ignorepri = 0;
	char buf[MAXLINE];
	char xbuf[200];

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(f != NULL);

	dprintf(" - traditional PRI filter\n");
	errno = 0;	/* keep strerror() stuff out of logerror messages */

	f->f_filter_type = FILTER_PRI;
	/* Note: file structure is pre-initialized to zero because it was
	 * created with calloc()!
	 */
	for (i = 0; i <= LOG_NFACILITIES; i++) {
		f->f_filterData.f_pmask[i] = TABLE_NOPRI;
		f->f_flags = 0;
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
			(void) snprintf(xbuf, sizeof(xbuf), "unknown priority name \"%s\"", buf);
			logerror(xbuf);
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

					(void) snprintf(xbuf, sizeof(xbuf), "unknown facility name \"%s\"", buf);
					logerror(xbuf);
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
static rsRetVal cflineProcessPropFilter(char **pline, register struct filed *f)
{
	rsParsObj *pPars;
	rsCStrObj *pCSCompOp;
	rsRetVal iRet;
	int iOffset; /* for compare operations */

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(f != NULL);

	dprintf(" - property-based filter\n");
	errno = 0;	/* keep strerror() stuff out of logerror messages */

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

	if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, "contains", 8)) {
		f->f_filterData.prop.operation = FIOP_CONTAINS;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, "isequal", 7)) {
		f->f_filterData.prop.operation = FIOP_ISEQUAL;
	} else if(!rsCStrOffsetSzStrCmp(pCSCompOp, iOffset, "startswith", 10)) {
		f->f_filterData.prop.operation = FIOP_STARTSWITH;
	} else {
		logerrorSz("error: invalid compare operation '%s' - ignoring selector",
		           rsCStrGetSzStr(pCSCompOp));
	}
	RSFREEOBJ(pCSCompOp); /* no longer needed */

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
static rsRetVal cflineProcessHostSelector(char **pline, register struct filed *f)
{
	rsRetVal iRet;

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(**pline == '-' || **pline == '+');
	assert(f != NULL);

	dprintf(" - host selector line\n");

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
		dprintf("resetting BSD-like hostname filter\n");
		eDfltHostnameCmpMode = HN_NO_COMP;
		if(pDfltHostnameCmp != NULL) {
			if((iRet = rsCStrSetSzStr(pDfltHostnameCmp, NULL)) != RS_RET_OK)
				return(iRet);
			pDfltHostnameCmp = NULL;
		}
	} else {
		dprintf("setting BSD-like hostname filter to '%s'\n", *pline);
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
static rsRetVal cflineProcessTagSelector(char **pline, register struct filed *f)
{
	rsRetVal iRet;

	assert(pline != NULL);
	assert(*pline != NULL);
	assert(**pline == '!');
	assert(f != NULL);

	dprintf(" - programname selector line\n");

	(*pline)++;	/* eat '!' */

	/* the below is somewhat of a quick hack, but it is efficient (this is
	 * why it is in here. "!*" resets the tag selector with BSD syslog. We mimic
	 * this, too. As it is easy to check that condition, we do not fire up a
	 * parser process, just make sure we do not address beyond our space.
	 * Order of conditions in the if-statement is vital! rgerhards 2005-10-18
	 */
	if(**pline != '\0' && **pline == '*' && *(*pline+1) == '\0') {
		dprintf("resetting programname filter\n");
		if(pDfltProgNameCmp != NULL) {
			if((iRet = rsCStrSetSzStr(pDfltProgNameCmp, NULL)) != RS_RET_OK)
				return(iRet);
			pDfltProgNameCmp = NULL;
		}
	} else {
		dprintf("setting programname filter to '%s'\n", *pline);
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


/*
 * Crack a configuration file line
 * rgerhards 2004-11-17: well, I somewhat changed this function. It now does NOT
 * handle config lines in general, but only lines that reflect actual filter
 * pairs (the original syslog message line format). Extended lines (those starting
 * with "$" have been filtered out by the caller and are passed to another function (cfsysline()).
 * Please note, however, that I needed to make changes in the line syntax to support
 * assignment of format definitions to a file. So it is not (yet) 100% transparent.
 * Eventually, we can overcome this limitation by prefixing the actual action indicator
 * (e.g. "/file..") by something (e.g. "$/file..") - but for now, we just modify it... 
 */
static rsRetVal cfline(char *line, register struct filed *f)
{
	char *p;
	register char *q;
	register int i;
	int syncfile;
	rsRetVal iRet;
#ifdef SYSLOG_INET
	struct hostent *hp;
	int bErr;
#endif
	char szTemplateName[128];
#ifdef WITH_DB
	int iMySQLPropErr = 0;
#endif

	dprintf("cfline(%s)", line);

	errno = 0;	/* keep strerror() stuff out of logerror messages */
	p = line;

	/* check which filter we need to pull... */
	switch(*p) {
		case ':':
			iRet = cflineProcessPropFilter(&p, f);
			break;
		case '!':
			iRet = cflineProcessTagSelector(&p, f);
			return iRet; /* in this case, we are done */
		case '+':
		case '-':
			iRet = cflineProcessHostSelector(&p, f);
			return iRet; /* in this case, we are done */
		default:
			iRet = cflineProcessTradPRIFilter(&p, f);
			break;
	}

	/* check if that went well... */
	if(iRet != RS_RET_OK) {
		f->f_type = F_UNUSED;
		return iRet;
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

	if (*p == '-') {
		syncfile = 0;
		p++;
	} else
		syncfile = 1;

	dprintf("leading char in action: %c\n", *p);
	switch (*p)
	{
	case '@':
#ifdef SYSLOG_INET
		++p; /* eat '@' */
		if(*p == '@') { /* indicator for TCP! */
			f->f_un.f_forw.protocol = FORW_TCP;
			++p; /* eat this '@', too */
			/* in this case, we also need a mutex... */
#			ifdef USE_PTHREADS
			pthread_mutex_init(&f->f_un.f_forw.mtxTCPSend, 0);
#			endif
		} else {
			f->f_un.f_forw.protocol = FORW_UDP;
		}
		/* extract the host first (we do a trick - we 
		 * replace the ';' or ':' with a '\0')
		 * now skip to port and then template name 
		 * rgerhards 2005-07-06
		 */
		for(q = p ; *p && *p != ';' && *p != ':' ; ++p)
		 	/* JUST SKIP */;
		if(*p == ':') { /* process port */
			register int i = 0;
			*p = '\0'; /* trick to obtain hostname (later)! */
			for(++p ; *p && isdigit(*p) ; ++p) {
				i = i * 10 + *p - '0';
			}
			f->f_un.f_forw.port = i;
		}
		
		/* now skip to template */
		bErr = 0;
		while(*p && *p != ';') {
			if(*p && *p != ';' && !isspace(*p)) {
				if(bErr == 0) { /* only 1 error msg! */
					bErr = 1;
					errno = 0;
					logerror("invalid selector line (port), probably not doing what was intended");
				}
			}
			++p;
		}
	
		if(*p == ';') {
			*p = '\0'; /* trick to obtain hostname (later)! */
			++p;
			 /* Now look for the template! */
			cflineParseTemplateName(f, &p, szTemplateName,
						sizeof(szTemplateName) / sizeof(char));
		} else
			szTemplateName[0] = '\0';
		if(szTemplateName[0] == '\0') {
			/* we do not have a template, so let's use the default */
			strcpy(szTemplateName, " StdFwdFmt");
		}

		/* first set the f->f_type */
		if ( (hp = gethostbyname(q)) == NULL ) {
			f->f_type = F_FORW_UNKN;
			f->f_prevcount = INET_RETRY_MAX;
			f->f_time = time((time_t *) NULL);
		} else {
			f->f_type = F_FORW;
		}

		/* then try to find the template and re-set f_type to UNUSED
		 * if it can not be found. */
		cflineSetTemplateAndIOV(f, szTemplateName);
		if(f->f_type == F_UNUSED)
			/* safety measure to make sure we have a valid
			 * selector line before we continue down below.
			 * rgerhards 2005-07-29
			 */
			break;

		(void) strcpy(f->f_un.f_forw.f_hname, q);
		memset((char *) &f->f_un.f_forw.f_addr, 0,
			 sizeof(f->f_un.f_forw.f_addr));
		f->f_un.f_forw.f_addr.sin_family = AF_INET;
		if(f->f_un.f_forw.port == 0)
			f->f_un.f_forw.port = 514;
		f->f_un.f_forw.f_addr.sin_port = htons(f->f_un.f_forw.port);

		dprintf("forwarding host: '%s:%d/%s' template '%s'\n",
		         q, f->f_un.f_forw.port,
			 f->f_un.f_forw.protocol == FORW_UDP ? "udp" : "tcp",
			 szTemplateName);

		if ( f->f_type == F_FORW )
			memcpy((char *) &f->f_un.f_forw.f_addr.sin_addr, hp->h_addr, hp->h_length);
		/*
		 * Otherwise the host might be unknown due to an
		 * inaccessible nameserver (perhaps on the same
		 * host). We try to get the ip number later, like
		 * FORW_SUSP.
		 */
#endif
		break;

        case '$':
		/* rgerhards 2005-06-21: this is a special setting for output-channel
		 * defintions. In the long term, this setting will probably replace
		 * anything else, but for the time being we must co-exist with the
		 * traditional mode lines.
		 */
		cflineParseOutchannel(f, p);
		f->f_file = open(f->f_un.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
				 0644);
		break;

        case '|':
	case '/':
		/* rgerhards 2004-11-17: from now, we need to have different
		 * processing, because after the first comma, the template name
		 * to use is specified. So we need to scan for the first coma first
		 * and then look at the rest of the line.
		 */
		cflineParseFileName(f, p);
		if(f->f_type == F_UNUSED)
			/* safety measure to make sure we have a valid
			 * selector line before we continue down below.
			 * rgerhards 2005-07-29
			 */
			break;

		if (syncfile)
			f->f_flags |= SYNC_FILE;
		if (f->f_type == F_PIPE) {
			f->f_file = open(f->f_un.f_fname, O_RDWR|O_NONBLOCK);
	        } else {
			f->f_file = open(f->f_un.f_fname, O_WRONLY|O_APPEND|O_CREAT|O_NOCTTY,
					 0644);
		}
		        
	  	if ( f->f_file < 0 ){
			f->f_file = -1;
			dprintf("Error opening log file: %s\n", f->f_un.f_fname);
			logerror(f->f_un.f_fname);
			break;
		}
		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			untty();
		}
		if (strcmp(p, ctty) == 0)
			f->f_type = F_CONSOLE;
		break;

	case '*':
		dprintf ("write-all");
		f->f_type = F_WALL;
		if(*(p+1) == ';') {
			/* we have a template specifier! */
			p += 2; /* eat "*;" */
			cflineParseTemplateName(f, &p, szTemplateName,
						sizeof(szTemplateName) / sizeof(char));
		}
		else	/* assign default format if none given! */
			szTemplateName[0] = '\0';
		if(szTemplateName[0] == '\0')
			strcpy(szTemplateName, " WallFmt");
		cflineSetTemplateAndIOV(f, szTemplateName);
		if(f->f_type == F_UNUSED)
			/* safety measure to make sure we have a valid
			 * selector line before we continue down below.
			 * rgerhards 2005-07-29
			 */
			break;

		dprintf(" template '%s'\n", szTemplateName);
		break;

	case '~':	/* rgerhards 2005-09-09: added support for discard */
		dprintf ("discard\n");
		f->f_type = F_DISCARD;
		break;

	case '>':	/* rger 2004-10-28: added support for MySQL
			 * >server,dbname,userid,password
			 * rgerhards 2005-08-12: changed rsyslogd so that
			 * if no DB is selected and > is used, an error
			 * message is logged.
			 */
#ifndef	WITH_DB
		f->f_type = F_UNUSED;
		errno = 0;
		logerror("write to database action in config file, but rsyslogd compiled without "
		         "database functionality - ignored");
#else /* WITH_DB defined! */
		f->f_type = F_MYSQL;
		p++;
		
		/* Now we read the MySQL connection properties 
		 * and verify that the properties are valid.
		 */
		if(getSubString(&p, f->f_dbsrv, MAXHOSTNAMELEN+1, ','))
			iMySQLPropErr++;
		if(*f->f_dbsrv == '\0')
			iMySQLPropErr++;
		if(getSubString(&p, f->f_dbname, _DB_MAXDBLEN+1, ','))
			iMySQLPropErr++;
		if(*f->f_dbname == '\0')
			iMySQLPropErr++;
		if(getSubString(&p, f->f_dbuid, _DB_MAXUNAMELEN+1, ','))
			iMySQLPropErr++;
		if(*f->f_dbuid == '\0')
			iMySQLPropErr++;
		if(getSubString(&p, f->f_dbpwd, _DB_MAXPWDLEN+1, ';'))
			iMySQLPropErr++;
		if(*p == '\n' || *p == '\0') { 
			/* assign default format if none given! */
			szTemplateName[0] = '\0';
		} else {
			/* we have a template specifier! */
			cflineParseTemplateName(f, &p, szTemplateName,
						sizeof(szTemplateName) / sizeof(char));
		}

		if(szTemplateName[0] == '\0')
			strcpy(szTemplateName, " StdDBFmt");

		cflineSetTemplateAndIOV(f, szTemplateName);
		
		/* we now check if the template was present. If not, we
		 * can abort this run as the selector line has been
		 * disabled. If we don't abort, we'll core dump
		 * below. rgerhards 2005-07-29
		 */
		if(f->f_type == F_UNUSED)
			break;

		dprintf(" template '%s'\n", szTemplateName);
		
		/* If db used, the template have to use the SQL option.
		   This is for your own protection (prevent sql injection). */
		if (f->f_pTpl->optFormatForSQL == 0) {
			f->f_type = F_UNUSED;
			logerror("DB logging disabled. You have to use"
				" the SQL or stdSQL option in your template!\n");
			break;
		}
		
		/* If we dedect invalid properties, we disable logging, 
		 * because right properties are vital at this place.  
		 * Retries make no sense. 
		 */
		if (iMySQLPropErr) { 
			f->f_type = F_UNUSED;
			dprintf("Trouble with MySQL conncetion properties.\n"
				"MySQL logging disabled.\n");
		} else {
			initMySQL(f);
		}
#endif	/* #ifdef WITH_DB */
		break;

	case '^': /* bkalkbrenner 2005-09-20: execute shell command */
		dprintf("exec\n");
		++p;
		cflineParseFileName(f, p);
		if (f->f_type == F_FILE) {
			f->f_type = F_SHELL;
		}
		break; 

	default:
		dprintf ("users: %s\n", p);	/* ASP */
		f->f_type = F_USERS;
		for (i = 0; i < MAXUNAMES && *p && *p != ';'; i++) {
			for (q = p; *q && *q != ',' && *q != ';'; )
				q++;
			(void) strncpy(f->f_un.f_uname[i], p, UNAMESZ);
			if ((q - p) > UNAMESZ)
				f->f_un.f_uname[i][UNAMESZ] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		/* done, now check if we have a template name
		 * TODO: we need to handle the case where i >= MAXUNAME!
		 */
		szTemplateName[0] = '\0';
		if(*p == ';') {
			/* we have a template specifier! */
			++p; /* eat ";" */
			cflineParseTemplateName(f, &p, szTemplateName,
						sizeof(szTemplateName) / sizeof(char));
		}
		if(szTemplateName[0] == '\0')
			strcpy(szTemplateName, " StdUsrMsgFmt");
		cflineSetTemplateAndIOV(f, szTemplateName);
		/* Please note that we would need to check if the template
		 * was found. If not, f->f_type would be F_UNUSED and we
		 * can NOT carry on processing. These checks can be seen
		 * on all other selector line code above. However, as we
		 * do not have anything else to do here, we do not include
		 * this check. Should you add any further processing at
		 * this point here, you must first add a check for this
		 * condition!
		 * rgerhards 2005-07-29
		 */
		break;
	}
	return RS_RET_OK;
}


/*
 *  Decode a symbolic name to a numeric value
 */

int decode(name, codetab)
	char *name;
	struct code *codetab;
{
	register struct code *c;
	register char *p;
	char buf[80];

	dprintf ("symbolic name: %s", name);
	if (isdigit(*name))
	{
		dprintf ("\n");
		return (atoi(name));
	}
	(void) strncpy(buf, name, 79);
	for (p = buf; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
		{
			dprintf (" ==> %d\n", c->c_val);
			return (c->c_val);
		}
	return (-1);
}

void dprintf(char *fmt, ...)
{
#	ifdef USE_PTHREADS
	static int bWasNL = FALSE;
#	endif
	va_list ap;

	if ( !(Debug && debugging_on) )
		return;
	
#	ifdef USE_PTHREADS
	/* TODO: The bWasNL handler does not really work. It works if no thread
	 * switching occurs during non-NL messages. Else, things are messed
	 * up. Anyhow, it works well enough to provide useful help during
	 * getting this up and running. It is questionable if the extra effort
	 * is worth fixing it, giving the limited appliability.
	 * rgerhards, 2005-10-25
	 */
	if(bWasNL) {
		fprintf(stdout, "%8.8d: ", (unsigned int) pthread_self());
	}
	bWasNL = (*(fmt + strlen(fmt) - 1) == '\n') ? TRUE : FALSE;
#	endif
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);

	fflush(stdout);
	return;
}


/*
 * The following function is resposible for handling a SIGHUP signal.  Since
 * we are now doing mallocs/free as part of init we had better not being
 * doing this during a signal handler.  Instead this function simply sets
 * a flag variable which will tell the main loop to go through a restart.
 */
void sighup_handler()
{
	restart = 1;
	signal(SIGHUP, sighup_handler);
	return;
}

#ifdef WITH_DB
/*
 * The following function is responsible for initiatlizing a
 * MySQL connection.
 * Initially added 2004-10-28 mmeckelein
 */
static void initMySQL(register struct filed *f)
{
	int iCounter = 0;
	assert(f != NULL);

	if (checkDBErrorState(f))
		return;
	
	mysql_init(&f->f_hmysql);
	do {
		/* Connect to database */
		if (!mysql_real_connect(&f->f_hmysql, f->f_dbsrv, f->f_dbuid, f->f_dbpwd, f->f_dbname, 0, NULL, 0)) {
			/* if also the second attempt failed
			   we call the error handler */
			if(iCounter)
				DBErrorHandler(f);
		} else {
			f->f_timeResumeOnError = 0; /* We have a working db connection */
			dprintf("connected successfully to db\n");
		}
		iCounter++;
	} while (mysql_errno(&f->f_hmysql) && iCounter<2);
}

/*
 * The following function is responsible for closing a
 * MySQL connection.
 * Initially added 2004-10-28
 */
static void closeMySQL(register struct filed *f)
{
	assert(f != NULL);
	dprintf("in closeMySQL\n");
	mysql_close(&f->f_hmysql);	
}

/*
 * Reconnect a MySQL connection.
 * Initially added 2004-12-02
 */
static void reInitMySQL(register struct filed *f)
{
	assert(f != NULL);

	dprintf("reInitMySQL\n");
	closeMySQL(f); /* close the current handle */
	initMySQL(f); /* new connection */   
}

/*
 * The following function writes the current log entry
 * to an established MySQL session.
 * Initially added 2004-10-28
 */
static void writeMySQL(register struct filed *f)
{
	char *psz;
	int iCounter=0;
	assert(f != NULL);
	/* dprintf("in writeMySQL()\n"); */
	iovCreate(f);
	psz = iovAsString(f);
	
	if (checkDBErrorState(f))
		return;

	/* 
	 * Now we are trying to insert the data. 
	 *
	 * If the first attampt failes we simply try a second one. If also 
	 * the second attampt failed we discard this message and enable  
	 * the "delay" error hanlding.
	 */
	do {
		/* query */
		if(mysql_query(&f->f_hmysql, psz)) {

			/* if also the second attempt failed
			   we call the error handler */
			if(iCounter)
				DBErrorHandler(f);	
		}
		else {
			/* dprintf("db insert sucessfully\n"); */
		}
		iCounter++;
	} while (mysql_errno(&f->f_hmysql) && iCounter<2);
}

/**
 * DBErrorHandler
 *
 * Call this function if an db error apears. It will initiate 
 * the "delay" handling which stopped the db logging for some 
 * time.  
 */
static void DBErrorHandler(register struct filed *f)
{
	char errMsg[512];
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
	 * Think about diffrent "delay" for diffrent errors!
	 */
	errno = 0;
	snprintf(errMsg, sizeof(errMsg)/sizeof(char),
		"db error (%d): %s\n", mysql_errno(&f->f_hmysql),
		mysql_error(&f->f_hmysql));

	/* Enable "delay" */
	f->f_timeResumeOnError = time(&f->f_timeResumeOnError) + _DB_DELAYTIMEONERROR ;
	f->f_iLastDBErrNo = mysql_errno(&f->f_hmysql);

	/* Log error is the last step. */ 
	logerror(errMsg);
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
int checkDBErrorState(register struct filed *f)
{
	assert(f != NULL);
	/* dprintf("in checkDBErrorState, timeResumeOnError: %d\n", f->f_timeResumeOnError); */

	/* If timeResumeOnError == 0 no error occured, 
	   we can return with 0 (no error) */
	if (f->f_timeResumeOnError == 0)
		return 0;
	
	(void) time(&now);
	/* Now we know an error occured. We check timeResumeOnError
	   if we can process. If we have not reach the resume time
	   yet, we return an error status. */  
	if (f->f_timeResumeOnError > now)
	{
		/* dprintf("Wait time is not over yet.\n"); */
		return 1;
	}
	 	
	/* Ok, we can try to resume the database logging. First
	   we have to reset the status (timeResumeOnError) and
	   the last error no. */
	/* TODO:
	 * To improve this code it would be better to check 
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
	f->f_timeResumeOnError = 0;
	f->f_iLastDBErrNo = 0; 
	reInitMySQL(f);
	return 0;

}

#endif	/* #ifdef WITH_DB */

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
int getSubString(char **ppSrc,  char *pDst, size_t DstSize, char cSep)
{
	char *pSrc = *ppSrc;
	int iErr = 0; /* 0 = no error, >0 = error */
	while(*pSrc != cSep && *pSrc != '\0' && DstSize>1) {
		*pDst++ = *(pSrc)++;
		DstSize--;
	}
	/* check if the Dst buffer was to small */
	if (*pSrc != cSep && *pSrc != '\0')
	{ 
		dprintf("in getSubString, error Src buffer > Dst buffer\n");
		iErr = 1;
	}	
	if (*pSrc == '\0')
		/* this line was missing, causing ppSrc to be invalid when it
		 * was returned in case of end-of-string. rgerhards 2005-07-29
		 */
		*ppSrc = pSrc;
	else
		*ppSrc = pSrc+1;
	*pDst = '\0';
	return iErr;
}



static void mainloop(void)
{
	int i;
#if !defined(__GLIBC__)
	int len;
#else /* __GLIBC__ */
#ifndef TESTING
	size_t len;
#endif
#endif /* __GLIBC__ */
	fd_set readfds;
#ifdef  SYSLOG_INET
	fd_set writefds;
	struct filed *f;
#endif
#ifdef	BSD
#ifdef	USE_PTHREADS
	struct timeval tvSelectTimeout;
#endif
#endif

#ifndef TESTING
	int	fd;
#ifdef  SYSLOG_INET
	struct sockaddr_in frominet;
	char *from;
	int iTCPSess;
#endif
#endif

	char line[MAXLINE +1];
	int maxfds;

	/* --------------------- Main loop begins here. ----------------------------------------- */
	while(!bFinished){
		int nfds;
		errno = 0;
		FD_ZERO(&readfds);
		maxfds = 0;
#ifdef SYSLOG_UNIXAF
#ifndef TESTING
		/*
		 * Add the Unix Domain Sockets to the list of read
		 * descriptors.
		 * rgerhards 2005-08-01: we must now check if there are
		 * any local sockets to listen to at all. If the -o option
		 * is given without -a, we do not need to listen at all..
		 */
		/* Copy master connections */
		for (i = startIndexUxLocalSockets; i < nfunix; i++) {
			if (funix[i] != -1) {
				FD_SET(funix[i], &readfds);
				if (funix[i]>maxfds) maxfds=funix[i];
			}
		}
#endif
#endif
#ifdef SYSLOG_INET
#ifndef TESTING
		/*
		 * Add the Internet Domain Socket to the list of read
		 * descriptors.
		 */
		if ( InetInuse && AcceptRemote ) {
			FD_SET(inetm, &readfds);
			if (inetm>maxfds) maxfds=inetm;
			dprintf("Listening on syslog UDP port.\n");
		}

		/* Add the TCP socket to the list of read descriptors.
	    	 */
		if(bEnableTCP && sockTCPLstn != -1) {
			FD_SET(sockTCPLstn, &readfds);
			if (sockTCPLstn>maxfds) maxfds=sockTCPLstn;
			dprintf("Listening on syslog TCP port.\n");
			/* do the sessions */
			iTCPSess = TCPSessGetNxtSess(-1);
			while(iTCPSess != -1) {
				int fd;
				fd = TCPSessions[iTCPSess].sock;
				dprintf("Adding TCP Session %d\n", fd);
				FD_SET(fd, &readfds);
				if (fd>maxfds) maxfds=fd;
				/* now get next... */
				iTCPSess = TCPSessGetNxtSess(iTCPSess);
			}
		}

		/* TODO: activate the code below only if we actually need to check
		 * for outstanding writefds.
		 */
		if(1) {
			/* Now add the TCP output sockets to the writefds set. This implementation
			 * is not optimal (performance-wise) and it should be replaced with something
			 * better in the longer term. I've not yet done this, as this code is
			 * scheduled to be replaced after the liblogging integration.
			 * rgerhards 2005-07-20
			 */
			FD_ZERO(&writefds);
			for (f = Files; f != NULL ; f = f->f_next) {
				if(   (f->f_type == F_FORW)
				   && (f->f_un.f_forw.protocol == FORW_TCP)
				   && (TCPSendGetStatus(f) == TCP_SEND_CONNECTING)) {
				   FD_SET(f->f_file, &writefds);
				   if(f->f_file > maxfds)
					maxfds = f->f_file;
				   }
			}
		}
#endif
#endif
#ifdef TESTING
		FD_SET(fileno(stdin), &readfds);
		if (fileno(stdin) > maxfds) maxfds = fileno(stdin);

		dprintf("Listening on stdin.  Press Ctrl-C to interrupt.\n");
#endif

		if ( debugging_on ) {
			dprintf("----------------------------------------\nCalling select, active file descriptors (max %d): ", maxfds);
			for (nfds= 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dprintf("%d ", nfds);
			dprintf("\n");
		}

#define  MAIN_SELECT_TIMEVAL NULL
#ifdef BSD
#ifdef USE_PTHREADS
		/* There seems to be a problem with BSD and threads. When running on
		 * multiple threads, a signal will not cause the select call to be
		 * interrrupted. I am not sure if this is by design or an bug (some
		 * information on the web let's me think it is a bug), but that really
		 * does not matter. The issue with our code is that we will not gain
		 * control when rsyslogd is terminated or huped. What I am doing now is
		 * make the select call timeout after 10 seconds, so that we can check
		 * the condition then. Obviously, this causes some sluggish behaviour and
		 * also the loss of some (very few) cpu cycles. Both, I think, are
		 * absolutely acceptable.
		 * rgerhards, 2005-10-26
		 */
		tvSelectTimeout.tv_sec = 10;
		tvSelectTimeout.tv_usec = 0;
#		undef MAIN_SELECT_TIMEVAL 
#		define MAIN_SELECT_TIMEVAL &tvSelectTimeout
#endif
#endif
#ifdef SYSLOG_INET
#define MAIN_SELECT_WRITEFDS (fd_set *) &writefds
#else
#define MAIN_SELECT_WRITEFDS NULL
#endif
		nfds = select(maxfds+1, (fd_set *) &readfds, MAIN_SELECT_WRITEFDS,
				  (fd_set *) NULL, MAIN_SELECT_TIMEVAL);
#undef MAIN_SELECT_TIMEVAL 
#undef MAIN_SELECT_WRITEFDS

		if(bRequestDoMark) {
			domark();
			bRequestDoMark = 0;
			/* We do not use continue, because domark() is carried out
			 * only when something else happened.
			 */
		}
		if (restart) {
			dprintf("\nReceived SIGHUP, reloading rsyslogd.\n");
#			ifdef USE_PTHREADS
				stopWorker();
#			endif
			init();
#			ifdef USE_PTHREADS
				startWorker();
#			endif
			restart = 0;
			continue;
		}
		if (nfds == 0) {
			dprintf("No select activity.\n");
			continue;
		}
		if (nfds < 0) {
			if (errno != EINTR)
				logerror("select");
			dprintf("Select interrupted.\n");
			continue;
		}

		if ( debugging_on )
		{
			dprintf("\nSuccessful select, descriptor count = %d, " \
				"Activity on: ", nfds);
			for (nfds= 0; nfds <= maxfds; ++nfds)
				if ( FD_ISSET(nfds, &readfds) )
					dprintf("%d ", nfds);
			dprintf(("\n"));
		}

#ifndef TESTING
#ifdef SYSLOG_INET
		/* TODO: activate the code below only if we actually need to check
		 * for outstanding writefds.
		 */
		if(1) {
			/* Now check the TCP send sockets. So far, we only see if they become
			 * writable and then change their internal status. No real async
			 * writing is currently done. This code will be replaced once liblogging
			 * is used, thus we try not to focus too much on it.
			 *
			 * IMPORTANT: With the current code, the writefds must be checked first,
			 * because the readfds might have messages to be forwarded, which
			 * rely on the status setting that is done here!
			 *
			 * rgerhards 2005-07-20
			 */
			for (f = Files; f != NULL ; f = f->f_next) {
				if(   (f->f_type == F_FORW)
				   && (f->f_un.f_forw.protocol == FORW_TCP)
				   && (TCPSendGetStatus(f) == TCP_SEND_CONNECTING)
				   && (FD_ISSET(f->f_file, &writefds))) {
					dprintf("tcp send socket %d ready for writing.\n", f->f_file);
					TCPSendSetStatus(f, TCP_SEND_READY);
					/* Send stored message (if any) */
					if(f->f_un.f_forw.savedMsg != NULL) {
						if(TCPSend(f, f->f_un.f_forw.savedMsg) != 0) {
							/* error! */
							f->f_type = F_FORW_SUSP;
							errno = 0;
							logerror("error forwarding via tcp, suspending...");
						}
					   free(f->f_un.f_forw.savedMsg);
					   f->f_un.f_forw.savedMsg = NULL;
					}
				   }
			}
		}
#endif /* #ifdef SYSLOG_INET */
#ifdef SYSLOG_UNIXAF
		for (i = 0; i < nfunix; i++) {
			if ((fd = funix[i]) != -1 && FD_ISSET(fd, &readfds)) {
				int iRcvd;
				memset(line, '\0', sizeof(line));
				iRcvd = recv(fd, line, MAXLINE - 2, 0);
				dprintf("Message from UNIX socket: #%d\n", fd);
				if (iRcvd > 0) {
					line[iRcvd] = line[iRcvd+1] = '\0';
					printchopped(LocalHostName, line, iRcvd + 2,  fd, funixParseHost[i]);
				} else if (iRcvd < 0 && errno != EINTR) {
					dprintf("UNIX socket error: %d = %s.\n", \
						errno, strerror(errno));
					logerror("recvfrom UNIX");
				}
			}
		}
#endif

#ifdef SYSLOG_INET
		if (InetInuse && AcceptRemote && FD_ISSET(inetm, &readfds)) {
			len = sizeof(frominet);
			memset(line, '\0', sizeof(line));
			i = recvfrom(finet, line, MAXLINE - 2, 0, \
				     (struct sockaddr *) &frominet, &len);
			dprintf("Message from UDP inetd socket: #%d, host: %s\n",
				inetm, inet_ntoa(frominet.sin_addr));
			if (i > 0) {
				from = (char *)cvthname(&frominet);
				/* Here we check if a host is permitted to send us
				 * syslog messages. If it isn't, we do not further
				 * process the message but log a warning (if we are
				 * configured to do this).
				 * rgerhards, 2005-09-26
				 */
				if(isAllowedSender(pAllowedSenders_UDP, &frominet)) {
					line[i] = line[i+1] = '\0';
					printchopped(from, line, i + 2,  finet, 1);
				} else {
					if(option_DisallowWarning) {
						logerrorSz("UDP message from disallowed sender %s discarded",
							   from);
					}
				}
			} else if (i < 0 && errno != EINTR && errno != EAGAIN) {
				/* see link below why we check EAGAIN:
				 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=188194
				 */
				dprintf("INET socket error: %d = %s.\n", \
					errno, strerror(errno));
				logerror("recvfrom inet");
				/* should be harmless now that we set
				 * BSDCOMPAT on the socket */
				sleep(1);
			}
		}

		if(bEnableTCP && sockTCPLstn != -1) {
			/* Now check for TCP input */
			if(FD_ISSET(sockTCPLstn, &readfds)) {
				dprintf("New connect on TCP inetd socket: #%d\n", sockTCPLstn);
				TCPSessAccept();
			}

			/* now check the sessions */
			/* TODO: optimize the whole thing. We could stop enumerating as
			 * soon as we have found all sockets flagged as active. */
			iTCPSess = TCPSessGetNxtSess(-1);
			while(iTCPSess != -1) {
				int fd;
				int state;
				fd = TCPSessions[iTCPSess].sock;
				if(FD_ISSET(fd, &readfds)) {
					char buf[MAXLINE];
					dprintf("tcp session socket with new data: #%d\n", fd);

					/* Receive message */
					state = recv(fd, buf, sizeof(buf), 0);
					if(state == 0) {
						/* Session closed */
						TCPSessClose(iTCPSess);
					} else if(state == -1) {
						char errmsg[128];
						snprintf(errmsg, sizeof(errmsg)/sizeof(char),
							 "TCP session %d will be closed, error ignored",
							 fd);
						logerror(errmsg);
						TCPSessClose(iTCPSess);
					} else {
						/* valid data received, process it! */
						TCPSessDataRcvd(iTCPSess, buf, state);
					}
				}
				iTCPSess = TCPSessGetNxtSess(iTCPSess);
			}
		}

#endif
#else
		if ( FD_ISSET(fileno(stdin), &readfds) ) {
			dprintf("Message from stdin.\n");
			memset(line, '\0', sizeof(line));
			line[0] = '.';
			parts[fileno(stdin)] = NULL;
			i = read(fileno(stdin), line, MAXLINE);
			if (i > 0) {
				printchopped(LocalHostName, line, i+1,
					     fileno(stdin), 0);
		  	} else if (i < 0) {
		    		if (errno != EINTR) {
		      			logerror("stdin");
				}
		  	}
			FD_CLR(fileno(stdin), &readfds);
		  }
#endif
	}
}

int main(int argc, char **argv)
{	register int i;
	register char *p;
#if !defined(__GLIBC__)
	int num_fds;
#else /* __GLIBC__ */
	int num_fds;
#endif /* __GLIBC__ */

#ifdef	MTRACE
	mtrace(); /* this is a debug aid for leak detection - either remove
	           * or put in conditional compilation. 2005-01-18 RGerhards */
#endif

#ifndef TESTING
	pid_t ppid = getpid();
#endif
	int ch;
	struct hostent *hent;

	extern int optind;
	extern char *optarg;
	char *pTmp;

#ifndef TESTING
	chdir ("/");
#endif
	for (i = 1; i < MAXFUNIX; i++) {
		funixn[i] = "";
		funix[i]  = -1;
	}

	while ((ch = getopt(argc, argv, "a:dhi:f:l:m:nop:r:s:t:u:vw")) != EOF)
		switch((char)ch) {
		case 'a':
			if (nfunix < MAXFUNIX)
				if(*optarg == ':') {
					funixParseHost[nfunix] = 1;
					funixn[nfunix++] = optarg+1;
				}
				else {
					funixParseHost[nfunix] = 0;
					funixn[nfunix++] = optarg;
				}
			else
				fprintf(stderr, "Out of descriptors, ignoring %s\n", optarg);
			break;
		case 'd':		/* debug */
			Debug = 1;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'h':
			NoHops = 0;
			break;
		case 'i':		/* pid file name */
			PidFile = optarg;
			break;
		case 'l':
			if (LocalHosts) {
				fprintf (stderr, "Only one -l argument allowed," \
					"the first one is taken.\n");
				break;
			}
			LocalHosts = crunch_list(optarg);
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'n':		/* don't fork */
			NoFork = 1;
			break;
		case 'o':		/* omit local logging (/dev/log) */
			startIndexUxLocalSockets = 1;
			break;
		case 'p':		/* path to regular log socket */
			funixn[0] = optarg;
			break;
		case 'r':		/* accept remote messages */
			AcceptRemote = 1;
			LogPort = atoi(optarg);
			break;
		case 's':
			if (StripDomains) {
				fprintf (stderr, "Only one -s argument allowed," \
					"the first one is taken.\n");
				break;
			}
			StripDomains = crunch_list(optarg);
			break;
		case 't':		/* enable tcp logging */
			bEnableTCP = -1;
			TCPLstnPort = atoi(optarg);
			break;
		case 'u':		/* misc user settings */
			if(atoi(optarg) == 1)
				bParseHOSTNAMEandTAG = 0;
			break;
		case 'v':
			printf("rsyslogd %s.%s, ", VERSION, PATCHLEVEL);
			printf("compiled with:\n");
#ifdef USE_PTHREADS
			printf("\tFEATURE_PTHREADS (dual-threading)\n");
#endif
#ifdef FEATURE_REGEXP
			printf("\tFEATURE_REGEXP\n");
#endif
#ifdef WITH_DB
			printf("\tFEATURE_DB\n");
#endif
#ifndef	NOLARGEFILE
			printf("\tFEATURE_LARGEFILE\n");
#endif
#ifdef	SYSLOG_INET
			printf("\tSYSLOG_INET (Internet/remote support)\n");
#endif
#ifndef	NDEBUG
			printf("\tFEATURE_DEBUG (debug build, slow code)\n");
#endif
			printf("\nSee http://www.rsyslog.com for more information.\n");
			exit(0); /* exit for -v option - so this is a "good one" */
		case 'w':		/* disable disallowed host warnigs */
			option_DisallowWarning = 0;
			break;
		case '?':
		default:
			usage();
		}
	if ((argc -= optind))
		usage();

#ifndef TESTING
	if ( !(Debug || NoFork) )
	{
		dprintf("Checking pidfile.\n");
		if (!check_pid(PidFile))
		{
			signal (SIGTERM, doexit);
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
#endif
		debugging_on = 1;
#ifndef SYSV
	else
		setlinebuf(stdout);
#endif

#ifndef TESTING
	/* tuck my process id away */
	if ( !Debug )
	{
		dprintf("Writing pidfile.\n");
		if (!check_pid(PidFile))
		{
			if (!write_pid(PidFile))
			{
				dprintf("Can't write pid.\n");
				exit(1); /* exit during startup - questionable */
			}
		}
		else
		{
			dprintf("Pidfile (and pid) already exist.\n");
			exit(1); /* exit during startup - questionable */
		}
	} /* if ( !Debug ) */
#endif
	myPid = getpid(); 	/* save our pid for further testing (also used for messages) */

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

	/* prepare emergency logging system */

	consfile.f_type = F_CONSOLE;
	strcpy(consfile.f_un.f_fname, ctty);
	cflineSetTemplateAndIOV(&consfile, " TradFmt");
	gethostname(LocalHostName, sizeof(LocalHostName));
	if ( (p = strchr(LocalHostName, '.')) ) {
		*p++ = '\0';
		LocalDomain = p;
	}
	else
	{
		LocalDomain = "";

		/*
		 * It's not clearly defined whether gethostname()
		 * should return the simple hostname or the fqdn. A
		 * good piece of software should be aware of both and
		 * we want to distribute good software.  Joey
		 *
		 * Good software also always checks its return values...
		 * If syslogd starts up before DNS is up & /etc/hosts
		 * doesn't have LocalHostName listed, gethostbyname will
		 * return NULL. 
		 */
		hent = gethostbyname(LocalHostName);
		if ( hent )
			snprintf(LocalHostName, sizeof(LocalHostName), "%s", hent->h_name);
			
		if ( (p = strchr(LocalHostName, '.')) )
		{
			*p++ = '\0';
			LocalDomain = p;
		}
	}

	/* Convert to lower case to recognize the correct domain laterly
	 */
	for (p = (char *)LocalDomain; *p ; p++)
		if (isupper(*p))
			*p = tolower(*p);

	(void) signal(SIGTERM, doDie);
	(void) signal(SIGINT, Debug ? doDie : SIG_IGN);
	(void) signal(SIGQUIT, Debug ? doDie : SIG_IGN);
	(void) signal(SIGCHLD, reapchild);
	(void) signal(SIGALRM, domarkAlarmHdlr);
	(void) signal(SIGUSR1, Debug ? debug_switch : SIG_IGN);
	(void) signal(SIGPIPE, SIG_IGN);
	(void) signal(SIGXFSZ, SIG_IGN); /* do not abort if 2gig file limit is hit */
	(void) alarm(TIMERINTVL);

	/* Create a partial message table for all file descriptors. */
	num_fds = getdtablesize();
	dprintf("Allocated parts table for %d file descriptors.\n", num_fds);
	if ((parts = (char **) malloc(num_fds * sizeof(char *))) == NULL)
	{
		logerror("Cannot allocate memory for message parts table.");
		die(0);
	}
	for(i= 0; i < num_fds; ++i)
	    parts[i] = NULL;

	dprintf("Starting.\n");
	init();
#ifndef TESTING
	if(Debug) {
		dprintf("Debugging enabled, SIGUSR1 to turn off debugging.\n");
		debugging_on = 1;
	}
	/*
	 * Send a signal to the parent to it can terminate.
	 */
	if (myPid != ppid)
		kill (ppid, SIGTERM);
#endif
	/* END OF INTIALIZATION
	 * ... but keep in mind that we might do a restart and thus init() might
	 * be called again. If that happens, we must shut down all active threads,
	 * do the init() and then restart things.
	 * rgerhards, 2005-10-24
	 */
#ifdef	USE_PTHREADS
	/* create message queue */
	pMsgQueue = queueInit();
	if(pMsgQueue == NULL) {
		errno = 0;
		logerror("error: could not create message queue - running single-threaded!\n");
	} else { /* start up worker thread */
		startWorker();
	}
#endif

	/* --------------------- Main loop begins here. ----------------------------------------- */
	mainloop();
	die(bFinished);
	return 0;
}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 * vi:set ai:
 */
