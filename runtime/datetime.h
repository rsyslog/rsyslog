/* The datetime object. Contains time-related functions.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
	void (*getCurrTime)(struct syslogTime *t, time_t *ttSeconds);
	rsRetVal (*ParseTIMESTAMP3339)(struct syslogTime *pTime, char** ppszTS);
	rsRetVal (*ParseTIMESTAMP3164)(struct syslogTime *pTime, char** pszTS);
	int (*formatTimestampToMySQL)(struct syslogTime *ts, char* pDst, size_t iLenDst);
	int (*formatTimestampToPgSQL)(struct syslogTime *ts, char *pDst, size_t iLenDst);
	int (*formatTimestamp3339)(struct syslogTime *ts, char* pBuf, size_t iLenBuf);
	int (*formatTimestamp3164)(struct syslogTime *ts, char* pBuf, size_t iLenBuf);
	int (*formatTimestampSecFrac)(struct syslogTime *ts, char* pBuf, size_t iLenBuf);
ENDinterface(datetime)
#define datetimeCURR_IF_VERSION 2 /* increment whenever you change the interface structure! */
/* interface changes:
 * 1 - initial version
 * 2 - not compatible to 1 - bugfix required ParseTIMESTAMP3164 to accept char ** as
 *     last parameter. Did not try to remain compatible as this is not something any
 *     third-party module should call. -- rgerhards, 2008.-09-12
 */

/* prototypes */
PROTOTYPEObj(datetime);

#endif /* #ifndef INCLUDED_DATETIME_H */
