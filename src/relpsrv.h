/* The RELPSRV object.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#ifndef RELPSRV_H_INCLUDED
#define	RELPSRV_H_INCLUDED

#include "relp.h"
#include "tcp.h"

/* the RELPSRV object 
 * rgerhards, 2008-03-17
 */
struct relpSrv_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	unsigned char *pLstnPort;
	relpTcp_t *pTcp; /**< our tcp support object */
	size_t maxDataSize;  /**< maximum size of a DATA element */
	void *pUsr; /**< user pointer (passed back in to callback) */

	/* Status of commands as supported in this session. */
	relpCmdEnaState_t stateCmdSyslog;
};


/* macros for quick memeber access */
#define relpSrvGetNumLstnSocks(pThis) (relpTcpGetNumSocks((pThis)->pTcp))
#define relpSrvGetLstnSock(pThis, i)  (relpTcpGetLstnSock((pThis)->pTcp, i))

/* prototypes */
relpRetVal relpSrvConstruct(relpSrv_t **ppThis, relpEngine_t *pEngine);
relpRetVal relpSrvDestruct(relpSrv_t **ppThis);
relpRetVal relpSrvSetLstnPort(relpSrv_t *pThis, unsigned char *pLstnPort);
relpRetVal relpSrvSetUsrPtr(relpSrv_t *pThis, void *pUsr);
relpRetVal relpSrvRun(relpSrv_t *pThis);

#endif /* #ifndef RELPSRV_H_INCLUDED */
