/* An implementation of the nsd interface for GnuTLS.
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

#ifndef INCLUDED_NSD_GTLS_H
#define INCLUDED_NSD_GTLS_H

#include "nsd.h"

typedef nsd_if_t nsd_gtls_if_t; /* we just *implement* this interface */

/* the nsd_gtls object */
struct nsd_gtls_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	nsd_t *pTcp;		/**< our aggregated nsd_ptcp data */
	int iMode;		/* 0 - plain tcp, 1 - TLS */
	gnutls_session sess;
	int bHaveSess;
};

/* interface is defined in nsd.h, we just implement it! */
#define nsd_gtlsCURR_IF_VERSION nsdCURR_IF_VERSION

/* prototypes */
PROTOTYPEObj(nsd_gtls);

/* the name of our library binary */
#define LM_NSD_GTLS_FILENAME "lmnsd_gtls"

#endif /* #ifndef INCLUDED_NSD_GTLS_H */
