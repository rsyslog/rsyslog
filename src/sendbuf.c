/* The relp send buffer object.
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
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
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
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include "relp.h"
#include "sendbuf.h"


/** Construct a RELP sendbuf instance
 * This is the first thing that a caller must do before calling any
 * RELP function. The relp sendbuf must only destructed after all RELP
 * operations have been finished.
 */
relpRetVal
relpSendbufConstruct(relpSendbuf_t **ppThis, relpEngine_t *pEngine)
{
	relpSendbuf_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(relpSendbuf_t))) == NULL) {
		ABORT_FINALIZE(RELP_RET_OUT_OF_MEMORY);
	}

	RELP_CORE_CONSTRUCTOR(pThis, Sendbuf);
	pThis->pEngine = pEngine;

	*ppThis = pThis;

finalize_it:
	LEAVE_RELPFUNC;
}


/** Destruct a RELP sendbuf instance
 */
relpRetVal
relpSendbufDestruct(relpSendbuf_t **ppThis)
{
	relpSendbuf_t *pThis;

	ENTER_RELPFUNC;
	assert(ppThis != NULL);
	pThis = *ppThis;
	RELPOBJ_assert(pThis, Engine);

	/* TODO: check for pending operations -- rgerhards, 2008-03-16 */

	/* done with de-init work, now free sendbuf object itself */
	free(pThis);
	*ppThis = NULL;

	LEAVE_RELPFUNC;
}


/* set send buffer contents. The provided data pointer is handed *over* to
 * the sendbuf, so the caller can no longer access it. Most importantly, the
 * caller MUST NOT free the buffer!
 * rgerhards, 2008-03-17
 */
relpRetVal
relpSendbufSetData(relpSendbuf_t *pThis, relpOctet_t *pData, size_t lenData)
{
	ENTER_RELPFUNC;
	RELPOBJ_assert(pThis, Engine);
	assert(pData != NULL);
	assert(lenData > 0);

	pThis->pData = pData;
	pThis->lenData = lenData;
	pThis->bufPtr = 0;

	LEAVE_RELPFUNC;
}


/* try to send as much as possible from the send buffer
 * rgerhards, 20008-03-17
 */
// TODO: implement?
