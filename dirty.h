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
int parseRFCSyslogMsg(msg_t *pMsg, int flags);
int parseLegacySyslogMsg(msg_t *pMsg, int flags);
rsRetVal diagGetMainMsgQSize(int *piSize); /* for imdiag */
char* getFIOPName(unsigned iFIOP);

/* Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 * TODO: move this to action object! Only action.c and syslogd.c use it.
 */
extern int bActExecWhenPrevSusp;
extern int iActExecOnceInterval;
extern int MarkInterval;
extern int repeatinterval[2];
extern int  bReduceRepeatMsgs;
#define	MAXREPEAT ((int)((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1))
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}
extern int bDropTrailingLF;
extern uchar cCCEscapeChar;
extern int  bEscapeCCOnRcv;
extern int  bEscapeTab;
extern int bSpaceLFOnRcv;
#ifdef USE_NETZIP
/* config param: minimum message size to try compression. The smaller
 * the message, the less likely is any compression gain. We check for
 * gain before we submit the message. But to do so we still need to
 * do the (costly) compress() call. The following setting sets a size
 * for which no call to compress() is done at all. This may result in
 * a few more bytes being transmited but better overall performance.
 * Note: I have not yet checked the minimum UDP packet size. It might be
 * that we do not save anything by compressing very small messages, because
 * UDP might need to pad ;)
 * rgerhards, 2006-11-30
 */
#define	MIN_SIZE_FOR_COMPRESS	60
#endif

#endif /* #ifndef DIRTY_H_INCLUDED */
