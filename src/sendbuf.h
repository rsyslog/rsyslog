/* The RELPSENDBUF object.
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
#ifndef RELPSENDBUF_H_INCLUDED
#define	RELPSENDBUF_H_INCLUDED

#include "relpsess.h"

/* the RELPSENDBUF object
 * rgerhards, 2008-03-16
 */
struct relpSendbuf_s {
	BEGIN_RELP_OBJ;
	relpSess_t *pSess; /**< the session this buffer belongs to */
	relpOctet_t *pData; /**< the buffer, as it can be put on the wire */
	relpTxnr_t txnr; /**< our txnr in native form */
	relpRetVal (*rspHdlr)(relpSess_t*, relpFrame_t*); /**< callback when response arrived */
	size_t lenData;
	size_t lenTxnr;	/**< length of txnr - actual send buffer starts at offset (9 - lenTxnr)! */
	size_t bufPtr; /**< multi-purpose, tracks sent octets when multi-send
	 	            send() calls are required and tracks consumption of response
			    buffer in a rsp frame. */
};


/* prototypes */
relpRetVal relpSendbufConstruct(relpSendbuf_t **ppThis, relpSess_t *pSess);
relpRetVal relpSendbufDestruct(relpSendbuf_t **ppThis);
relpRetVal relpSendbufSend(relpSendbuf_t *pThis, relpTcp_t *pTcp);
relpRetVal relpSendbufSendAll(relpSendbuf_t *pThis, relpSess_t *pSess, int bAddToUnacked);

#endif /* #ifndef RELPSENDBUF_H_INCLUDED */
