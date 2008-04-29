/* Definitions for the nssel IO waiter.
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

#ifndef INCLUDED_NSSEL_H
#define INCLUDED_NSSEL_H

#include "netstrms.h"

/* the nssel object */
struct nssel_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	nsd_t *pDrvrData;	/**< the driver's data elements */
	uchar *pBaseDrvrName;	/**< nsd base driver name to use, or NULL if system default */
	uchar *pDrvrName;	/**< full base driver name (set when driver is loaded) */
	nsdsel_if_t Drvr;	/**< our stream driver */
};


/* interface */
BEGINinterface(nssel) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*Construct)(nssel_t **ppThis);
	rsRetVal (*ConstructFinalize)(nssel_t *pThis);
	rsRetVal (*Destruct)(nssel_t **ppThis);
	rsRetVal (*Add)(nssel_t *pThis, netstrm_t *pStrm, nsdsel_waitOp_t waitOp);
	rsRetVal (*Wait)(nssel_t *pThis, int *pNumReady);
	rsRetVal (*IsReady)(nssel_t *pThis, netstrm_t *pStrm, nsdsel_waitOp_t waitOp, int *pbIsReady, int *piNumReady);
ENDinterface(nssel)
#define nsselCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(nssel);

/* the name of our library binary */
#define LM_NSSEL_FILENAME LM_NETSTRMS_FILENAME

#endif /* #ifndef INCLUDED_NSSEL_H */
