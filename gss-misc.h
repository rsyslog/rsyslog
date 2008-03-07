/* Definitions for gssutil class. This implements a session of the
 * plain TCP server.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#ifndef	GSS_MISC_H_INCLUDED
#define	GSS_MISC_H_INCLUDED 1

#include <gssapi/gssapi.h>
#include "obj.h"

/* interfaces */
BEGINinterface(gssutil) /* name must also be changed in ENDinterface macro! */
	int (*recv_token)(int s, gss_buffer_t tok);
	int (*send_token)(int s, gss_buffer_t tok);
	void (*display_status)(char *m, OM_uint32 maj_stat, OM_uint32 min_stat);
	void (*display_ctx_flags)(OM_uint32 flags);
ENDinterface(gssutil)
#define gssutilCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(gssutil);

/* the name of our library binary */
#define LM_GSSUTIL_FILENAME "lmgssutil"

#endif /* #ifndef GSS_MISC_H_INCLUDED */
