/* cfsysline.c
 * Implementation of the configuration system line object.
 *
 * File begun on 2007-07-30 by RGerhards
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "rsyslog.h"
#include "cfsysline.h"


/* static data */
cslCmd_t *pCmdListRoot = NULL; /* The list of known configuration commands. */
cslCmd_t *pCmdListLast = NULL;

/* destructor for cslCmdHdlr
 */
rsRetVal cslchDestruct(cslCmdHdlr_t *pThis)
{
	assert(pThis != NULL);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmdHdlr
 */
rsRetVal cslchConstruct(cslCmdHdlr_t **ppThis)
{
	cslCmdHdlr_t *pThis;
	rsRetVal iRet = RS_RET_OK;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmdHdlr_t))) == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}

abort_it:
	*ppThis = pThis;
	return iRet;
}

/* set data members for this object
 */
rsRetVal cslchSetEntry(cslCmdHdlr_t *pThis, ecslCmdHdrlType eType, rsRetVal (*pHdlr)(), void *pData)
{
	assert(pThis != NULL);
	assert(eType != eCmdHdlrInvalid);

	pThis->eType = eType;
	pThis->cslCmdHdlr = pHdlr;
	pThis->pData = pData;

	return RS_RET_OK;
}


/* call the specified handler
 */
rsRetVal cslchCallHdlr(cslCmdHdlr_t *pThis, uchar **ppConfLine)
{
	assert(pThis != NULL);
	assert(ppConfLine != NULL);


	return RS_RET_OK;
}

/* ---------------------------------------------------------------------- *
 * now come the handlers for cslCmd_t
 * ---------------------------------------------------------------------- */

/* destructor for cslCmd
 */
rsRetVal cslcDestruct(cslCmd_t *pThis)
{
	cslCmdHdlr_t *pHdlr;
	cslCmdHdlr_t *pHdlrPrev;
	assert(pThis != NULL);

	for(pHdlr = pThis->pHdlrRoot ; pHdlr != NULL ; pHdlr = pHdlrPrev->pNext) {
		pHdlrPrev = pHdlr; /* else we get a segfault */
		cslchDestruct(pHdlr);
	}

	free(pThis->pszCmdString);
	free(pThis);
	
	return RS_RET_OK;
}


/* constructor for cslCmdHdlr
 */
rsRetVal cslcConstruct(cslCmd_t **ppThis)
{
	cslCmd_t *pThis;
	rsRetVal iRet = RS_RET_OK;

	assert(ppThis != NULL);
	if((pThis = calloc(1, sizeof(cslCmd_t))) == NULL) {
		iRet = RS_RET_OUT_OF_MEMORY;
		goto abort_it;
	}

abort_it:
	*ppThis = pThis;
	return iRet;
}
/*
 * vi:set ai:
 */
