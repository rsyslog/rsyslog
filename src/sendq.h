/* The RELPSENDQ object.
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
#ifndef RELPSENDQ_H_INCLUDED
#define	RELPSENDQ_H_INCLUDED

#include <pthread.h>
#include "sendbuf.h"

/* The relp sendq entry object.
 * This is a doubly-linked list
 * rgerhards, 2008-03-16
 */
typedef struct relpSendqe_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	struct relpSendqe_s *pNext;
	struct relpSendqe_s *pPrev;
	relpSendbuf_t *pBuf; /* our send buffer */
} relpSendqe_t;

/* the RELPSENDQ object 
 * This provides more or less just the root of the sendq entries.
 * rgerhards, 2008-03-16
 */
typedef struct relpSendq_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpSendqe_t *pRoot;
	relpSendqe_t *pLast;
	pthread_mutex_t mut;
} relpSendq_t;


/* prototypes */
relpRetVal relpSendqConstruct(relpSendq_t **ppThis, relpEngine_t *pEngine);
relpRetVal relpSendqDestruct(relpSendq_t **ppThis);
relpRetVal relpSendqAddBuf(relpSendq_t *pThis, relpSendbuf_t *pBuf, relpTcp_t *pTcp);
relpRetVal relpSendqSend(relpSendq_t *pThis, relpTcp_t *pTcp);
int relpSendqIsEmpty(relpSendq_t *pThis);

#endif /* #ifndef RELPSENDQ_H_INCLUDED */
