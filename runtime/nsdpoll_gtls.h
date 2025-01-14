/* An implementation of the nsd poll interface for Gnu TLS.
 *
 * Copyright 2025 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
