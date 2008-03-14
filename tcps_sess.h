/* Definitions for tcps_sess class. This implements a session of the
 * plain TCP server.
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
#ifndef INCLUDED_TCPS_SESS_H
#define INCLUDED_TCPS_SESS_H

#include "obj.h"

/* a forward-definition, we are somewhat cyclic */
struct tcpsrv_s;

/* framing modes for TCP */
typedef enum _TCPFRAMINGMODE {
		TCP_FRAMING_OCTET_STUFFING = 0, /* traditional LF-delimited */
		TCP_FRAMING_OCTET_COUNTING = 1  /* -transport-tls like octet count */
	} TCPFRAMINGMODE;

/* the tcps_sess object */
typedef struct tcps_sess_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	struct tcpsrv_s *pSrv;	/* pointer back to my server (e.g. for callbacks) */
	int sock;
	int iMsg; /* index of next char to store in msg */
	int bAtStrtOfFram;	/* are we at the very beginning of a new frame? */
	enum {
		eAtStrtFram,
		eInOctetCnt,
		eInMsg
	} inputState;		/* our current state */
	int iOctetsRemain;	/* Number of Octets remaining in message */
	TCPFRAMINGMODE eFraming;
	char msg[MAXLINE+1];
	char *fromHost;
	void *pUsr;	/* a user-pointer */
} tcps_sess_t;


/* interfaces */
BEGINinterface(tcps_sess) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(tcps_sess);
	rsRetVal (*Construct)(tcps_sess_t **ppThis);
	rsRetVal (*ConstructFinalize)(tcps_sess_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(tcps_sess_t **ppThis);
	rsRetVal (*PrepareClose)(tcps_sess_t *pThis);
	rsRetVal (*Close)(tcps_sess_t *pThis);
	rsRetVal (*DataRcvd)(tcps_sess_t *pThis, char *pData, size_t iLen);
	/* set methods */
	rsRetVal (*SetTcpsrv)(tcps_sess_t *pThis, struct tcpsrv_s *pSrv);
	rsRetVal (*SetUsrP)(tcps_sess_t*, void*);
	rsRetVal (*SetHost)(tcps_sess_t *pThis, uchar*);
	rsRetVal (*SetSock)(tcps_sess_t *pThis, int);
	rsRetVal (*SetMsgIdx)(tcps_sess_t *pThis, int);
ENDinterface(tcps_sess)
#define tcps_sessCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(tcps_sess);


#endif /* #ifndef INCLUDED_TCPS_SESS_H */
