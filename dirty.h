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

rsRetVal __attribute__((deprecated)) multiSubmitMsg(multi_submit_t *pMultiSub);
rsRetVal multiSubmitMsg2(multi_submit_t *pMultiSub); /* friends only! */
rsRetVal submitMsg2(msg_t *pMsg);
rsRetVal __attribute__((deprecated)) submitMsg(msg_t *pMsg);
rsRetVal multiSubmitFlush(multi_submit_t *pMultiSub);
rsRetVal logmsgInternal(int iErr, int pri, uchar *msg, int flags);
rsRetVal __attribute__((deprecated)) parseAndSubmitMessage(uchar *hname, uchar *hnameIP, uchar *msg, int len, int flags, flowControl_t flowCtlTypeu, prop_t *pInputName, struct syslogTime *stTime, time_t ttGenTime, ruleset_t *pRuleset);
rsRetVal diagGetMainMsgQSize(int *piSize); /* for imdiag */
rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct cnfparamvals *queueParams);

extern int MarkInterval;
extern qqueue_t *pMsgQueue;				/* the main message queue */
extern int iConfigVerify;				/* is this just a config verify run? */
extern int bHaveMainQueue;
#endif /* #ifndef DIRTY_H_INCLUDED */
