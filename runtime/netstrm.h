/* Definitions for the stream-based netstrmworking class.
 *
 * Copyright 2007, 2008 Rainer Gerhards and Adiscon GmbH.
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

#ifndef INCLUDED_NETSTRM_H
#define INCLUDED_NETSTRM_H

#include "nsd.h" /* we need our driver interface to be defined */

/* the netstrm object */
struct netstrm_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	nsd_if_t Drvr;		/**< our stream driver */
	nsd_t *pDrvrData;	/**< the driver's data elements */
	uchar *pDrvrName;	/**< nsd driver name to use, or NULL if system default */
};


/* interface */
BEGINinterface(netstrm) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(netstrm_t **ppThis);
	rsRetVal (*ConstructFinalize)(netstrm_t *pThis);
	rsRetVal (*Destruct)(netstrm_t **ppThis);
	rsRetVal (*AbortDestruct)(netstrm_t **ppThis);
	rsRetVal (*LstnInit)(netstrm_t *pThis, unsigned char *pLstnPort);
	rsRetVal (*AcceptConnReq)(netstrm_t **ppThis, int sock);
	rsRetVal (*Rcv)(netstrm_t *pThis, uchar *pRcvBuf, ssize_t *pLenBuf);
	rsRetVal (*Send)(netstrm_t *pThis, uchar *pBuf, ssize_t *pLenBuf);
	rsRetVal (*Connect)(netstrm_t *pThis, int family, unsigned char *port, unsigned char *host);
ENDinterface(netstrm)
#define netstrmCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(netstrm);

/* the name of our library binary */
#define LM_NETSTRM_FILENAME "lmnetstrm"

#endif /* #ifndef INCLUDED_NETSTRM_H */
