/* Definition of the worker thread instance (wti) class.
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

#ifndef WTI_H_INCLUDED
#define WTI_H_INCLUDED

#include <pthread.h>
#include "wtp.h"
#include "obj.h"

/* the worker thread instance class */
typedef struct wti_s {
	BEGINobjInstance;
	pthread_t thrdID;  /* thread ID */
	qWrkCmd_t tCurrCmd; /* current command to be carried out by worker */
	obj_t *pUsr;		/* current user object being processed (or NULL if none) */
	wtp_t *pWtp; /* my worker thread pool (important if only the work thread instance is passed! */
	pthread_cond_t condInitDone; /* signaled when the thread startup is done (once per thread existance) */
	pthread_mutex_t mut;
	uchar *pszDbgHdr;	/* header string for debug messages */
} wti_t;

/* some symbolic constants for easier reference */


/* prototypes */
rsRetVal wtiConstruct(wti_t **ppThis);
rsRetVal wtiConstructFinalize(wti_t *pThis);
rsRetVal wtiDestruct(wti_t **ppThis);
rsRetVal wtiWorker(wti_t *pThis);
PROTOTYPEObjClassInit(wti);
PROTOTYPEpropSetMeth(wti, pszDbgHdr, uchar*);
#define wtiGetID(pThis) ((unsigned long) pThis)

#endif /* #ifndef WTI_H_INCLUDED */
