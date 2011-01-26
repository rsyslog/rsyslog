/* Definitions for the nspoll io activity waiter
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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

#ifndef INCLUDED_NSPOLL_H
#define INCLUDED_NSPOLL_H

#include "netstrms.h"

/* some operations to be portable when we do not have epoll() available */
#define NSDPOLL_ADD	1
#define NSDPOLL_DEL	2

/* and some mode specifiers for waiting on input/output */
#define NSDPOLL_IN	1	/* EPOLLIN */
#define NSDPOLL_OUT	2	/* EPOLLOUT */
/* next is 4, 8, 16, ... - must be bit values, as they are ored! */

/* the nspoll object */
struct nspoll_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	nsd_t *pDrvrData;	/**< the driver's data elements */
	uchar *pBaseDrvrName;	/**< nsd base driver name to use, or NULL if system default */
	uchar *pDrvrName;	/**< full base driver name (set when driver is loaded) */
	nsdpoll_if_t Drvr;	/**< our stream driver */
};


/* interface */
BEGINinterface(nspoll) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(nspoll_t **ppThis);
	rsRetVal (*ConstructFinalize)(nspoll_t *pThis);
	rsRetVal (*Destruct)(nspoll_t **ppThis);
	rsRetVal (*Wait)(nspoll_t *pNsdpoll, int timeout, int *numEntries, nsd_epworkset_t workset[]);
	rsRetVal (*Ctl)(nspoll_t *pNsdpoll, netstrm_t *pStrm, int id, void *pUsr, int mode, int op);
	rsRetVal (*IsEPollSupported)(void); /* static method */
ENDinterface(nspoll)
#define nspollCURR_IF_VERSION 2 /* increment whenever you change the interface structure! */
/* interface change in v2 is that wait supports multiple return objects */

/* prototypes */
PROTOTYPEObj(nspoll);

/* the name of our library binary */
#define LM_NSPOLL_FILENAME LM_NETSTRMS_FILENAME

#endif /* #ifndef INCLUDED_NSPOLL_H */
