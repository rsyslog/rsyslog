/* syslogd-type.h
 * This file contains type defintions used by syslogd and its modules.
 * It is a required input for any module.
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
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
#ifndef	SYSLOGD_TYPES_INCLUDED
#define	SYSLOGD_TYPES_INCLUDED 1
#include "config.h"  /* make sure we have autoconf macros */

#include "stringbuf.h"
#include "net.h"
#include <sys/param.h>
#include <sys/syslog.h>
#include <utmp.h>	/* TOODO: this goes away when struct filed has been relieved of UT_NAMESIZE */

#define FALSE 0
#define TRUE 1

#ifdef UT_NAMESIZE
# define UNAMESZ	UT_NAMESIZE	/* length of a login name */
#else
# define UNAMESZ	8	/* length of a login name */
#endif
#define MAXUNAMES	20	/* maximum number of user names */
#define MAXFNAME	200	/* max file pathname length */

#ifdef	WITH_DB
#include "mysql/mysql.h" 
#define	_DB_MAXDBLEN	128	/* maximum number of db */
#define _DB_MAXUNAMELEN	128	/* maximum number of user name */
#define	_DB_MAXPWDLEN	128 	/* maximum number of user's pass */
#define _DB_DELAYTIMEONERROR	20	/* If an error occur we stop logging until
					   a delayed time is over */
#endif


/* we define features of the syslog code. This features can be used
 * to check if modules are compatible with them - and possible other
 * applications I do not yet envision. -- rgerhards, 2007-07-24
 */
typedef enum _syslogFeature {
	sFEATURERepeatedMsgReduction = 1
} syslogFeature;

/* we define our own facility and severities */
/* facility and severity codes */
typedef struct _syslogCode {
	char    *c_name;
	int     c_val;
} syslogCODE;

typedef enum _TCPFRAMINGMODE {
		TCP_FRAMING_OCTET_STUFFING = 0, /* traditional LF-delimited */
		TCP_FRAMING_OCTET_COUNTING = 1  /* -transport-tls like octet count */
	} TCPFRAMINGMODE;

/* The following structure is a dynafile name cache entry.
 */
struct s_dynaFileCacheEntry {
	uchar *pName;	/* name currently open, if dynamic name */
	short	fd;		/* name associated with file name in cache */
	time_t	lastUsed;	/* for LRU - last access */
};
typedef struct s_dynaFileCacheEntry dynaFileCacheEntry;

/* values for host comparisons specified with host selector blocks
 * (+host, -host). rgerhards 2005-10-18.
 */
enum _EHostnameCmpMode {
	HN_NO_COMP = 0, /* do not compare hostname */
	HN_COMP_MATCH = 1, /* hostname must match */
	HN_COMP_NOMATCH = 2 /* hostname must NOT match */
};
typedef enum _EHostnameCmpMode EHostnameCmpMode;

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


/* values for f_type in struct filed below*/
/* gone away #define F_UNUSED	0 */		/* unused entry */
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
 */
