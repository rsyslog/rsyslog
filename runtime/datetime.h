/* The datetime object. Contains time-related functions.
 *
 * Copyright 2008-2012 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_DATETIME_H
#define INCLUDED_DATETIME_H

/* TODO: define error codes */
#define NO_ERRCODE -1

/* the datetime object */
typedef struct datetime_s {
    	char dummy;
} datetime_t;


/* interfaces */
BEGINinterface(datetime) /* name must also be changed in ENDinterface macro! */
	void (*getCurrTime)(struct syslogTime *t, time_t *ttSeconds);
	rsRetVal (*ParseTIMESTAMP3339)(struct syslogTime *pTime, uchar** ppszTS, int*);
	rsRetVal (*ParseTIMESTAMP3164)(struct syslogTime *pTime, uchar** pszTS, int*);
	int (*formatTimestampToMySQL)(struct syslogTime *ts, char* pDst);
	int (*formatTimestampToPgSQL)(struct syslogTime *ts, char *pDst);
	int (*formatTimestamp3339)(struct syslogTime *ts, char* pBuf);
	int (*formatTimestamp3164)(struct syslogTime *ts, char* pBuf, int);
	int (*formatTimestampSecFrac)(struct syslogTime *ts, char* pBuf);
	/* v3, 2009-11-12 */
	time_t (*GetTime)(time_t *ttSeconds);
	/* v6, 2011-06-20 */
	void (*timeval2syslogTime)(struct timeval *tp, struct syslogTime *t);
	/* v7, 2012-03-29 */
	int (*formatTimestampUnix)(struct syslogTime *ts, char*pBuf);
	time_t (*syslogTime2time_t)(struct syslogTime *ts);
ENDinterface(datetime)
#define datetimeCURR_IF_VERSION 7 /* increment whenever you change the interface structure! */
/* interface changes:
 * 1 - initial version
 * 2 - not compatible to 1 - bugfix required ParseTIMESTAMP3164 to accept char ** as
 *     last parameter. Did not try to remain compatible as this is not something any
 *     third-party module should call. -- rgerhards, 2008.-09-12
 * 3 - taken by v5 branch!
 * 4 - formatTimestamp3164 takes a third int parameter
 * 5 - merge of versions 3 + 4 (2010-03-09)
 * 6 - see above
 */

/* prototypes */
PROTOTYPEObj(datetime);

#endif /* #ifndef INCLUDED_DATETIME_H */
