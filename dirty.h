/* This file is an aid to support non-modular object accesses
 * while we do not have fully modularized everything. Once this is
 * done, this file can (and should) be deleted. Presence of it
 * also somewhat indicates that the runtime library is not really
 * yet a runtime library, because it depends on some functionality
 * residing somewhere else.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef	DIRTY_H_INCLUDED
#define	DIRTY_H_INCLUDED 1

rsRetVal multiSubmitMsg(multi_submit_t *pMultiSub);
rsRetVal submitMsg(msg_t *pMsg);
rsRetVal logmsgInternal(int iErr, int pri, uchar *msg, int flags);
rsRetVal parseAndSubmitMessage(uchar *hname, uchar *hnameIP, uchar *msg, int len, int flags, flowControl_t flowCtlTypeu, prop_t *pInputName, struct syslogTime *stTime, time_t ttGenTime);
rsRetVal diagGetMainMsgQSize(int *piSize); /* for imdiag */
rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName);

/* Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 * TODO: move this to action object! Only action.c and syslogd.c use it.
 */
extern int MarkInterval;
extern int repeatinterval[2];
extern int  bReduceRepeatMsgs;
extern qqueue_t *pMsgQueue;				/* the main message queue */
#define	MAXREPEAT ((int)((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1))
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}
#endif /* #ifndef DIRTY_H_INCLUDED */
