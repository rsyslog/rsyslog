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
	rsRetVal (*Ctl)(nspoll_t *pThis, int fd, int op, epoll_event_t *event);
	rsRetVal (*Wait)(nspoll_t *pThis, epoll_event_t *events, int maxevents, int timeout);
ENDinterface(nspoll)
#define nspollCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(nspoll);

/* the name of our library binary */
#define LM_NSPOLL_FILENAME LM_NETSTRMS_FILENAME

#endif /* #ifndef INCLUDED_NSPOLL_H */