struct filed {
	struct	filed *f_next;		/* next in linked list */
	/*__attribute__((deprecated))*/ short	f_type; /* entry type, see below */
	short	f_file;			/* file descriptor */
	short	bEnabled;		/* is the related action enabled (1) or disabled (0)? */
	time_t	f_time;			/* time this was last written */
	/* filter properties */
	enum {
		FILTER_PRI = 0,		/* traditional PRI based filer */
		FILTER_PROP = 1		/* extended filter, property based */
	} f_filter_type;
	EHostnameCmpMode eHostnameCmpMode;
	rsCStrObj *pCSHostnameComp;/* hostname to check */
	rsCStrObj *pCSProgNameComp;	/* tag to check or NULL, if not to be checked */
	struct moduleInfo *pMod;			/* pointer to output module handling this selector */
	void	*pModData;		/* pointer to module data - contents is module-specific */
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
#if 1
	union {
		//char	f_uname[MAXUNAMES][UNAMESZ+1];
#ifdef	WITH_DB
		struct {
			MYSQL	*f_hmysql;		/* handle to MySQL */
			char	f_dbsrv[MAXHOSTNAMELEN+1];	/* IP or hostname of DB server*/ 
			char	f_dbname[_DB_MAXDBLEN+1];	/* DB name */
			char	f_dbuid[_DB_MAXUNAMELEN+1];	/* DB user */
			char	f_dbpwd[_DB_MAXPWDLEN+1];	/* DB user's password */
			time_t	f_timeResumeOnError;		/* 0 if no error is present,	
								   otherwise is it set to the
								   time a retrail should be attampt */
			int	f_iLastDBErrNo;			/* Last db error number. 0 = no error */
		} f_mysql;
#endif
		struct {
			char	f_hname[MAXHOSTNAMELEN+1];
			struct addrinfo *f_addr;
			int compressionLevel; /* 0 - no compression, else level for zlib */
			char *port;
			int protocol;
			TCPFRAMINGMODE tcp_framing;
#			define	FORW_UDP 0
#			define	FORW_TCP 1
			/* following fields for TCP-based delivery */
			enum TCPSendStatus {
				TCP_SEND_NOTCONNECTED = 0,
				TCP_SEND_CONNECTING = 1,
				TCP_SEND_READY = 2
			} status;
			char *savedMsg;
			int savedMsgLen; /* length of savedMsg in octets */
			time_t	ttSuspend;	/* time selector was suspended */
#			ifdef USE_PTHREADS
			pthread_mutex_t mtxTCPSend;
#			endif
		} f_forw;		/* forwarding address */
		struct {
			char	f_fname[MAXFNAME];/* file or template name (display only) */
			struct template *pTpl;	/* pointer to template object */
			char	bDynamicName;	/* 0 - static name, 1 - dynamic name (with properties) */
			int	fCreateMode;	/* file creation mode for open() */
			int	fDirCreateMode;	/* creation mode for mkdir() */
			int	bCreateDirs;	/* auto-create directories? */
			uid_t	fileUID;	/* IDs for creation */
			uid_t	dirUID;
			gid_t	fileGID;
			gid_t	dirGID;
			int	bFailOnChown;	/* fail creation if chown fails? */
			int	iCurrElt;	/* currently active cache element (-1 = none) */
			int	iCurrCacheSize;	/* currently cache size (1-based) */
			int	iDynaFileCacheSize; /* size of file handle cache */
			/* The cache is implemented as an array. An empty element is indicated
			 * by a NULL pointer. Memory is allocated as needed. The following
			 * pointer points to the overall structure.
			 */
			dynaFileCacheEntry **dynCache;
			off_t	f_sizeLimit;		/* file size limit, 0 = no limit */
			char	*f_sizeLimitCmd;	/* command to carry out when size limit is reached */
		} f_file;
	} f_un;
#endif
	int	f_ReduceRepeated;		/* reduce repeated lines 0 - no, 1 - yes */
	int	f_prevcount;			/* repetition cnt of prevline */
	int	f_repeatcount;			/* number of "repeated" msgs */
	int	f_flags;			/* store some additional flags */
	struct template *f_pTpl;		/* pointer to template to use */
	struct iovec *f_iov;			/* dyn allocated depinding on template */
	unsigned short *f_bMustBeFreed;		/* indicator, if iov_base must be freed to destruct */
	int	f_iIovUsed;			/* nbr of elements used in IOV */
	char	*f_psziov;			/* iov as string */
	int	f_iLenpsziov;			/* length of iov as string */
	struct msg* f_pMsg;			/* pointer to the message (this will
					         * replace the other vars with msg
						 * content later). This is preserved after
						 * the message has been processed - it is
						 * also used to detect duplicates. */
};
typedef struct filed selector_t;	/* new type name */


#ifdef SYSLOG_INET
struct AllowedSenders {
	struct NetAddr allowedSender; /* ip address allowed */
	uint8_t SignificantBits;      /* defines how many bits should be discarded (eqiv to mask) */
	struct AllowedSenders *pNext;
};
#endif

#endif /* #ifndef SYSLOGD_TYPES_INCLUDED */
/*
 * vi:set ai:
 */
