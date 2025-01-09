/* Definitions for the nspoll io activity waiter
 *
 * Copyright 2009-2025 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDED_NSPOLL_H
#define INCLUDED_NSPOLL_H

#include "netstrms.h"

/* limits - TODO: think if this shall be configurable */
#define NSPOLL_MAX_EVENTS_PER_WAIT 128

/* some operations to be portable when we do not have epoll() available */
#define NSDPOLL_ADD	1
#define NSDPOLL_DEL	2

/* and some mode specifiers for waiting on input/output */
#define NSDPOLL_IN	1	/* EPOLLIN */
#define NSDPOLL_OUT	2	/* EPOLLOUT */

/* the NSDPOLL_IN_LSTN is just a temporary work-around: it enables us to
 * use listeners in level-triggered mode while we migrate toward edge-triggered
 * mode. Once the refactoring is finish and everything works with edge triggered,
 * we can AND SHALL remove it! <-- TODO
 */
#define NSDPOLL_IN_LSTN	128	/* EPOLLIN for listeners! */


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
	rsRetVal (*Wait)(nspoll_t *pNsdpoll, int timeout, int *numEntries, tcpsrv_io_descr_t *workset[]);
	rsRetVal (*Ctl)(nspoll_t *pNsdpoll, tcpsrv_io_descr_t *ioDescr, int mode, int op);
	rsRetVal (*IsEPollSupported)(void); /* static method */
	/* v3 - 2013-09-17 by rgerhards */
	rsRetVal (*SetDrvrName)(nspoll_t *pThis, uchar *name);
ENDinterface(nspoll)
#define nspollCURR_IF_VERSION 4 /* increment whenever you change the interface structure! */
/* interface change in v2 is that wait supports multiple return objects */

/* prototypes */
PROTOTYPEObj(nspoll);

/* the name of our library binary */
#define LM_NSPOLL_FILENAME LM_NETSTRMS_FILENAME

#endif /* #ifndef INCLUDED_NSPOLL_H */
