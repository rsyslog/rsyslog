/* The RELPSESS object.
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
#ifndef RELPSESS_H_INCLUDED
#define	RELPSESS_H_INCLUDED

#include "relpsrv.h"
#include "relpframe.h"
#include "sendq.h"
#include "sendbuf.h"

/* unacked msgs linked list */
typedef struct replSessUnacked_s {
	struct replSessUnacked_s *pNext;
	struct replSessUnacked_s *pPrev;
	relpTxnr_t txnr;	/**< txnr of unacked message */
	relpSendbuf_t pSendbuf; /**< the unacked message */
} relpSessUnacked_t;

/* relp session state */
typedef enum relpSessState_e {
	eRelpSessState_INVALID = 0,
	eRelpSessState_PRE_INIT = 1,
	eRelpSessState_PRE_GO = 2,
	eRelpSessState_RUNNING = 3,
	eRelpSessState_SHUTDOWN = 4
} relpSessState_t;


/* the RELPSESS object 
 * rgerhards, 2008-03-16
 */
typedef struct relpSess_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpSrv_t *pSrv;	/**< the server we belong to */
	relpTcp_t *pTcp;	/**< our sockt to the remote peer */
	relpTxnr_t nxtTxnr;	/**< next txnr to be used for commands */
	relpSessState_t sessState; /**< state of our session */
	relpSendq_t *pSendq; /**< our send queue */
} relpSess_t;

/* prototypes */
relpRetVal relpSessConstruct(relpSess_t **ppThis, relpEngine_t *pEngine, relpSrv_t *pSrv);
relpRetVal relpSessDestruct(relpSess_t **ppThis);
relpRetVal relpSessAcceptAndConstruct(relpSrv_t *pSrv, int sock);

#endif /* #ifndef RELPSESS_H_INCLUDED */
