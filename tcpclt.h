/* tcpclt.h
 *
 * This are the definitions for the TCP based clients class.
 *
 * File begun on 2007-07-21 by RGerhards (extracted from syslogd.c)
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
#ifndef	TCPCLT_H_INCLUDED
#define	TCPCLT_H_INCLUDED 1

#include "tcpsyslog.h"
#include "obj.h"

/* the tcpclt object */
typedef struct tcpclt_s {
	BEGINobjInstance;	/**< Data to implement generic object - MUST be the first data element! */
	TCPFRAMINGMODE tcp_framing;
	char *prevMsg;
	size_t lenPrevMsg;
	/* session specific callbacks */
	rsRetVal (*initFunc)(void*);
	rsRetVal (*sendFunc)(void*, char*, size_t);
	rsRetVal (*prepRetryFunc)(void*);
} tcpclt_t;


/* interfaces */
BEGINinterface(tcpclt) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(tcpclt_t **ppThis);
	rsRetVal (*ConstructFinalize)(tcpclt_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(tcpclt_t **ppThis);
	int (*Send)(tcpclt_t *pThis, void*pData, char*msg, size_t len);
	int (*CreateSocket)(struct addrinfo *addrDest);
	/* set methods */
	rsRetVal (*SetSendInit)(tcpclt_t*, rsRetVal (*)(void*));
	rsRetVal (*SetSendFrame)(tcpclt_t*, rsRetVal (*)(void*, char*, size_t));
	rsRetVal (*SetSendPrepRetry)(tcpclt_t*, rsRetVal (*)(void*));
	rsRetVal (*SetFraming)(tcpclt_t*, TCPFRAMINGMODE framing);
ENDinterface(tcpclt)
#define tcpcltCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(tcpclt);

/* the name of our library binary */
#define LM_TCPCLT_FILENAME "lmtcpclt"

#endif /* #ifndef TCPCLT_H_INCLUDED */
/* vim:set ai:
 */
