/* Definitions for strmsrv class.
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
#ifndef INCLUDED_STRMSRV_H
#define INCLUDED_STRMSRV_H

#include "obj.h"
#include "strms_sess.h"

/* list of strm listen ports */
struct strmLstnPortList_s {
	uchar *pszPort;			/**< the ports the listener shall listen on */
	uchar *pszInputName;		/**< value to be used as input name */
	strmsrv_t *pSrv;			/**< pointer to higher-level server instance */
	strmLstnPortList_t *pNext;	/**< next port or NULL */
};


/* the strmsrv object */
struct strmsrv_s {
	BEGINobjInstance;	/**< Data to implement generic object - MUST be the first data element! */
	int bUseKeepAlive;	/**< use socket layer KEEPALIVE handling? */
	netstrms_t *pNS;	/**< pointer to network stream subsystem */
	int iDrvrMode;		/**< mode of the stream driver to use */
	uchar *pszDrvrAuthMode;	/**< auth mode of the stream driver to use */
	uchar *pszInputName;	/**< value to be used as input name */
	permittedPeers_t *pPermPeers;/**< driver's permitted peers */
	int iLstnMax;		/**< max nbr of listeners currently supported */
	netstrm_t **ppLstn;	/**< our netstream listners */
	strmLstnPortList_t **ppLstnPort; /**< pointer to relevant listen port description */
	int iSessMax;		/**< max number of sessions supported */
	strmLstnPortList_t *pLstnPorts;	/**< head pointer for listen ports */
	int addtlFrameDelim;	/**< additional frame delimiter for plain STRM syslog framing (e.g. to handle NetScreen) */
	strms_sess_t **pSessions;/**< array of all of our sessions */
	void *pUsr;		/**< a user-settable pointer (provides extensibility for "derived classes")*/
	/* callbacks */
	int      (*pIsPermittedHost)(struct sockaddr *addr, char *fromHostFQDN, void*pUsrSrv, void*pUsrSess);
	rsRetVal (*pRcvData)(strms_sess_t*, char*, size_t, ssize_t *);
	rsRetVal (*OpenLstnSocks)(struct strmsrv_s*);
	rsRetVal (*pOnListenDeinit)(void*);
	rsRetVal (*OnDestruct)(void*);
	rsRetVal (*pOnRegularClose)(strms_sess_t *pSess);
	rsRetVal (*pOnErrClose)(strms_sess_t *pSess);
	/* session specific callbacks */
	rsRetVal (*pOnSessAccept)(strmsrv_t *, strms_sess_t*);
	rsRetVal (*OnSessConstructFinalize)(void*);
	rsRetVal (*pOnSessDestruct)(void*);
	rsRetVal (*OnCharRcvd)(strms_sess_t*, uchar);
};


/* interfaces */
BEGINinterface(strmsrv) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(strmsrv);
	rsRetVal (*Construct)(strmsrv_t **ppThis);
	rsRetVal (*ConstructFinalize)(strmsrv_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(strmsrv_t **ppThis);
	rsRetVal (*configureSTRMListen)(strmsrv_t*, uchar *pszPort);
	//rsRetVal (*SessAccept)(strmsrv_t *pThis, strmLstnPortList_t*, strms_sess_t **ppSess, netstrm_t *pStrm);
	rsRetVal (*create_strm_socket)(strmsrv_t *pThis);
	rsRetVal (*Run)(strmsrv_t *pThis);
	/* set methods */
	rsRetVal (*SetAddtlFrameDelim)(strmsrv_t*, int);
	rsRetVal (*SetInputName)(strmsrv_t*, uchar*);
	rsRetVal (*SetKeepAlive)(strmsrv_t*, int);
	rsRetVal (*SetUsrP)(strmsrv_t*, void*);
	rsRetVal (*SetCBIsPermittedHost)(strmsrv_t*, int (*) (struct sockaddr *addr, char*, void*, void*));
	rsRetVal (*SetCBOpenLstnSocks)(strmsrv_t *, rsRetVal (*)(strmsrv_t*));
	rsRetVal (*SetCBOnDestruct)(strmsrv_t*, rsRetVal (*) (void*));
	rsRetVal (*SetCBOnRegularClose)(strmsrv_t*, rsRetVal (*) (strms_sess_t*));
	rsRetVal (*SetCBOnErrClose)(strmsrv_t*, rsRetVal (*) (strms_sess_t*));
	rsRetVal (*SetDrvrMode)(strmsrv_t *pThis, int iMode);
	rsRetVal (*SetDrvrAuthMode)(strmsrv_t *pThis, uchar *pszMode);
	rsRetVal (*SetDrvrPermPeers)(strmsrv_t *pThis, permittedPeers_t*);
	/* session specifics */
	rsRetVal (*SetCBOnSessAccept)(strmsrv_t*, rsRetVal (*) (strmsrv_t*, strms_sess_t*));
	rsRetVal (*SetCBOnSessDestruct)(strmsrv_t*, rsRetVal (*) (void*));
	rsRetVal (*SetCBOnSessConstructFinalize)(strmsrv_t*, rsRetVal (*) (void*));
	rsRetVal (*SetSessMax)(strmsrv_t *pThis, int iMaxSess);
	rsRetVal (*SetOnCharRcvd)(strmsrv_t *pThis, rsRetVal (*OnMsgCharRcvd)(strms_sess_t*, uchar));
ENDinterface(strmsrv)
#define strmsrvCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */
/* change for v?:
 */


/* prototypes */
PROTOTYPEObj(strmsrv);

/* the name of our library binary */
#define LM_STRMSRV_FILENAME "lmstrmsrv"

#endif /* #ifndef INCLUDED_STRMSRV_H */
