/* An implementation of the nsd poll interface for plain tcp sockets.
 *
 * Copyright 2009-2025 Rainer Gerhards and Adiscon GmbH.
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

#ifndef INCLUDED_NSDPOLL_PTCP_H
#define INCLUDED_NSDPOLL_PTCP_H

#include "nsd.h"
#ifdef HAVE_SYS_EPOLL_H
#	include <sys/epoll.h>
#endif
typedef nsdpoll_if_t nsdpoll_ptcp_if_t; /* we just *implement* this interface */

/* the nsdpoll_ptcp object */
struct nsdpoll_ptcp_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	int efd;		/* file descriptor used by epoll */
};

/* interface is defined in nsd.h, we just implement it! */
#define nsdpoll_ptcpCURR_IF_VERSION nsdCURR_IF_VERSION

/* prototypes */
PROTOTYPEObj(nsdpoll_ptcp);

#endif /* #ifndef INCLUDED_NSDPOLL_PTCP_H */
