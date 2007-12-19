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

#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
#include <gssapi.h>
#endif

struct TCPSession {
	int sock;
	int iMsg; /* index of next char to store in msg */
	int bAtStrtOfFram;	/* are we at the very beginning of a new frame? */
	int iOctetsRemain;	/* Number of Octets remaining in message */
	TCPFRAMINGMODE eFraming;
	char msg[MAXLINE+1];
	char *fromHost;
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
	OM_uint32 gss_flags;
	gss_ctx_id_t gss_context;
	char allowedMethods;
#endif
};

/* static data */
extern int  *sockTCPLstn;
extern char *TCPLstnPort;
extern int bEnableTCP;
extern struct TCPSession *pTCPSessions;
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
extern char *gss_listen_service_name;

#define ALLOWEDMETHOD_GSS 2
#endif

#define ALLOWEDMETHOD_TCP 1

/* prototypes */
void deinit_tcp_listener(void);
int *create_tcp_socket(void);
int TCPSessGetNxtSess(int iCurr);
int TCPSessAccept(int fd);
void TCPSessPrepareClose(int iTCPSess);
void TCPSessClose(int iSess);
int TCPSessDataRcvd(int iTCPSess, char *pData, int iLen);
void configureTCPListen(char *cOptarg);
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
int TCPSessGSSInit(void);
int TCPSessGSSAccept(int fd);
int TCPSessGSSRecv(int fd, void *buf, size_t buf_len);
void TCPSessGSSClose(int sess);
void TCPSessGSSDeinit(void);
#endif

#endif /* #ifndef TCPSYSLOG_H_INCLUDED */
/*
 * vi:set ai:
 */
