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
	struct action_s *pAction;	/* pointer to the action descriptor */
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
