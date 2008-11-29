/* The interface definition for "NetStream Drivers" (nsd).
 *
 * This is just an abstract driver interface, which needs to be
 * implemented by concrete classes. As such, no nsd data type itself
 * is defined.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_NSD_H
#define INCLUDED_NSD_H

#include <sys/socket.h>

enum nsdsel_waitOp_e {
	NSDSEL_RD = 1,
	NSDSEL_WR = 2,
	NSDSEL_RDWR = 3
}; /**< the operation we wait for */

/* nsd_t is actually obj_t (which is somewhat better than void* but in essence
 * much the same).
 */

/* interface */
BEGINinterface(nsd) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(nsd_t **ppThis);
	rsRetVal (*Destruct)(nsd_t **ppThis);
	rsRetVal (*Abort)(nsd_t *pThis);
	rsRetVal (*Rcv)(nsd_t *pThis, uchar *pRcvBuf, ssize_t *pLenBuf);
	rsRetVal (*Send)(nsd_t *pThis, uchar *pBuf, ssize_t *pLenBuf);
	rsRetVal (*Connect)(nsd_t *pThis, int family, unsigned char *port, unsigned char *host);
	rsRetVal (*LstnInit)(netstrms_t *pNS, void *pUsr, rsRetVal(*fAddLstn)(void*,netstrm_t*),
			     uchar *pLstnPort, uchar *pLstnIP, int iSessMax);
	rsRetVal (*AcceptConnReq)(nsd_t *pThis, nsd_t **ppThis);
	rsRetVal (*GetRemoteHName)(nsd_t *pThis, uchar **pszName);
	rsRetVal (*GetRemoteIP)(nsd_t *pThis, uchar **pszIP);
	rsRetVal (*SetMode)(nsd_t *pThis, int mode); /* sets a driver specific mode - see driver doc for details */
	rsRetVal (*SetAuthMode)(nsd_t *pThis, uchar*); /* sets a driver specific mode - see driver doc for details */
	rsRetVal (*SetPermPeers)(nsd_t *pThis, permittedPeers_t*); /* sets driver permitted peers for auth needs */
	void     (*CheckConnection)(nsd_t *pThis);	/* This is a trick mostly for plain tcp syslog */
	rsRetVal (*GetSock)(nsd_t *pThis, int *pSock);
	rsRetVal (*SetSock)(nsd_t *pThis, int sock);
	/* GetSock() and SetSock() return an error if the driver does not use plain
	 * OS sockets. This interface is primarily meant as an internal aid for
	 * those drivers that utilize the nsd_ptcp to do some of their work.
	 */
	rsRetVal (*GetRemAddr)(nsd_t *pThis, struct sockaddr_storage **ppAddr);
	/* getRemAddr() is an aid needed by the legacy ACL system. It exposes the remote
	 * peer's socket addr structure, so that the legacy matching functions can work on
	 * it. Note that this ties netstream drivers to things that can be implemented over
	 * sockets - not really desirable, but not the end of the world... TODO: should be
	 * reconsidered when a new ACL system is build. -- rgerhards, 2008-12-01
	 */
ENDinterface(nsd)
#define nsdCURR_IF_VERSION 4 /* increment whenever you change the interface structure! */
/* interface version 4 added GetRemAddr() */

/* interface  for the select call */
BEGINinterface(nsdsel) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(nsdsel_t **ppThis);
	rsRetVal (*Destruct)(nsdsel_t **ppThis);
	rsRetVal (*Add)(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp);
	rsRetVal (*Select)(nsdsel_t *pNsdsel, int *piNumReady);
	rsRetVal (*IsReady)(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp, int *pbIsReady);
ENDinterface(nsdsel)
#define nsdselCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

#endif /* #ifndef INCLUDED_NSD_H */
