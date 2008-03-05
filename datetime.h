/* The datetime object. Contains time-related functions.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_DATETIME_H
#define INCLUDED_DATETIME_H

#include "datetime.h"

/* TODO: define error codes */
#define NO_ERRCODE -1

/* the datetime object */
typedef struct datetime_s {
} datetime_t;


/* interfaces */
BEGINinterface(datetime) /* name must also be changed in ENDinterface macro! */
	void (*getCurrTime)(struct syslogTime *t);
	//static int srSLMGParseInt32(char** ppsz);
	int (*ParseTIMESTAMP3339)(struct syslogTime *pTime, char** ppszTS);
	int (*ParseTIMESTAMP3164)(struct syslogTime *pTime, char* pszTS);
	int (*formatTimestampToMySQL)(struct syslogTime *ts, char* pDst, size_t iLenDst);
	int (*formatTimestampToPgSQL)(struct syslogTime *ts, char *pDst, size_t iLenDst);
	int (*formatTimestamp3339)(struct syslogTime *ts, char* pBuf, size_t iLenBuf);
	int (*formatTimestamp3164)(struct syslogTime *ts, char* pBuf, size_t iLenBuf);
ENDinterface(datetime)
#define datetimeCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(datetime);

#endif /* #ifndef INCLUDED_DATETIME_H */
