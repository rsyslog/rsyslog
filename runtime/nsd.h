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

/* nsd_t is actually obj_t (which is somewhat better than void* but in essence
 * much the same).
 */

/* interface */
BEGINinterface(nsd) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(nsd_t **ppThis);
	rsRetVal (*Destruct)(nsd_t **ppThis);
	rsRetVal (*Abort)(nsd_t *pThis);
	rsRetVal (*LstnInit)(nsd_t *pThis, unsigned char *pLstnPort);
	rsRetVal (*AcceptConnReq)(nsd_t **ppThis, int sock);
	rsRetVal (*Rcv)(nsd_t *pThis, uchar *pRcvBuf, ssize_t *pLenBuf);
	rsRetVal (*Send)(nsd_t *pThis, uchar *pBuf, ssize_t *pLenBuf);
	rsRetVal (*Connect)(nsd_t *pThis, int family, unsigned char *port, unsigned char *host);
ENDinterface(nsd)
#define nsdCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

#endif /* #ifndef INCLUDED_NSD_H */
