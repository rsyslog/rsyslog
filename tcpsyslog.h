/* tcpsyslog.h
 * These are the definitions for TCP-based syslog.
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c)
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
#ifndef	TCPSYSLOG_H_INCLUDED
#define	TCPSYSLOG_H_INCLUDED 1

struct TCPSession {
	int sock;
	int iMsg; /* index of next char to store in msg */
	int bAtStrtOfFram;	/* are we at the very beginning of a new frame? */
	int iOctetsRemain;	/* Number of Octets remaining in message */
	TCPFRAMINGMODE eFraming;
	char msg[MAXLINE+1];
	char *fromHost;
};

/* static data */
extern int  *sockTCPLstn;
extern char *TCPLstnPort;
extern int bEnableTCP;
extern struct TCPSession *pTCPSessions;

/* prototypes */
void deinit_tcp_listener(void);
int *create_tcp_socket(void);
int TCPSessGetNxtSess(int iCurr);
void TCPSessAccept(int fd);
void TCPSessPrepareClose(int iTCPSess);
void TCPSessClose(int iSess);
int TCPSessDataRcvd(int iTCPSess, char *pData, int iLen);
void configureTCPListen(char *cOptarg);

#endif /* #ifndef TCPSYSLOG_H_INCLUDED */
/*
 * vi:set ai:
 */
