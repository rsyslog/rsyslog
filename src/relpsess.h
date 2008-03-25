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

#include <pthread.h>

#include "relpsrv.h"

/* unacked msgs linked list */
typedef struct replSessUnacked_s {
	struct replSessUnacked_s *pNext;
	struct replSessUnacked_s *pPrev;
	struct relpSendbuf_s *pSendbuf; /**< the unacked message */
} relpSessUnacked_t;

/* relp session state */
typedef enum relpSessState_e {
	eRelpSessState_DISCONNECTED = 0,  /* session is disconnected and must be destructed */
	eRelpSessState_PRE_INIT = 1,
	eRelpSessState_INIT_CMD_SENT = 2,
	eRelpSessState_INIT_RSP_RCVD = 3,
	eRelpSessState_READY_TO_SEND = 4,
	eRelpSessState_WINDOW_FULL = 5,
	eRelpSessState_CLOSE_CMD_SENT = 6, /* once we are in a close state, we can not send any  */
	eRelpSessState_CLOSE_RSP_RCVD = 7, /* other commands */
	eRelpSessState_BROKEN = 9   /**< something went wrong, session must be dropped */
} relpSessState_t;

typedef enum relpSessType_e {
	eRelpSess_Server = 0,
	eRelpSess_Client = 1
} relpSessType_t; /* what type of session are we? */


/* the RELPSESS object 
 * rgerhards, 2008-03-16
 */
struct relpSess_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpSessType_t sessType;	/**< session type: 0 - server, 1 - client */
	relpTcp_t *pTcp;	/**< our sockt to the remote peer */
	struct relpFrame_s *pCurrRcvFrame; /**< the current receive frame (a buffer) */
	relpTxnr_t txnr;	/**< next txnr expected when receiving or to be used when sending */
	size_t maxDataSize;  /**< maximum size of a DATA element (TODO: set after handshake on connect) */
	pthread_mutex_t mutSend; /**< mutex for send operation (make sure txnr is correct) */

	/* connection parameters */
	int protocolVersion; /* relp protocol version in use in this session */
	/* Status of commands as supported in this session. */
	relpCmdEnaState_t stateCmdSyslog;

	/* save the following for auto-reconnect case */
	int protFamily;
	unsigned char *srvPort;
	unsigned char *srvAddr;

	/* properties needed for server operation */
	relpSrv_t *pSrv;	/**< the server we belong to */
	struct relpSendq_s *pSendq; /**< our send queue */

	/* properties needed for client operation */
	int bAutoRetry;	/**< automatically try (once) to reestablish a broken session? */
	int sizeWindow;	/**< size of our app-level communications window */
	int timeout; /**< timeout after which session is to be considered broken */
	relpSessState_t sessState; /**< state of our session */
	/* linked list of frames with outstanding "rsp" */
	relpSessUnacked_t *pUnackedLstRoot;
	relpSessUnacked_t *pUnackedLstLast;
	int lenUnackedLst;
};

/* macros for quick memeber access */
#define relpSessGetSock(pThis)  (relpTcpGetSock((pThis)->pTcp))
#define relpSessGetSessState(pThis) ((pThis)->sessState)
#define relpSessSetSessState(pThis, state) ((pThis)->sessState = (state))

#include "relpframe.h" /* this needs to be done after relpSess_t is defined! */
#include "sendbuf.h"
#include "sendq.h"

/* prototypes */
relpRetVal relpSessConstruct(relpSess_t **ppThis, relpEngine_t *pEngine, relpSrv_t *pSrv);
relpRetVal relpSessDestruct(relpSess_t **ppThis);
relpRetVal relpSessAcceptAndConstruct(relpSess_t **ppThis, relpSrv_t *pSrv, int sock);
relpRetVal relpSessRcvData(relpSess_t *pThis);
relpRetVal relpSessSendFrame(relpSess_t *pThis, relpFrame_t *pFrame);
relpRetVal relpSessSendResponse(relpSess_t *pThis, relpTxnr_t txnr, unsigned char *pData, size_t lenData);
relpRetVal relpSessSndData(relpSess_t *pThis);
relpRetVal relpSessSendCommand(relpSess_t *pThis, unsigned char *pCmd, size_t lenCmd,
		    unsigned char *pData, size_t lenData, relpRetVal (*rspHdlr)(relpSess_t*,relpFrame_t*));
relpRetVal relpSessConnect(relpSess_t *pThis, int protFamily, unsigned char *port, unsigned char *host);
relpRetVal relpSessAddUnacked(relpSess_t *pThis, relpSendbuf_t *pSendbuf);
relpRetVal relpSessGetUnacked(relpSess_t *pThis, relpSendbuf_t **ppSendbuf, relpTxnr_t txnr);
relpRetVal relpSessTryReestablish(relpSess_t *pThis);
relpRetVal relpSessSetProtocolVersion(relpSess_t *pThis, int protocolVersion);
relpRetVal relpSessConstructOffers(relpSess_t *pThis, relpOffers_t **ppOffers);
relpRetVal relpSessSendSyslog(relpSess_t *pThis, unsigned char *pMsg, size_t lenMsg);
relpRetVal relpSessSetEnableCmd(relpSess_t *pThis, unsigned char *pszCmd, relpCmdEnaState_t stateCmd);

#endif /* #ifndef RELPSESS_H_INCLUDED */
