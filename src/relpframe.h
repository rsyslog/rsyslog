/* The RELPFRAME object.
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
#ifndef RELPFRAME_H_INCLUDED
#define	RELPFRAME_H_INCLUDED


/* state the relpframe may have during reception of messages */
typedef enum relpFrameRcvStates_e {
	eRelpFrameRcvState_BEGIN_FRAME = 0,
	eRelpFrameRcvState_IN_TXNR = 1,
	eRelpFrameRcvState_IN_CMD = 2,
	eRelpFrameRcvState_IN_DATALEN = 3,
	eRelpFrameRcvState_IN_DATA = 4,
	eRelpFrameRcvState_IN_TRAILER = 5,
	eRelpFrameRcvState_FINISHED = 6	 /**< the frame is fully received and ready for processing */
} relpFrameRcvState_t;


/* the RELPFRAME object 
 * rgerhards, 2008-03-16
 */
struct relpFrame_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpFrameRcvState_t rcvState;
	size_t iRcv;		/**< a multi-purpose field index used during frame reception */
	relpTxnr_t txnr;	/**< the current transaction (sequence) number */
	relpOctet_t cmd[32+1];	/**< the current command (+1 for C string terminator) */
	size_t lenData;		/**< length of data part of frame */
	relpOctet_t *pData;	/**< frame data part */
	size_t idxData;		/**< keeps track of processed chars when reading pData bytewise */
};

#include "relpsess.h" /* this needs to be done after relpFrame_t is defined! */
#include "sendbuf.h"

/* prototypes */
relpRetVal relpFrameProcessOctetRcvd(relpFrame_t **ppThis, relpOctet_t c, relpSess_t *pSess);
relpRetVal relpFrameBuildSendbuf(relpSendbuf_t **ppSendbuf, relpTxnr_t txnr, unsigned char *pCmd, size_t lenCmd,
	   relpOctet_t *pData, size_t lenData, relpSess_t *pSess, relpRetVal (*rspHdlr)(relpSess_t*,relpFrame_t*));
relpRetVal relpFrameRewriteTxnr(relpSendbuf_t *pSendbuf, relpTxnr_t txnr);
relpRetVal relpFrameGetNextC(relpFrame_t *pThis, unsigned char *pC);


#endif /* #ifndef RELPFRAME_H_INCLUDED */
