/* Definitions for tcpsrv class.
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
#ifndef INCLUDED_TCPSRV_H
#define INCLUDED_TCPSRV_H

#include "obj.h"
#include "tcps_sess.h"

/* the tcpsrv object */
typedef struct tcpsrv_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	int *pSocksLstn;	/* listen socket array for server [0] holds count */
	int iSessMax;		/* max number of sessions supported */
	/* callbacks */
	int (*pIsPermittedHost)(struct sockaddr *addr, char *fromHostFQDN);
	int (*pRcvData)(tcps_sess_t*, char*, size_t);
	rsRetVal (*pOnListenDeinit)(void*);
	rsRetVal (*pOnSessAccept)(struct tcpsrv_s *, int fd);
	rsRetVal (*pOnRegularClose)(tcps_sess_t *pSess);
	rsRetVal (*pOnErrClose)(tcps_sess_t *pSess);
} tcpsrv_t;


/* interfaces */
BEGINinterface(tcpsrv) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(tcpsrv);
	rsRetVal (*Construct)(tcpsrv_t **ppThis);
	rsRetVal (*ConstructFinalize)(tcpsrv_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(tcpsrv_t **ppThis);
	void (*configureTCPListen)(tcpsrv_t*, char *cOptarg);
	//no longer needed? void (*configureTCPListenSessMax)(char *cOptarg);
	int (*SessAccept)(tcpsrv_t *pThis, int fd);
	rsRetVal (*Run)(tcpsrv_t *pThis);
	/* set methods */
	rsRetVal (*SetCBIsPermittedHost)(tcpsrv_t*, int (*) (struct sockaddr *addr, char*));
	rsRetVal (*SetCBRcvData)(tcpsrv_t *, int (*)(tcps_sess_t*, char*, size_t));
	rsRetVal (*SetCBOnListenDeinit)(tcpsrv_t*, rsRetVal (*)(void*));
	rsRetVal (*SetCBOnSessAccept)(tcpsrv_t*, rsRetVal (*) (tcpsrv_t*,int));
	rsRetVal (*SetCBOnRegularClose)(tcpsrv_t*, rsRetVal (*) (tcps_sess_t*));
	rsRetVal (*SetCBOnErrClose)(tcpsrv_t*, rsRetVal (*) (tcps_sess_t*));
ENDinterface(tcpsrv)
#define tcpsrvCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(tcpsrv);


#endif /* #ifndef INCLUDED_TCPSRV_H */
