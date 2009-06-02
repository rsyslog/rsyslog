/* Definitions for strms_sess class. This implements a session of the
 * generic stream server.
 *
 * Copyright 2008, 2009 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_STRMS_SESS_H
#define INCLUDED_STRMS_SESS_H

#include "obj.h"

/* a forward-definition, we are somewhat cyclic */
struct strmsrv_s;

/* the strms_sess object */
struct strms_sess_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	strmsrv_t *pSrv;	/* pointer back to my server (e.g. for callbacks) */
	strmLstnPortList_t *pLstnInfo;	/* pointer back to listener info */
	netstrm_t *pStrm;
//	uchar *pMsg;		/* message (fragment) received */
	uchar *fromHost;
	uchar *fromHostIP;
	void *pUsr;		/* a user-pointer */
};


/* interfaces */
BEGINinterface(strms_sess) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(strms_sess);
	rsRetVal (*Construct)(strms_sess_t **ppThis);
	rsRetVal (*ConstructFinalize)(strms_sess_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(strms_sess_t **ppThis);
	rsRetVal (*Close)(strms_sess_t *pThis);
	rsRetVal (*DataRcvd)(strms_sess_t *pThis, char *pData, size_t iLen);
	/* set methods */
	rsRetVal (*SetStrmsrv)(strms_sess_t *pThis, struct strmsrv_s *pSrv);
	rsRetVal (*SetLstnInfo)(strms_sess_t *pThis, strmLstnPortList_t *pLstnInfo);
	rsRetVal (*SetUsrP)(strms_sess_t*, void*);
	void*    (*GetUsrP)(strms_sess_t*);
	rsRetVal (*SetHost)(strms_sess_t *pThis, uchar*);
	rsRetVal (*SetHostIP)(strms_sess_t *pThis, uchar*);
	rsRetVal (*SetStrm)(strms_sess_t *pThis, netstrm_t*);
	rsRetVal (*SetOnMsgReceive)(strms_sess_t *pThis, rsRetVal (*OnMsgReceive)(strms_sess_t*, uchar*, int));
ENDinterface(strms_sess)
#define strms_sessCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */
/* interface changes
 * to version v2, rgerhards, 2009-05-22
 * - Data structures changed
 * - SetLstnInfo entry point added
 */


/* prototypes */
PROTOTYPEObj(strms_sess);


#endif /* #ifndef INCLUDED_STRMS_SESS_H */
