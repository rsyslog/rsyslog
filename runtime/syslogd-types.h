/* syslogd-type.h
 * This file contains type defintions used by syslogd and its modules.
 * It is a required input for any module.
 *
 * File begun on 2007-07-13 by RGerhards (extracted from syslogd.c)
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	SYSLOGD_TYPES_INCLUDED
#define	SYSLOGD_TYPES_INCLUDED 1

#include "stringbuf.h"
#include <sys/param.h>
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

#define FALSE 0
#define TRUE 1

#ifdef UT_NAMESIZE
# define UNAMESZ	UT_NAMESIZE	/* length of a login name */
#else
# define UNAMESZ	8	/* length of a login name */
#endif
#define MAXUNAMES	20	/* maximum number of user names */
#define MAXFNAME	200	/* max file pathname length */

#define	_DB_MAXDBLEN	128	/* maximum number of db */
#define _DB_MAXUNAMELEN	128	/* maximum number of user name */
#define	_DB_MAXPWDLEN	128 	/* maximum number of user's pass */
#define _DB_DELAYTIMEONERROR	20	/* If an error occur we stop logging until
					   a delayed time is over */


/* we define features of the syslog code. This features can be used
 * to check if modules are compatible with them - and possible other
 * applications I do not yet envision. -- rgerhards, 2007-07-24
 */
typedef enum _syslogFeature {
	sFEATURERepeatedMsgReduction = 1,	/* for output modules */
	sFEATURENonCancelInputTermination = 2,	/* for input modules */
	sFEATUREAutomaticSanitazion = 3,	/* for parser modules */
	sFEATUREAutomaticPRIParsing = 4		/* for parser modules */
} syslogFeature;

/* we define our own facility and severities */
/* facility and severity codes */
typedef struct _syslogCode {
	char    *c_name;
	int     c_val;
} syslogCODE;

/* values for host comparisons specified with host selector blocks
 * (+host, -host). rgerhards 2005-10-18.
 */
enum _EHostnameCmpMode {
	HN_NO_COMP = 0, /* do not compare hostname */
	HN_COMP_MATCH = 1, /* hostname must match */
	HN_COMP_NOMATCH = 2 /* hostname must NOT match */
};
typedef enum _EHostnameCmpMode EHostnameCmpMode;

/* time type numerical values for structure below */
#define TIME_TYPE_UNINIT	0
#define TIME_TYPE_RFC3164	1
#define TIME_TYPE_RFC5424	2
/* rgerhards 2004-11-11: the following structure represents
 * a time as it is used in syslog.
 * rgerhards, 2009-06-23: packed structure for better cache performance
 * (but left ultimate decision about packing to compiler)
 */
struct syslogTime {
	intTiny timeType;	/* 0 - unitinialized , 1 - RFC 3164, 2 - syslog-protocol */
	intTiny month;
	intTiny day;
	intTiny hour; /* 24 hour clock */
	intTiny minute;
	intTiny second;
	intTiny secfracPrecision;
	intTiny OffsetMinute;	/* UTC offset in minutes */
	intTiny OffsetHour;	/* UTC offset in hours
				 * full UTC offset minutes = OffsetHours*60 + OffsetMinute. Then use
				 * OffsetMode to know the direction.
				 */
	char OffsetMode;	/* UTC offset + or - */
	short year;
	int secfrac;	/* fractional seconds (must be 32 bit!) */
};
typedef struct syslogTime syslogTime_t;

#endif /* #ifndef SYSLOGD_TYPES_INCLUDED */
/* vi:set ai:
 */
