/* An implementation of the nsd select interface for GnuTLS.
 *
 * Copyright (C) 2008-2012 Adiscon GmbH.
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

#ifndef INCLUDED_NSDSEL_GTLS_H
#define INCLUDED_NSDSEL_GTLS_H

#include "nsd.h"
typedef nsdsel_if_t nsdsel_gtls_if_t; /* we just *implement* this interface */

/* the nsdsel_gtls object */
struct nsdsel_gtls_s {
	BEGINobjInstance
		;		     /* Data to implement generic object - MUST be the first data element! */
		nsdsel_t *pTcp;      /* our aggregated ptcp sel handler (which does almost everything) */
		int iBufferRcvReady; /* number of descriptiors where no RD select is needed because we have data in buf */
};

/* interface is defined in nsd.h, we just implement it! */
#define nsdsel_gtlsCURR_IF_VERSION nsdCURR_IF_VERSION

/* prototypes */
PROTOTYPEObj(nsdsel_gtls);

#endif /* #ifndef INCLUDED_NSDSEL_GTLS_H */
