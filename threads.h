/* Definition of the threading support module.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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

#ifndef THREADS_H_INCLUDED
#define THREADS_H_INCLUDED


/* the thread object */
typedef struct thrdInfo {
	pthread_mutex_t *mutTermOK;	/* Is it ok to terminate that thread now? */
	int bIsActive;		/* Is thread running? */
	int bShallStop;		/* set to 1 if the thread should be stopped ? */
	rsRetVal (*pUsrThrdMain)(struct thrdInfo*); /* user thread main to be called in new thread */
	rsRetVal (*pAfterRun)(struct thrdInfo*);   /* cleanup function */
	pthread_t thrdID;
} thrdInfo_t;

/* prototypes */
rsRetVal thrdExit(void);
rsRetVal thrdInit(void);
rsRetVal thrdTerminate(thrdInfo_t *pThis);
rsRetVal thrdTerminateAll(void);
rsRetVal thrdCreate(rsRetVal (*thrdMain)(thrdInfo_t*), rsRetVal(*afterRun)(thrdInfo_t *));
rsRetVal thrdSleep(thrdInfo_t *pThis, int iSeconds, int iuSeconds);

/* macros (replace inline functions) */

#endif /* #ifndef THREADS_H_INCLUDED */
